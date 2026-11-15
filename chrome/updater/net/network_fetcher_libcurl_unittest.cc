// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network_fetcher_libcurl.h"

#include <curl/curl.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pair;

constexpr char kCrxBody[] = "crx-bytes";
constexpr char kUpdateResponse[] = "<response protocol=\"3.1\"/>";

// A host that only the proxy can resolve, so a proxied request must reach the
// proxy as sent.
GURL OriginUrl(std::string_view path) {
  return GURL("http://downloads.example.com").Resolve(path);
}

struct ReceivedRequest {
  std::string method;
  std::string host;
  std::string path;
  std::string content;
};

struct DownloadOutcome {
  int net_error = net::OK;
  int64_t content_size = -1;
  std::optional<std::pair<int, int64_t>> response;
  // Set if progress was reported before the response was.
  bool progress_before_response = false;
};

struct PostOutcome {
  std::optional<std::string> body;
  int net_error = net::OK;
  std::optional<std::pair<int, int64_t>> response;
  int progress_calls = 0;
};

class LibcurlNetworkFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    server_.RegisterRequestMonitor(base::BindRepeating(
        &LibcurlNetworkFetcherTest::RecordRequest, base::Unretained(this)));
    server_.RegisterRequestHandler(base::BindRepeating(
        &LibcurlNetworkFetcherTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(server_.Start());
  }

  // `proxy` is passed to the fetcher as is; empty means no proxy.
  std::unique_ptr<LibcurlNetworkFetcher> MakeFetcher(
      std::string proxy = std::string()) {
    std::unique_ptr<LibcurlNetworkFetcher> fetcher =
        LibcurlNetworkFetcher::Create(std::move(proxy));
    CHECK(fetcher);
    return fetcher;
  }

  // Downloads `url` and waits for completion. If `cancel` is set, runs the
  // cancel closure once the server has received the request, so the transfer
  // is cancelled while it waits on the server.
  DownloadOutcome Download(LibcurlNetworkFetcher& fetcher,
                           const GURL& url,
                           bool cancel = false) {
    DownloadOutcome outcome;
    base::RunLoop run_loop;
    base::RunLoop request_received;
    if (cancel) {
      base::AutoLock lock(requests_lock_);
      on_request_received_ =
          base::BindPostTaskToCurrentDefault(request_received.QuitClosure());
    }
    base::OnceClosure cancel_closure = fetcher.DownloadToFile(
        url, download_path(),
        base::BindLambdaForTesting(
            [&](int response_code, int64_t content_length) {
              outcome.response.emplace(response_code, content_length);
            }),
        base::BindLambdaForTesting([&](int64_t) {
          if (!outcome.response) {
            outcome.progress_before_response = true;
          }
        }),
        base::BindLambdaForTesting([&](int net_error, int64_t content_size) {
          outcome.net_error = net_error;
          outcome.content_size = content_size;
          run_loop.Quit();
        }));
    if (cancel) {
      request_received.Run();
      std::move(cancel_closure).Run();
    }
    run_loop.Run();
    return outcome;
  }

  PostOutcome Post(LibcurlNetworkFetcher& fetcher,
                   const GURL& url,
                   const std::string& data) {
    PostOutcome outcome;
    base::RunLoop run_loop;
    fetcher.PostRequest(
        url, data, "application/xml", {},
        base::BindLambdaForTesting(
            [&](int response_code, int64_t content_length) {
              outcome.response.emplace(response_code, content_length);
            }),
        base::BindLambdaForTesting([&](int64_t) { ++outcome.progress_calls; }),
        base::BindLambdaForTesting([&](std::optional<std::string> body,
                                       int net_error, const std::string&,
                                       const std::string&, const std::string&,
                                       int64_t) {
          outcome.body = std::move(body);
          outcome.net_error = net_error;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  // The test server in libcurl's proxy syntax.
  std::string ProxyString() const {
    return base::StrCat({"http://", server_.host_port_pair().ToString()});
  }

  // A proxy that refuses connections.
  std::string ClosedProxyString() {
    net::test_server::EmbeddedTestServer server;
    EXPECT_TRUE(server.Start());
    const std::string proxy =
        base::StrCat({"http://", server.host_port_pair().ToString()});
    EXPECT_TRUE(server.ShutdownAndWaitUntilComplete());
    return proxy;
  }

  std::string ReadDownload() {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(download_path(), &content));
    return content;
  }

  base::FilePath download_path() const {
    return temp_dir_.GetPath().AppendASCII("download");
  }

  std::vector<ReceivedRequest> received_requests() {
    base::AutoLock lock(requests_lock_);
    return requests_;
  }

  base::test::TaskEnvironment task_environment_;
  net::test_server::EmbeddedTestServer server_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/hang") {
      return std::make_unique<net::test_server::HungResponse>();
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.relative_url == "/update.crx") {
      response->set_content(kCrxBody);
    } else if (request.relative_url == "/redirect") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", OriginUrl("/update.crx").spec());
      // A body makes libcurl report progress for the redirect response too.
      response->set_content("moved");
    } else if (request.relative_url == "/redirect-to-ftp") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", "ftp://ftp.example.com/update.crx");
    } else if (request.relative_url == "/service/update2") {
      response->set_content(kUpdateResponse);
    } else {
      // No body: libcurl's progress callback never sees this response.
      response->set_code(net::HTTP_NOT_FOUND);
    }
    return response;
  }

  // Runs on the test server's thread.
  void RecordRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_lock_);
    auto host = request.headers.find("Host");
    requests_.push_back(
        {request.method_string,
         host != request.headers.end() ? host->second : std::string(),
         request.relative_url, request.content});
    if (on_request_received_) {
      std::move(on_request_received_).Run();
    }
  }

  base::ScopedTempDir temp_dir_;
  base::Lock requests_lock_;
  std::vector<ReceivedRequest> requests_ GUARDED_BY(requests_lock_);
  base::OnceClosure on_request_received_ GUARDED_BY(requests_lock_);
};

