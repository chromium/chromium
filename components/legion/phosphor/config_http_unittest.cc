// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/legion/phosphor/config_http.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/legion/features.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace legion::phosphor {
namespace {

const char kProtobufContentType[] = "application/x-protobuf";

class ConfigHttpTest : public testing::Test {
 protected:
  void SetUp() override {
    http_fetcher_ = std::make_unique<ConfigHttp>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ConfigHttp> http_fetcher_;
};

TEST_F(ConfigHttpTest, DoRequestSendsCorrectRequestInitialData) {
  auto request_type = quiche::BlindSignMessageRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetInitialDataPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  ASSERT_TRUE(expected_url.is_valid());

  // Set up the response to return from the mock.
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, expected_url);

        ASSERT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional(base::StrCat({"Bearer ", authorization_header})));
        ASSERT_THAT(request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
                    testing::Optional(std::string(kProtobufContentType)));

        std::string response_str = "Response body";
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            request.url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  // Note: We use a lambda expression and `TestFuture::SetValue()` instead of
  // `TestFuture::GetCallback()` to avoid having to convert the
  // `base::OnceCallback` to a `quiche::SignedTokenCallback` (an
  // `absl::AnyInvocable` behind the scenes).
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(ConfigHttpTest, DoRequestFailsToConnectReturnsFailureStatus) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetTokensPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  ASSERT_TRUE(expected_url.is_valid());

  // Mock no response from Authentication Server (such as a network error).
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      expected_url, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ("Failed Request to Authentication Server",
            result.error().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.error().code());
}

TEST_F(ConfigHttpTest, DoRequestInvalidFinchParametersFailsGracefully) {
  std::map<std::string, std::string> parameters;
  parameters["LegionTokenServerUrl"] = "<(^_^)>";
  parameters["LegionTokenServerGetInitialDataPath"] = "(>_<)";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(legion::kLegion,
                                                         std::move(parameters));

  // Create a new ConfigHttp for this test so that the new
  // FeatureParams get used.
  std::unique_ptr<ConfigHttp> http_fetcher = std::make_unique<ConfigHttp>(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_));

  auto request_type = quiche::BlindSignMessageRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher->DoRequest(request_type, authorization_header, body,
                          std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ("Invalid Legion Token URL", result.error().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.error().code());
}

TEST_F(ConfigHttpTest, DoRequestHttpFailureStatus) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetTokensPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  ASSERT_TRUE(expected_url.is_valid());

  // Mock a non-200 HTTP response from Authentication Server.
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(expected_url.spec(), response_body,
                                       net::HTTP_BAD_REQUEST);

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(quiche::BlindSignMessageResponse::HttpCodeToStatusCode(
                net::HTTP_BAD_REQUEST),
            result.value().status_code());
}

TEST_F(ConfigHttpTest, DoRequestRetriesOnNetworkChangeThenSucceeds) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetTokensPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  ASSERT_TRUE(expected_url.is_valid());

  int requests_made = 0;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        requests_made++;
        if (requests_made == 1) {
          test_url_loader_factory_.AddResponse(
              request.url, network::mojom::URLResponseHead::New(),
              std::string(),
              network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          return;
        }

        std::string response_str = "Response body";
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            request.url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ(2, requests_made);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(ConfigHttpTest, DoRequestRetriesExhaustedFails) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetTokensPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  ASSERT_TRUE(expected_url.is_valid());

  int requests_made = 0;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        requests_made++;
        test_url_loader_factory_.AddResponse(
            request.url, network::mojom::URLResponseHead::New(), std::string(),
            network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
      }));

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ(3, requests_made);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ("Failed Request to Authentication Server",
            result.error().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.error().code());
}

TEST_F(ConfigHttpTest, DoRequestInvalidRequestType) {
  auto request_type = quiche::BlindSignMessageRequestType::kUnknown;
  std::string authorization_header = "token";
  std::string body = "body";

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ("Invalid request type", result.error().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.error().code());
}

TEST_F(ConfigHttpTest, DoRequestMissingAuthorizationHeaderFails) {
  auto request_type = quiche::BlindSignMessageRequestType::kGetInitialData;
  std::optional<std::string_view> authorization_header;
  std::string body = "body";

  base::test::TestFuture<
      base::expected<quiche::BlindSignMessageResponse, absl::Status>>
      result_future;
  auto callback = [&result_future](auto response) {
    if (response.ok()) {
      result_future.SetValue(base::ok(*std::move(response)));
    } else {
      result_future.SetValue(base::unexpected(std::move(response).status()));
    }
  };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  auto result = result_future.Get();

  EXPECT_EQ("Missing Authorization header", result.error().message());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument, result.error().code());
}

}  // namespace
}  // namespace legion::phosphor
