// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/background_downloader_mac.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

using net::test_server::BasicHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::HttpResponseDelegate;
using net::test_server::HungResponse;

namespace update_client {
namespace {
constexpr char kSmallDownloadData[] = "Hello, World!";
constexpr char kDownloadUrlSwitchName[] = "download-url";
constexpr char kDownloadSessionIdSwitchName[] = "download-session-id";

// Returns the lower range from a range header value.
int ParseRangeHeader(const std::string& header) {
  int lower_range = 0;
  // TODO(crbug.com/40285933): Don't use sscanf.
  EXPECT_EQ(std::sscanf(header.c_str(), "bytes=%d-", &lower_range), 1);
  return lower_range;
}

const std::string GetLargeDownloadData() {
  return std::string(10000, 'A');
}

}  // namespace

class BackgroundDownloaderTest : public testing::Test {
 public:
  void SetUp() override {
    environment_ = CreateTaskEnvironment();
    background_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
        kTaskTraitsBackgroundDownloader);

    shared_session_ = MakeBackgroundDownloaderSharedSession(
        background_sequence_, download_cache_,
        base::UnguessableToken::Create().ToString());

    downloader_ = base::MakeRefCounted<BackgroundDownloader>(
        nullptr, shared_session_, background_sequence_);

    test_server_ = std::make_unique<EmbeddedTestServer>();
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &BackgroundDownloaderTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));
  }

  void TearDown() override {
    background_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundDownloaderSharedSession::InvalidateAndCancel,
                       shared_session_));
  }

 protected:
  virtual std::unique_ptr<base::test::TaskEnvironment> CreateTaskEnvironment() {
    return std::make_unique<base::test::TaskEnvironment>();
  }

  void DoStartDownload(
      const GURL& url,
      BackgroundDownloaderSharedSession::OnDownloadCompleteCallback
          on_download_complete_callback) {
    downloader_->DoStartDownload(url, on_download_complete_callback);
  }

  void ExpectSmallDownloadContents(const base::FilePath& location) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(location, &contents));
    EXPECT_EQ(contents, kSmallDownloadData);
  }

  void ExpectLargeDownloadContents(const base::FilePath& location) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(location, &contents));
    EXPECT_EQ(contents, GetLargeDownloadData());
  }

  void ExpectDownloadMetrics(const CrxDownloader::DownloadMetrics& metrics,
                             int expected_error,
                             int expected_extra_code1,
                             int64_t expected_downloaded_bytes,
                             int64_t expected_total_bytes,
                             bool expect_nonzero_download_time) {
    EXPECT_EQ(metrics.url, GetURL());
    EXPECT_EQ(metrics.downloader,
              CrxDownloader::DownloadMetrics::kBackgroundMac);
    EXPECT_EQ(metrics.error, expected_error);
    EXPECT_EQ(metrics.extra_code1, expected_extra_code1);
    EXPECT_EQ(metrics.downloaded_bytes, expected_downloaded_bytes);
    EXPECT_EQ(metrics.total_bytes, expected_total_bytes);
    if (expect_nonzero_download_time) {
      EXPECT_NE(metrics.download_time_ms, 0U);
    } else {
      EXPECT_EQ(metrics.download_time_ms, 0U);
    }
  }

  GURL GetURL(const std::string& file = "") {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string path =
        base::StrCat({test_info->test_suite_name(), "/", file});
    GURL::Replacements replacements;
    replacements.SetPathStr(path);
    return test_server_->base_url().ReplaceComponents(replacements);
  }

  const base::FilePath download_cache_ =
      base::CreateUniqueTempDirectoryScopedToTest();
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;
  scoped_refptr<BackgroundDownloaderSharedSession> shared_session_;
  scoped_refptr<BackgroundDownloader> downloader_;
  base::RepeatingCallback<std::unique_ptr<HttpResponse>(
      const HttpRequest& request)>
      request_handler_;
  std::unique_ptr<base::test::TaskEnvironment> environment_;

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    CHECK(request_handler_) << "Request handler not configured for test";
    return request_handler_.Run(request);
  }

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  EmbeddedTestServerHandle test_server_handle_;
};

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderTest, DISABLED_SimpleDownload) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest&) {
    std::unique_ptr<BasicHttpResponse> response =
        std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(kSmallDownloadData);
    response->set_content_type("text/plain");
    return base::WrapUnique<HttpResponse>(response.release());
  });

  base::RunLoop run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [&](bool is_handled, const CrxDownloader::Result& result,
                          const CrxDownloader::DownloadMetrics& metrics) {
                        EXPECT_TRUE(is_handled);
                        EXPECT_EQ(result.error, 0);
                        ExpectSmallDownloadContents(result.response);
                        ExpectDownloadMetrics(
                            metrics, static_cast<int>(CrxDownloaderError::NONE),
                            0, std::strlen(kSmallDownloadData),
                            std::strlen(kSmallDownloadData), true);
                      })
                      .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderTest, DISABLED_DownloadDiscoveredInCache) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest&) {
    EXPECT_TRUE(false) << "The download server was expected to not be reached.";
    return base::WrapUnique<HttpResponse>(nullptr);
  });

  // Place a download in the cache.
  ASSERT_TRUE(base::CreateDirectory(download_cache_));
  uint32_t url_hash = base::PersistentHash(GetURL().spec());
  base::FilePath cached_download_path = download_cache_.AppendASCII(
      base::HexEncode(reinterpret_cast<uint8_t*>(&url_hash), sizeof(url_hash)));
  ASSERT_TRUE(base::WriteFile(cached_download_path, kSmallDownloadData));

  base::RunLoop run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [&](bool is_handled, const CrxDownloader::Result& result,
                          const CrxDownloader::DownloadMetrics& metrics) {
                        EXPECT_TRUE(is_handled);
                        EXPECT_EQ(result.error, 0);
                        ExpectSmallDownloadContents(result.response);
                        ExpectDownloadMetrics(
                            metrics, static_cast<int>(CrxDownloaderError::NONE),
                            0, std::strlen(kSmallDownloadData),
                            std::strlen(kSmallDownloadData), false);
                      })
                      .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

