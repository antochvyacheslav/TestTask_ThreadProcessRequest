#include <memory>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

class Request {};

// возвращает nullptr если нужно завершить процесс, либо указатель на память, которую в дальнейшем требуется удалить
Request* GetRequest() 
{
    constexpr auto TestRequestCount = 500;
    static int i = 0;
    return (i++ < TestRequestCount ? new Request : nullptr);
};

// обрабатывает запрос, но память не удаляет
void ProcessRequest(Request* request) { std::this_thread::sleep_for(std::chrono::milliseconds(9)); };

constexpr int NumberOfThreads = 2;

int main()
{
    std::queue<std::unique_ptr<Request>> requests;
    std::mutex requestsMutex;
    std::condition_variable cv;

    auto threadProcessRequest = [&](std::stop_token stoken)
	{
        while (!stoken.stop_requested())
        {
            std::unique_ptr<Request> request;
            {
                std::unique_lock locker(requestsMutex);
                cv.wait(locker, [&requests, &stoken] { return (!requests.empty() || stoken.stop_requested()); });
                if (!requests.empty())
                {
                    request = std::move(requests.front());
                    requests.pop();
                }
            }
            if (request)
            {
                ProcessRequest(request.get());
            }
        }
	};

    std::vector<std::jthread> pool;
    pool.reserve(NumberOfThreads);

    for (int i = 0; i < NumberOfThreads; ++i)
    {
        pool.emplace_back(threadProcessRequest);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (;;)
    {
        std::unique_ptr<Request> request(GetRequest());
        if (request.get())
        {
            {
                std::lock_guard locker(requestsMutex);
                requests.push(std::move(request));
            }
            cv.notify_one();
        }
        else
        {
            break;
        }
    }

    while (!requests.empty()) 
    { 
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    for (auto& x : pool)
    {
        x.request_stop();
    }
    cv.notify_all();

    return 0;
}