using LibcurlNetworkFetcherDeathTest = LibcurlNetworkFetcherTest;

TEST_F(LibcurlNetworkFetcherTest, DownloadsFile) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(), server_.GetURL("/update.crx"));

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_EQ(outcome.content_size, static_cast<int64_t>(strlen(kCrxBody)));
  EXPECT_EQ(ReadDownload(), kCrxBody);
  EXPECT_THAT(outcome.response, Optional(Pair(net::HTTP_OK, strlen(kCrxBody))));
}

TEST_F(LibcurlNetworkFetcherTest, DownloadsThroughProxy) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(ProxyString()), OriginUrl("/update.crx"));

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_EQ(ReadDownload(), kCrxBody);
  EXPECT_THAT(
      received_requests(),
      ElementsAre(AllOf(Field(&ReceivedRequest::method, "GET"),
                        Field(&ReceivedRequest::host, "downloads.example.com"),
                        Field(&ReceivedRequest::path, "/update.crx"))));
}

TEST_F(LibcurlNetworkFetcherTest, PostsThroughProxy) {
  const PostOutcome outcome = Post(*MakeFetcher(ProxyString()),
                                   OriginUrl("/service/update2"), "<request/>");

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_THAT(outcome.body, Optional(std::string(kUpdateResponse)));
  EXPECT_THAT(outcome.response,
              Optional(Pair(net::HTTP_OK, strlen(kUpdateResponse))));
  EXPECT_THAT(
      received_requests(),
      ElementsAre(AllOf(Field(&ReceivedRequest::method, "POST"),
                        Field(&ReceivedRequest::host, "downloads.example.com"),
                        Field(&ReceivedRequest::content, "<request/>"))));
}

TEST_F(LibcurlNetworkFetcherTest, FollowsRedirects) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(ProxyString()), OriginUrl("/redirect"));

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_EQ(ReadDownload(), kCrxBody);
  EXPECT_THAT(outcome.response, Optional(Pair(net::HTTP_OK, strlen(kCrxBody))));
  // The redirect's own body is not progress of the download.
  EXPECT_FALSE(outcome.progress_before_response);
  EXPECT_THAT(received_requests(),
              ElementsAre(Field(&ReceivedRequest::path, "/redirect"),
                          Field(&ReceivedRequest::path, "/update.crx")));
}

TEST_F(LibcurlNetworkFetcherTest, DoesNotFollowRedirectToNonHttpScheme) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(), server_.GetURL("/redirect-to-ftp"));

  EXPECT_EQ(outcome.net_error, CURLE_UNSUPPORTED_PROTOCOL);
  EXPECT_FALSE(outcome.response.has_value());
  EXPECT_THAT(received_requests(),
              ElementsAre(Field(&ReceivedRequest::path, "/redirect-to-ftp")));
}

TEST_F(LibcurlNetworkFetcherTest, ReportsStatusOfResponseWithoutBody) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(), server_.GetURL("/missing.crx"));

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_THAT(outcome.response, Optional(Pair(net::HTTP_NOT_FOUND, 0)));
}

TEST_F(LibcurlNetworkFetcherTest, PostReportsStatusOfResponseWithoutBody) {
  const PostOutcome outcome =
      Post(*MakeFetcher(), server_.GetURL("/missing"), "<request/>");

  EXPECT_EQ(outcome.net_error, net::OK);
  EXPECT_THAT(outcome.response, Optional(Pair(net::HTTP_NOT_FOUND, 0)));
  // The request body's upload is not download progress.
  EXPECT_EQ(outcome.progress_calls, 0);
}

TEST_F(LibcurlNetworkFetcherTest, CancelCompletesWithErrAborted) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(), server_.GetURL("/hang"), /*cancel=*/true);

  EXPECT_EQ(outcome.net_error, net::ERR_ABORTED);
  EXPECT_FALSE(outcome.response.has_value());
  // The transfer was cancelled while blocked on the server's response.
  EXPECT_THAT(received_requests(),
              ElementsAre(Field(&ReceivedRequest::path, "/hang")));
}

TEST_F(LibcurlNetworkFetcherDeathTest, RejectsSecondTransfer) {
  std::unique_ptr<LibcurlNetworkFetcher> fetcher = MakeFetcher();
  ASSERT_EQ(Post(*fetcher, server_.GetURL("/service/update2"), "<request/>")
                .net_error,
            net::OK);

  EXPECT_CHECK_DEATH(
      Post(*fetcher, server_.GetURL("/service/update2"), "<request/>"));
}

TEST_F(LibcurlNetworkFetcherTest, FailsWhenProxyRefusesConnections) {
  const DownloadOutcome outcome =
      Download(*MakeFetcher(ClosedProxyString()), OriginUrl("/update.crx"));

  EXPECT_EQ(outcome.net_error, CURLE_COULDNT_CONNECT);
  EXPECT_FALSE(outcome.response.has_value());
  EXPECT_TRUE(received_requests().empty());
}

TEST_F(LibcurlNetworkFetcherTest, PostFailsWhenProxyRefusesConnections) {
  const PostOutcome outcome = Post(*MakeFetcher(ClosedProxyString()),
                                   OriginUrl("/service/update2"), "<request/>");

  EXPECT_EQ(outcome.net_error, CURLE_COULDNT_CONNECT);
  EXPECT_FALSE(outcome.response.has_value());
  EXPECT_TRUE(received_requests().empty());
}

}  // namespace

}  // namespace updater