// Sends headers and the first half of the response before invoking `on_reply`
// and hanging up.
class InterruptedHttpResponse : public HttpResponse {
 public:
  explicit InterruptedHttpResponse(base::RepeatingClosure on_reply)
      : on_reply_(on_reply) {}

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override {
    std::string data = GetLargeDownloadData();
    base::StringPairs headers;
    headers.emplace_back("ETag", "42");
    headers.emplace_back("Accept-Ranges", "bytes");
    headers.emplace_back("Content-Length", base::NumberToString(data.size()));
    delegate->SendResponseHeaders(
        net::HTTP_OK, net::GetHttpReasonPhrase(net::HTTP_OK), headers);
    delegate->SendContents(std::string(data, data.size() / 2));
    on_reply_.Run();
    delegate->FinishResponse();
  }

 private:
  base::RepeatingClosure on_reply_;
};

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
// Tests that the download can resume after the server unexpectedly disconnects.
TEST_F(BackgroundDownloaderTest, DISABLED_ServerHangup) {
  const std::string data = GetLargeDownloadData();
  // If the request contains a range request, serve the content as requested.
  // Otherwise, send the first half of the data before hanging up.
  request_handler_ =
      base::BindLambdaForTesting([&](const HttpRequest& request) {
        if (request.headers.contains("Range")) {
          EXPECT_EQ(request.headers.at("If-Range"), "42");
          int lower_range = ParseRangeHeader(request.headers.at("Range"));

          std::unique_ptr<BasicHttpResponse> response =
              std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_PARTIAL_CONTENT);
          response->AddCustomHeader(
              "Content-Range",
              base::StringPrintf("bytes %d-%zu/%zu", lower_range, data.size(),
                                 data.size()));
          response->set_content(data.substr(lower_range));
          return base::WrapUnique<HttpResponse>(response.release());
        } else {
          return base::WrapUnique<HttpResponse>(
              new InterruptedHttpResponse(base::DoNothing()));
        }
      });

  base::RunLoop run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [&](bool is_handled, const CrxDownloader::Result& result,
                          const CrxDownloader::DownloadMetrics& metrics) {
                        EXPECT_TRUE(is_handled);
                        EXPECT_EQ(result.error, 0);
                        ExpectLargeDownloadContents(result.response);
                        ExpectDownloadMetrics(
                            metrics, static_cast<int>(CrxDownloaderError::NONE),
                            0, data.size(), data.size(), true);
                      })
                      .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderTest, DISABLED_DuplicateDownload) {
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop second_download_run_loop;
  request_handler_ = base::BindLambdaForTesting([&](const HttpRequest&) {
    current_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          DoStartDownload(
              GetURL(),
              base::BindLambdaForTesting(
                  [&](bool is_handled, const CrxDownloader::Result& result,
                      const CrxDownloader::DownloadMetrics& metrics) {
                    EXPECT_FALSE(is_handled);
                    EXPECT_EQ(
                        result.error,
                        static_cast<int>(
                            CrxDownloaderError::MAC_BG_DUPLICATE_DOWNLOAD));
                    ExpectDownloadMetrics(
                        metrics,
                        static_cast<int>(
                            CrxDownloaderError::MAC_BG_DUPLICATE_DOWNLOAD),
                        0, -1, -1, false);
                  })
                  .Then(second_download_run_loop.QuitClosure()));
        }));
    return base::WrapUnique<HttpResponse>(new HungResponse());
  });

  base::RunLoop first_download_run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [&](bool is_handled, const CrxDownloader::Result& result,
                          const CrxDownloader::DownloadMetrics& metrics) {
                        first_download_run_loop.Quit();
                      }));
  second_download_run_loop.Run();
  shared_session_->InvalidateAndCancel();
  first_download_run_loop.Run();
}

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
// Tests that downloads can complete when using multiple instances of
// BackgroundDownloader.
TEST_F(BackgroundDownloaderTest, DISABLED_ConcurrentDownloaders) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest&) {
    std::unique_ptr<BasicHttpResponse> response =
        std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(kSmallDownloadData);
    response->set_content_type("text/plain");
    return base::WrapUnique<HttpResponse>(response.release());
  });

  scoped_refptr<BackgroundDownloader> other_downloader =
      base::MakeRefCounted<BackgroundDownloader>(nullptr, shared_session_,
                                                 background_sequence_);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  DoStartDownload(GetURL("file1"),
                  base::BindLambdaForTesting(
                      [&](bool is_handled, const CrxDownloader::Result& result,
                          const CrxDownloader::DownloadMetrics& metrics) {
                        EXPECT_TRUE(is_handled);
                        EXPECT_EQ(result.error, 0);
                        ExpectSmallDownloadContents(result.response);
                      })
                      .Then(base::BindPostTaskToCurrentDefault(barrier_closure,
                                                               FROM_HERE)));
  other_downloader->DoStartDownload(
      GetURL("file2"),
      base::BindLambdaForTesting([&](bool is_handled,
                                     const CrxDownloader::Result& result,
                                     const CrxDownloader::DownloadMetrics&
                                         metrics) {
        EXPECT_TRUE(is_handled);
        EXPECT_EQ(result.error, 0);
        ExpectSmallDownloadContents(result.response);
      }).Then(base::BindPostTaskToCurrentDefault(barrier_closure, FROM_HERE)));

  run_loop.Run();
}

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderTest, DISABLED_MaxDownloads) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest& request) {
    return base::WrapUnique<HttpResponse>(new HungResponse());
  });

  base::RunLoop all_downloads_complete_run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(10, all_downloads_complete_run_loop.QuitClosure());
  for (int i = 0; i < 10; i++) {
    DoStartDownload(
        GetURL(base::NumberToString(i)),
        base::BindLambdaForTesting(
            [](bool is_handled, const CrxDownloader::Result& result,
               const CrxDownloader::DownloadMetrics& metrics) {})
            .Then(base::BindPostTaskToCurrentDefault(barrier_closure)));
  }

  base::RunLoop run_loop;
  // Mac may decide to not make progress on all of the downloads created
  // immediately. Thus, we have no way of observing when all of the download
  // tasks above have been created. Delaying the request for the final download
  // is an alternative to adding intrusive instrumentation to the
  // implementation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        DoStartDownload(
            GetURL(),
            base::BindLambdaForTesting(
                [&](bool is_handled, const CrxDownloader::Result& result,
                    const CrxDownloader::DownloadMetrics& metrics) {
                  EXPECT_FALSE(is_handled);
                  EXPECT_EQ(
                      result.error,
                      static_cast<int>(
                          CrxDownloaderError::MAC_BG_SESSION_TOO_MANY_TASKS));
                  ExpectDownloadMetrics(
                      metrics,
                      static_cast<int>(
                          CrxDownloaderError::MAC_BG_SESSION_TOO_MANY_TASKS),
                      0, -1, -1, false);
                })
                .Then(run_loop.QuitClosure()));
      }),
      base::Milliseconds(100));
  run_loop.Run();

  shared_session_->InvalidateAndCancel();
  all_downloads_complete_run_loop.Run();
}

