// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/net/network.h"
#include "chrome/updater/mac/net/network_fetcher.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ResponseStartedCallback =
    update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadToFileCompleteCallback =
    update_client::NetworkFetcher::DownloadToFileCompleteCallback;

namespace updater {
ACTION_P(RunClosure, closure) {
  closure.Run();
}

static base::FilePath testFilePath;

class ChromeUpdaterNetworkMacTest : public ::testing::Test {
 public:
  ~ChromeUpdaterNetworkMacTest() override = default;

#pragma mark - Callback Methods
  void StartedCallback(int response_code, int64_t content_length) {
    EXPECT_EQ(response_code, 200);
  }

  void ProgressCallback(int64_t current) {
    EXPECT_GE(current, 0);
    EXPECT_LE(current, 100);
  }

  void PostRequestCompleteCallback(std::unique_ptr<std::string> response_body,
                                   int net_error,
                                   const std::string& header_etag,
                                   int64_t xheader_retry_after_sec) {
    EXPECT_EQ(net_error, 200);
    EXPECT_GT(header_etag.length(), 0u);
    EXPECT_EQ(xheader_retry_after_sec, 67);
    PostRequestCompleted();
  }

  void DownloadCallback(int net_error, int64_t content_size) {
    EXPECT_EQ(net_error, 200);
    EXPECT_GT(content_size, 0);
    EXPECT_FALSE(testFilePath.empty());
    EXPECT_TRUE(base::PathExists(testFilePath));
    DownloadToFileCompleted();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader("X-Retry-After", "67");
    http_response->AddCustomHeader("ETag", "Wfhw789h");
    return http_response;
  }

  MOCK_METHOD0(DownloadToFileCompleted, void(void));
  MOCK_METHOD0(PostRequestCompleted, void(void));

  base::test::SingleThreadTaskEnvironment task_environment_;
};

#pragma mark - Test Methods
TEST_F(ChromeUpdaterNetworkMacTest, NetworkFetcherMacHTTPFactory) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();
  quit_closure.Run();
  run_loop.Run();
  EXPECT_NE(nullptr, fetcher.get());
}

TEST_F(ChromeUpdaterNetworkMacTest, NetworkFetcherMacPostRequest) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::Bind(
      &ChromeUpdaterNetworkMacTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(test_server.Start());
  const GURL url = test_server.GetURL("/echo");

  fetcher->PostRequest(
      url, "", {},
      base::BindOnce(&ChromeUpdaterNetworkMacTest::StartedCallback,
                     base::Unretained(this)),
      base::BindRepeating(&ChromeUpdaterNetworkMacTest::ProgressCallback,
                          base::Unretained(this)),
      base::BindOnce(&ChromeUpdaterNetworkMacTest::PostRequestCompleteCallback,
                     base::Unretained(this)));

  run_loop.Run();
}

TEST_F(ChromeUpdaterNetworkMacTest, NetworkFetcherMacDownloadToFile) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DownloadToFileCompleted())
      .WillOnce(RunClosure(quit_closure));
  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::Bind(
      &ChromeUpdaterNetworkMacTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(test_server.Start());
  const GURL url = test_server.GetURL("/echo");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  testFilePath =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("downloaded_file"));

  fetcher->DownloadToFile(
      url, testFilePath,
      base::BindOnce(&ChromeUpdaterNetworkMacTest::StartedCallback,
                     base::Unretained(this)),
      base::BindRepeating(&ChromeUpdaterNetworkMacTest::ProgressCallback,
                          base::Unretained(this)),
      base::BindOnce(&ChromeUpdaterNetworkMacTest::DownloadCallback,
                     base::Unretained(this)));

  run_loop.Run();
}
}  // namespace updater
