// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/ip_protection/common/ip_protection_config_http.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {
namespace {

const char kProtobufContentType[] = "application/x-protobuf";

class IpProtectionConfigHttpTest : public testing::Test {
 protected:
  void SetUp() override {
    http_fetcher_ = std::make_unique<IpProtectionConfigHttp>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    token_server_get_initial_data_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetInitialDataPath.Get()}));
    ASSERT_TRUE(token_server_get_initial_data_url_.is_valid());
    token_server_get_tokens_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetTokensPath.Get()}));
    ASSERT_TRUE(token_server_get_tokens_url_.is_valid());
    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IpProtectionConfigHttp> http_fetcher_;
  GURL token_server_get_initial_data_url_;
  GURL token_server_get_tokens_url_;
  GURL token_server_get_proxy_config_url_;
};

TEST_F(IpProtectionConfigHttpTest, DoRequestSendsCorrectRequestInitialData) {
  auto request_type = quiche::BlindSignMessageRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  // Set up the response to return from the mock.
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_initial_data_url_);

        ASSERT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional(base::StrCat({"Bearer ", authorization_header})));
        EXPECT_FALSE(
            request.headers.HasHeader("Ip-Protection-Debug-Experiment-Arm"));
        ASSERT_THAT(request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
                    testing::Optional(std::string(kProtobufContentType)));

        std::string response_str = "Response body";
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            request.url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  // Note: We use a lambda expression and `TestFuture::SetValue()` instead of
  // `TestFuture::GetCallback()` to avoid having to convert the
  // `base::OnceCallback` to a `quiche::SignedTokenCallback` (an
  // `absl::AnyInvocable` behind the scenes).
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignMessageResponse> result = result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(IpProtectionConfigHttpTest,
       DoRequestSendsCorrectRequestAuthAndSignAndExperimentArm) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyDebugExperimentArm"] = "42";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Set up the response to return from the mock.
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_tokens_url_);

        ASSERT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional(base::StrCat({"Bearer ", authorization_header})));
        ASSERT_THAT(
            request.headers.GetHeader("Ip-Protection-Debug-Experiment-Arm"),
            testing::Optional(std::string("42")));
        ASSERT_THAT(request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
                    testing::Optional(std::string(kProtobufContentType)));

        std::string response_str = "Response body";
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            request.url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  // Note: We use a lambda expression and `TestFuture::SetValue()` instead of
  // `TestFuture::GetCallback()` to avoid having to convert the
  // `base::OnceCallback` to a `quiche::SignedTokenCallback` (an
  // `absl::AnyInvocable` behind the scenes).
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignMessageResponse> result = result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(IpProtectionConfigHttpTest,
       DoRequestFailsToConnectReturnsFailureStatus) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock no response from Authentication Server (such as a network error).
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_tokens_url_, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignMessageResponse> result = result_future.Get();

  EXPECT_EQ("Failed Request to Authentication Server",
            result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}

TEST_F(IpProtectionConfigHttpTest,
       DoRequestInvalidFinchParametersFailsGracefully) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyTokenServer"] = "<(^_^)>";
  parameters["IpPrivacyTokenServerGetInitialDataPath"] = "(>_<)";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  // Create a new IpProtectionConfigHttp for this test so that the new
  // FeatureParams get used.
  std::unique_ptr<IpProtectionConfigHttp> http_fetcher =
      std::make_unique<IpProtectionConfigHttp>(
          test_url_loader_factory_.GetSafeWeakWrapper());

  auto request_type = quiche::BlindSignMessageRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher->DoRequest(request_type, authorization_header, body,
                          std::move(callback));

  absl::StatusOr<quiche::BlindSignMessageResponse> result = result_future.Get();

  EXPECT_EQ("Invalid IP Protection Token URL", result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}

TEST_F(IpProtectionConfigHttpTest, DoRequestHttpFailureStatus) {
  auto request_type = quiche::BlindSignMessageRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock a non-200 HTTP response from Authentication Server.
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(token_server_get_tokens_url_.spec(),
                                       response_body, net::HTTP_BAD_REQUEST);

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignMessageResponse> result = result_future.Get();

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(quiche::BlindSignMessageResponse::HttpCodeToStatusCode(
                net::HTTP_BAD_REQUEST),
            result.value().status_code());
}

}  // namespace
}  // namespace ip_protection