class BackgroundDownloaderPeriodicTasksTest : public BackgroundDownloaderTest {
 public:
  std::unique_ptr<base::test::TaskEnvironment> CreateTaskEnvironment()
      override {
    // Configure the task environment to use mocked time that starts at the
    // current real time. This is important for tests which rely on cached file
    // ages, which are irrespective of mocked time.
    base::Time now = base::Time::NowFromSystemTime();
    auto environment = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    environment->AdvanceClock(now - base::Time::Now());
    return environment;
  }
};

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderPeriodicTasksTest, DISABLED_CleansStaleDownloads) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest&) {
    std::unique_ptr<BasicHttpResponse> response =
        std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(kSmallDownloadData);
    response->set_content_type("text/plain");
    return base::WrapUnique<HttpResponse>(response.release());
  });

  ASSERT_TRUE(base::WriteFile(download_cache_.AppendASCII("file1"),
                              kSmallDownloadData));
  ASSERT_TRUE(base::WriteFile(download_cache_.AppendASCII("file2"),
                              kSmallDownloadData));
  environment_->FastForwardBy(base::Days(3));

  base::RunLoop run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [](bool is_handled, const CrxDownloader::Result& result,
                         const CrxDownloader::DownloadMetrics& metrics) {})
                      .Then(run_loop.QuitClosure()));
  run_loop.Run();

  environment_->FastForwardBy(base::Minutes(30));
  environment_->RunUntilIdle();

  EXPECT_FALSE(base::PathExists(download_cache_.AppendASCII("file1")));
  EXPECT_FALSE(base::PathExists(download_cache_.AppendASCII("file2")));
}

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
TEST_F(BackgroundDownloaderPeriodicTasksTest,
       DISABLED_CancelsTasksWithNoProgress) {
  request_handler_ = base::BindLambdaForTesting([](const HttpRequest&) {
    return base::WrapUnique<HttpResponse>(new HungResponse());
  });

  base::RunLoop run_loop;
  DoStartDownload(GetURL(),
                  base::BindLambdaForTesting(
                      [](bool is_handled, const CrxDownloader::Result& result,
                         const CrxDownloader::DownloadMetrics& metrics) {
                        EXPECT_EQ(result.error, -999 /* NSURLErrorCancelled */);
                      })
                      .Then(run_loop.QuitClosure()));
  environment_->FastForwardBy(base::Minutes(30));
  run_loop.Run();
}

