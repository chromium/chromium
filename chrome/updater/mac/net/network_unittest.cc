// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/net/network.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/updater/mac/net/network_fetcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace {

using ::base::test::RunClosure;
using ResponseStartedCallback =
    ::update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = ::update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    ::update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadToFileCompleteCallback =
    ::update_client::NetworkFetcher::DownloadToFileCompleteCallback;

}  // namespace

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

  void PostRequestCompleteCallback(const std::string& expected_body,
                                   std::unique_ptr<std::string> response_body,
                                   int net_error,
                                   const std::string& header_etag,
                                   const std::string& header_x_cup_server_proof,
                                   int64_t xheader_retry_after_sec) {
    EXPECT_STREQ(response_body->c_str(), expected_body.c_str());
    EXPECT_EQ(net_error, 0);
    EXPECT_STREQ(header_etag.c_str(), "Wfhw789h");
    EXPECT_STREQ(header_x_cup_server_proof.c_str(), "server-proof");
    EXPECT_EQ(xheader_retry_after_sec, 67);
    PostRequestCompleted();
  }

  void DownloadCallback(const base::FilePath& test_file_path,
                        int net_error,
                        int64_t content_size) {
    EXPECT_EQ(net_error, 0);
    EXPECT_GT(content_size, 0);
    EXPECT_FALSE(test_file_path.empty());
    EXPECT_TRUE(base::PathExists(test_file_path));
    DownloadToFileCompleted();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.method == net::test_server::HttpMethod::METHOD_POST) {
      // Echo the posted data back.
      http_response->set_content(request.content);
      http_response->AddCustomHeader("X-Retry-After", "67");
      http_response->AddCustomHeader("ETag", "Wfhw789h");
      http_response->AddCustomHeader("X-Cup-Server-Proof", "server-proof");
    } else if (request.method == net::test_server::HttpMethod::METHOD_GET) {
      http_response->set_content("hello");
      http_response->set_content_type("application/octet-stream");
    } else {
      NOTREACHED();
    }

    http_response->set_code(net::HTTP_OK);
    return http_response;
  }

  MOCK_METHOD0(DownloadToFileCompleted, void(void));
  MOCK_METHOD0(PostRequestCompleted, void(void));

  base::test::SingleThreadTaskEnvironment task_environment_;
};

#pragma mark - Test Methods

TEST_F(ChromeUpdaterNetworkMacTest, NetworkFetcherMacPostRequest) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      &ChromeUpdaterNetworkMacTest::HandleRequest, base::Unretained(this)));
  const net::test_server::EmbeddedTestServerHandle server_handle =
      test_server.StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  const std::string kPostData = "\x01\x00\x55\x33\xda\x10\x44";

  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();
  fetcher->PostRequest(
      test_server.GetURL("/echo"), kPostData, {}, {},
      base::BindOnce(&ChromeUpdaterNetworkMacTest::StartedCallback,
                     base::Unretained(this)),
      base::BindRepeating(&ChromeUpdaterNetworkMacTest::ProgressCallback,
                          base::Unretained(this)),
      base::BindOnce(&ChromeUpdaterNetworkMacTest::PostRequestCompleteCallback,
                     base::Unretained(this), kPostData));
  run_loop.Run();
}

TEST_F(ChromeUpdaterNetworkMacTest, NetworkFetcherMacDownloadToFile) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DownloadToFileCompleted())
      .WillOnce(RunClosure(quit_closure));

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      &ChromeUpdaterNetworkMacTest::HandleRequest, base::Unretained(this)));
  const net::test_server::EmbeddedTestServerHandle server_handle =
      test_server.StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath test_file_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("downloaded_file"));

  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();
  fetcher->DownloadToFile(
      test_server.GetURL("/echo"), test_file_path,
      base::BindOnce(&ChromeUpdaterNetworkMacTest::StartedCallback,
                     base::Unretained(this)),
      base::BindRepeating(&ChromeUpdaterNetworkMacTest::ProgressCallback,
                          base::Unretained(this)),
      base::BindOnce(&ChromeUpdaterNetworkMacTest::DownloadCallback,
                     base::Unretained(this), test_file_path));
  run_loop.Run();
}

}  // namespace updater
