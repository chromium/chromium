// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/update_client/network.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <Urlmon.h>
#endif

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

class UpdaterNetworkTest : public ::testing::Test {
 public:
  ~UpdaterNetworkTest() override = default;

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
    DownloadToFileCompleted();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.method == net::test_server::HttpMethod::METHOD_POST) {
      // Echo the posted data back.
      http_response->set_content(request.content);
      http_response->AddCustomHeader("x-retry-after", "67");
      http_response->AddCustomHeader("etag", "Wfhw789h");
      http_response->AddCustomHeader("x-cup-server-proof", "server-proof");
    } else if (request.method == net::test_server::HttpMethod::METHOD_GET) {
      http_response->set_content("hello");
      http_response->set_content_type("application/octet-stream");
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    http_response->set_code(net::HTTP_OK);
    return http_response;
  }

  MOCK_METHOD(void, DownloadToFileCompleted, ());
  MOCK_METHOD(void, PostRequestCompleted, ());

 protected:
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  net::test_server::EmbeddedTestServer test_server_;
  std::unique_ptr<update_client::NetworkFetcher> fetcher_;

 private:
  void SetUp() override {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &UpdaterNetworkTest::HandleRequest, base::Unretained(this)));
    server_handle_ = test_server_.StartAndReturnHandle();
    ASSERT_TRUE(server_handle_);
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>(
        PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()));
    fetcher_ = network_fetcher_factory_->Create();
  }

  net::test_server::EmbeddedTestServerHandle server_handle_;
  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
};

// Sets up a download for "chrome/updater/test/data/signed.exe" using the
// embedded test server running on localhost.
class UpdaterDownloadTest : public ::testing::Test {
 protected:
  ~UpdaterDownloadTest() override = default;

  base::FilePath dest_;
  GURL gurl_;

 private:
  void SetUp() override {
    server_.ServeFilesFromSourceDirectory("chrome/updater/test/data");
    server_handle_ = server_.StartAndReturnHandle();
    ASSERT_TRUE(scoped_dir_.CreateUniqueTempDir());
    dest_ = scoped_dir_.GetPath().AppendASCII("updater-signed.exe");
    gurl_ = GURL(base::StrCat({server_.base_url().spec(), "signed.exe"}));
  }

  base::test::TaskEnvironment task_environment_;
  net::test_server::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
  base::ScopedTempDir scoped_dir_;
};

}  // namespace

TEST_F(UpdaterNetworkTest, NetworkFetcherPostRequest) {
  base::ScopedDisallowBlocking no_blocking_allowed_on_sequence;
  const std::string kPostData = "\x01\x00\x55\x33\xda\x10\x44";
  EXPECT_CALL(*this, PostRequestCompleted())
      .WillOnce(RunClosure(run_loop_.QuitClosure()));
  fetcher_->PostRequest(
      test_server_.GetURL("/echo"), kPostData, {}, {},
      base::BindRepeating(&UpdaterNetworkTest::StartedCallback,
                          base::Unretained(this)),
      base::BindRepeating(&UpdaterNetworkTest::ProgressCallback,
                          base::Unretained(this)),
      base::BindOnce(&UpdaterNetworkTest::PostRequestCompleteCallback,
                     base::Unretained(this), kPostData));
  run_loop_.Run();
}

TEST_F(UpdaterNetworkTest, NetworkFetcherDownloadToFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath test_file_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("downloaded_file"));

  {
    base::ScopedDisallowBlocking no_blocking_allowed_on_sequence;

    EXPECT_CALL(*this, DownloadToFileCompleted())
        .WillOnce(RunClosure(run_loop_.QuitClosure()));
    fetcher_->DownloadToFile(
        test_server_.GetURL("/echo"), test_file_path,
        base::BindRepeating(&UpdaterNetworkTest::StartedCallback,
                            base::Unretained(this)),
        base::BindRepeating(&UpdaterNetworkTest::ProgressCallback,
                            base::Unretained(this)),
        base::BindOnce(&UpdaterNetworkTest::DownloadCallback,
                       base::Unretained(this), test_file_path));
    run_loop_.Run();
  }

  EXPECT_TRUE(base::PathExists(test_file_path));
}

TEST_F(UpdaterDownloadTest, NetworkFetcher) {
  EXPECT_FALSE(base::PathExists(dest_));

  base::RunLoop run_loop;
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()));
  ASSERT_NE(factory, nullptr);
  {
    base::ScopedDisallowBlocking no_blocking_allowed_on_sequence;
    std::unique_ptr<update_client::NetworkFetcher> fetcher = factory->Create();
    ASSERT_NE(fetcher, nullptr);
    fetcher->DownloadToFile(
        gurl_, dest_,
        base::BindRepeating([](int response_code, int64_t /*content_length*/) {
          EXPECT_EQ(response_code, 200);
        }),
        base::BindRepeating([](int64_t /*current*/) {}),
        base::BindOnce([](int net_error, int64_t content_size) {
          EXPECT_EQ(net_error, 0);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_TRUE(base::PathExists(dest_));
}

#if BUILDFLAG(IS_WIN)
// Tests that a direct download through URL moniker from a local HTTP server is
// reasonably fast. This provides a baseline to compare the network throughput
// of various fetchers.
TEST_F(UpdaterDownloadTest, URLMonFetcher) {
  EXPECT_FALSE(base::PathExists(dest_));
  EXPECT_HRESULT_SUCCEEDED(
      ::URLDownloadToFile(nullptr, base::ASCIIToWide(gurl_.spec()).c_str(),
                          dest_.value().c_str(), 0, nullptr));
  EXPECT_TRUE(base::PathExists(dest_));
}
#endif
}  // namespace updater