class BackgroundDownloaderCrashingClientTest : public testing::Test {
 public:
  void SetUp() override {
    background_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
        kTaskTraitsBackgroundDownloader);

    test_server_ = std::make_unique<EmbeddedTestServer>();
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &BackgroundDownloaderCrashingClientTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));
  }

  static void DoStartDownload(
      scoped_refptr<BackgroundDownloader> downloader,
      const GURL& url,
      base::RepeatingCallback<void(bool,
                                   const CrxDownloader::Result&,
                                   const CrxDownloader::DownloadMetrics&)>
          on_download_complete_callback) {
    downloader->DoStartDownload(url, on_download_complete_callback);
  }

 protected:
  GURL GetURL() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    GURL::Replacements replacements;
    replacements.SetPathStr(test_info->test_suite_name());
    return test_server_->base_url().ReplaceComponents(replacements);
  }

  base::RepeatingCallback<std::unique_ptr<HttpResponse>(
      const HttpRequest& request)>
      request_handler_;

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    CHECK(request_handler_) << "Request handler not configured for test";
    return request_handler_.Run(request);
  }

  base::test::TaskEnvironment environment_;
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  EmbeddedTestServerHandle test_server_handle_;
};

// TODO(crbug.com/40939899): Disabled due to excessive flakiness.
// Test that the download can be recovered after the client process crashes.
TEST_F(BackgroundDownloaderCrashingClientTest, DISABLED_ClientCrash) {
  const std::string data = GetLargeDownloadData();
  base::Process test_child_process;
  // If the request contains a range request, serve the content as requested.
  // Otherwise, send the first half of the data and hang before terminating the
  // client process.
  request_handler_ =
      base::BindLambdaForTesting([&](const HttpRequest& request) {
        if (request.headers.contains("Range")) {
          EXPECT_EQ(request.headers.at("If-Range"), "42");
          int lower_range = ParseRangeHeader(request.headers.at("Range"));

          std::unique_ptr<BasicHttpResponse> response =
              std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_PARTIAL_CONTENT);
          response->AddCustomHeader(
              "Content-Range",
              base::StringPrintf("bytes %d-%zu/%zu", lower_range, data.size(),
                                 data.size()));
          response->set_content(data.substr(lower_range));
          return base::WrapUnique<HttpResponse>(response.release());
        } else {
          return base::WrapUnique<HttpResponse>(
              new InterruptedHttpResponse(base::BindLambdaForTesting([&] {
                CHECK(test_child_process.IsValid());
                // Terminate the child process with extreme prejudice. SIGKILL
                // is used to prevent the client from cleaning up.
                kill(test_child_process.Handle(), SIGKILL);
              })));
        }
      });

  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitchASCII(kDownloadUrlSwitchName, GetURL().spec());
  command_line.AppendSwitchASCII(kDownloadSessionIdSwitchName,
                                 base::UnguessableToken::Create().ToString());
  test_child_process = base::SpawnMultiProcessTestChild(
      "CrashingDownloadClient", command_line, {});

  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), nullptr));

  // Restart the client and expect it to request the remaining content.
  test_child_process = base::SpawnMultiProcessTestChild(
      "CrashingDownloadClient", command_line, {});

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(CrashingDownloadClient) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDownloadUrlSwitchName));
  const GURL url(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      kDownloadUrlSwitchName));
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDownloadSessionIdSwitchName));
  const std::string download_session_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kDownloadSessionIdSwitchName);

  base::ScopedTempDir download_cache;
  EXPECT_TRUE(download_cache.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment;
  scoped_refptr<base::SequencedTaskRunner> background_sequence =
      base::ThreadPool::CreateSequencedTaskRunner(
          kTaskTraitsBackgroundDownloader);
  scoped_refptr<BackgroundDownloaderSharedSession> shared_session =
      MakeBackgroundDownloaderSharedSession(
          background_sequence, download_cache.GetPath(), download_session_id);
  scoped_refptr<BackgroundDownloader> downloader =
      base::MakeRefCounted<BackgroundDownloader>(nullptr, shared_session,
                                                 background_sequence);

  base::RunLoop run_loop;
  BackgroundDownloaderCrashingClientTest::DoStartDownload(
      downloader, url,
      base::BindLambdaForTesting(
          [](bool is_handled, const CrxDownloader::Result& result,
             const CrxDownloader::DownloadMetrics& metrics) {
            EXPECT_TRUE(is_handled);
            EXPECT_EQ(result.error, 0);
            EXPECT_TRUE(base::PathExists(result.response));
          })
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  return 0;
}

}  // namespace update_client
