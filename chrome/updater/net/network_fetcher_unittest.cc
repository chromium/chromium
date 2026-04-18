// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/update_client/network.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace {

using ::testing::_;

using HeaderMap = base::flat_map<std::string, std::string>;

// A mock NetworkFetcher for testing LoggingNetworkFetcher delegation.
class MockNetworkFetcher : public update_client::NetworkFetcher {
 public:
  ~MockNetworkFetcher() override = default;

  MOCK_METHOD(void,
              PostRequest,
              (const GURL& url,
               const std::string& post_data,
               const std::string& content_type,
               const HeaderMap& post_additional_headers,
               ResponseStartedCallback response_started_callback,
               ProgressCallback progress_callback,
               PostRequestCompleteCallback post_request_complete_callback),
              (override));

  MOCK_METHOD(
      base::OnceClosure,
      DownloadToFile,
      (const GURL& url,
       const base::FilePath& file_path,
       ResponseStartedCallback response_started_callback,
       ProgressCallback progress_callback,
       DownloadToFileCompleteCallback download_to_file_complete_callback),
      (override));
};

// Tests for NetworkFetcherFactory.
class NetworkFetcherFactoryTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkFetcherFactoryTest, CreatesNonNullFetcher) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()),
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, NulloptProxyConfig) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      /*policy_service_proxy_configuration=*/std::nullopt,
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, AutoDetectProxyConfig) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = true;
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, ExplicitProxyUrl) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = false;
  proxy_config.proxy_url = "http://proxy.example.com:8080";
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, PacUrl) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = false;
  proxy_config.proxy_pac_url = "http://proxy.example.com/pac";
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, CreatesMultipleIndependentFetchers) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      /*policy_service_proxy_configuration=*/std::nullopt,
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);

  auto fetcher1 = factory->Create();
  auto fetcher2 = factory->Create();
  EXPECT_NE(fetcher1, nullptr);
  EXPECT_NE(fetcher2, nullptr);
  EXPECT_NE(fetcher1.get(), fetcher2.get());
}

// Tests for LoggingNetworkFetcher.
class LoggingNetworkFetcherTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoggingNetworkFetcherTest, NullImplDoesNotCrash) {
  LoggingNetworkFetcher logging_fetcher(nullptr);
}

TEST_F(LoggingNetworkFetcherTest, DelegatesPostRequest) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const GURL kUrl("http://example.com/update");
  const std::string kPostData = "test_data";
  const std::string kContentType = "application/json";

  EXPECT_CALL(*mock_ptr, PostRequest(kUrl, kPostData, kContentType, _, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run("response", 0, "etag", "proof", "cookie", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      kUrl, kPostData, kContentType, {}, base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, std::optional<std::string> response_body,
             int net_error, const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            EXPECT_EQ(*response_body, "response");
            EXPECT_EQ(net_error, 0);
            EXPECT_EQ(header_etag, "etag");
            EXPECT_EQ(header_x_cup_server_proof, "proof");
            EXPECT_EQ(header_set_cookie, "cookie");
            EXPECT_EQ(xheader_retry_after_sec, -1);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, DelegatesDownloadToFile) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const GURL kUrl("http://example.com/download");
  const base::FilePath kPath(FILE_PATH_LITERAL("file.bin"));

  EXPECT_CALL(*mock_ptr, DownloadToFile(kUrl, kPath, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       callback) {
        std::move(callback).Run(0, 1024);
        return base::DoNothing();
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  base::OnceClosure cancel = logging_fetcher.DownloadToFile(
      kUrl, kPath, base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, int net_error, int64_t content_size) {
            EXPECT_EQ(net_error, 0);
            EXPECT_EQ(content_size, 1024);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, PropagatesPostRequestError) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  EXPECT_CALL(*mock_ptr, PostRequest(_, _, _, _, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run(std::nullopt, -1, "", "", "", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      GURL("http://example.com"), "data", "text/plain", {}, base::DoNothing(),
      base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, std::optional<std::string> response_body,
             int net_error, const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            EXPECT_FALSE(response_body.has_value());
            EXPECT_EQ(net_error, -1);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, ForwardsAdditionalHeaders) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const HeaderMap kHeaders = {
      {"Authorization", "Bearer token123"},
      {"X-Custom", "value"},
  };

  EXPECT_CALL(*mock_ptr, PostRequest(_, _, _, kHeaders, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run("ok", 0, "", "", "", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      GURL("http://example.com"), "data", "text/plain", kHeaders,
      base::DoNothing(), base::DoNothing(),
      base::BindOnce([](base::OnceClosure quit, std::optional<std::string>, int,
                        const std::string&, const std::string&,
                        const std::string&, int64_t) { std::move(quit).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, PropagatesDownloadError) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  EXPECT_CALL(*mock_ptr, DownloadToFile(_, _, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       callback) {
        std::move(callback).Run(-1, 0);
        return base::DoNothing();
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.DownloadToFile(
      GURL("http://example.com/file"), base::FilePath(FILE_PATH_LITERAL("f")),
      base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, int net_error, int64_t content_size) {
            EXPECT_EQ(net_error, -1);
            EXPECT_EQ(content_size, 0);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace
}  // namespace updater
