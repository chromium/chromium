// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/mock_access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/mock_cup.h"
#include "components/autofill_assistant/browser/service/mock_simple_url_loader_factory.h"
#include "components/autofill_assistant/browser/service/mock_url_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArgs;

namespace {

std::unique_ptr<::network::mojom::URLResponseHead> CreateResponseInfo(
    int status_code,
    const std::string& status) {
  auto response = std::make_unique<::network::mojom::URLResponseHead>();
  response->headers =
      base::MakeRefCounted<::net::HttpResponseHeaders>(base::StrCat(
          {"HTTP/1.1 ", base::NumberToString(status_code), " ", status}));
  return response;
}

class ServiceRequestSenderImplTest : public testing::Test {
 public:
  ServiceRequestSenderImplTest() = default;
  ~ServiceRequestSenderImplTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockCallback<base::OnceCallback<void(int, const std::string&)>>
      mock_response_callback_;
  // Note: |task_environment_| must be created before |context_|, else creation
  // of |context_| will fail (see content/public/test/test_browser_context.cc).
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  NiceMock<MockAccessTokenFetcher> mock_access_token_fetcher_;

  void InitCupFeatures(bool enableSigning, bool enableVerifying) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (enableSigning) {
      enabled_features.push_back(autofill_assistant::features::
                                     kAutofillAssistantSignGetActionsRequests);
    } else {
      disabled_features.push_back(autofill_assistant::features::
                                      kAutofillAssistantSignGetActionsRequests);
    }

    if (enableVerifying) {
      enabled_features.push_back(
          autofill_assistant::features::
              kAutofillAssistantVerifyGetActionsResponses);
    } else {
      disabled_features.push_back(
          autofill_assistant::features::
              kAutofillAssistantVerifyGetActionsResponses);
    }

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

TEST_F(ServiceRequestSenderImplTest, SendUnauthenticatedRequest) {
  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ nullptr,
      std::move(cup_factory),
      std::move(loader_factory),
      std::string("fake_api_key"),
      /* auth_enabled = */ false,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(
      GURL("https://www.example.com"), std::string("request"),
      mock_response_callback_.Get(), RpcType::GET_TRIGGER_SCRIPTS);
}

TEST_F(ServiceRequestSenderImplTest, SendAuthenticatedRequest) {
  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        std::string authorization;
        EXPECT_TRUE(resource_request->headers.GetHeader("Authorization",
                                                        &authorization));
        EXPECT_EQ(authorization, "Bearer access_token");
        EXPECT_EQ(resource_request->url, GURL("https://www.example.com"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));
  EXPECT_CALL(mock_access_token_fetcher_, OnFetchAccessToken)
      .Times(1)
      .WillOnce(RunOnceCallback<0>(true, "access_token"));
  EXPECT_CALL(mock_access_token_fetcher_, InvalidateAccessToken).Times(0);

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ &mock_access_token_fetcher_,
      std::move(cup_factory),
      std::move(loader_factory),
      /* api_key = */ std::string(""),
      /* auth_enabled = */ true,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(GURL("https://www.example.com"),
                             std::string("request"),
                             mock_response_callback_.Get(),
                             autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS);
}

TEST_F(ServiceRequestSenderImplTest,
       AuthRequestFallsBackToApiKeyOnEmptyAccessToken) {
  EXPECT_CALL(mock_access_token_fetcher_, OnFetchAccessToken)
      .Times(1)
      .WillOnce(RunOnceCallback<0>(true, /*access_token = */ ""));

  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ &mock_access_token_fetcher_,
      std::move(cup_factory),
      std::move(loader_factory),
      /* api_key = */ std::string("fake_api_key"),
      /* auth_enabled = */ true,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(GURL("https://www.example.com"),
                             std::string("request"),
                             mock_response_callback_.Get(),
                             autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS);
}

TEST_F(ServiceRequestSenderImplTest,
       AuthRequestFallsBackToApiKeyIfFetchingAccessTokenFails) {
  EXPECT_CALL(mock_access_token_fetcher_, OnFetchAccessToken)
      .Times(1)
      .WillOnce(
          RunOnceCallback<0>(/*success = */ false, /*access_token = */ ""));

  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ &mock_access_token_fetcher_,
      std::move(cup_factory),
      std::move(loader_factory),
      /* api_key = */ std::string("fake_api_key"),
      /* auth_enabled = */ true,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(GURL("https://www.example.com"),
                             std::string("request"),
                             mock_response_callback_.Get(),
                             autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS);
}

TEST_F(ServiceRequestSenderImplTest,
       DoesNotCreateInstanceWhenFeatureNotEnabled) {
  InitCupFeatures(false, false);
  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));

  EXPECT_CALL(*cup_factory, CreateInstance(_)).Times(0);
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ nullptr,
      std::move(cup_factory),
      std::move(loader_factory),
      std::string("fake_api_key"),
      /* auth_enabled = */ false,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(
      GURL("https://www.example.com"), std::string("request"),
      mock_response_callback_.Get(), RpcType::GET_ACTIONS);
}

TEST_F(ServiceRequestSenderImplTest, SignsGetActionsRequestWhenFeatureEnabled) {
  InitCupFeatures(true, false);
  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto cup = std::make_unique<NiceMock<autofill_assistant::cup::MockCUP>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("signed_request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(std::make_unique<std::string>("response"));
      }));
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));

  EXPECT_CALL(*cup_factory,
              CreateInstance(autofill_assistant::RpcType::GET_ACTIONS))
      .WillOnce([&]() { return std::move(cup); });
  EXPECT_CALL(*cup, PackAndSignRequest("request")).WillOnce([&]() {
    return "signed_request";
  });
  EXPECT_CALL(*cup, UnpackResponse(_)).Times(0);
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ nullptr,
      std::move(cup_factory),
      std::move(loader_factory),
      std::string("fake_api_key"),
      /* auth_enabled = */ false,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(
      GURL("https://www.example.com"), std::string("request"),
      mock_response_callback_.Get(), RpcType::GET_ACTIONS);
}

TEST_F(ServiceRequestSenderImplTest, ValidatesGetActionsResponsesWhenEnabled) {
  InitCupFeatures(true, true);
  auto cup_factory =
      std::make_unique<NiceMock<autofill_assistant::cup::MockCUPFactory>>();
  auto cup = std::make_unique<NiceMock<autofill_assistant::cup::MockCUP>>();
  auto loader_factory =
      std::make_unique<NiceMock<MockSimpleURLLoaderFactory>>();
  auto loader = std::make_unique<NiceMock<MockURLLoader>>();
  auto response_info = CreateResponseInfo(net::HTTP_OK, "OK");
  EXPECT_CALL(*loader_factory, OnCreateLoader(_, _))
      .WillOnce([&](::network::ResourceRequest* resource_request,
                    const ::net::NetworkTrafficAnnotationTag& annotation_tag) {
        EXPECT_FALSE(resource_request->headers.HasHeader("Authorization"));
        EXPECT_EQ(resource_request->url,
                  GURL("https://www.example.com/?key=fake_api_key"));
        return std::move(loader);
      });
  EXPECT_CALL(*loader,
              AttachStringForUpload(std::string("signed_request"),
                                    std::string("application/x-protobuffer")))
      .Times(1);
  EXPECT_CALL(*loader, DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _))
      .WillOnce(WithArgs<1>([&](auto&& callback) {
        std::move(callback).Run(
            std::make_unique<std::string>("packed_response"));
      }));
  EXPECT_CALL(*loader, ResponseInfo)
      .WillRepeatedly(Return(response_info.get()));
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response"));

  EXPECT_CALL(*cup_factory,
              CreateInstance(autofill_assistant::RpcType::GET_ACTIONS))
      .WillOnce([&]() { return std::move(cup); });
  EXPECT_CALL(*cup, PackAndSignRequest("request")).WillOnce([&]() {
    return "signed_request";
  });
  EXPECT_CALL(*cup, UnpackResponse("packed_response")).WillOnce([&]() {
    return "response";
  });
  ServiceRequestSenderImpl request_sender{
      &context_,
      /* access_token_fetcher = */ nullptr,
      std::move(cup_factory),
      std::move(loader_factory),
      std::string("fake_api_key"),
      /* auth_enabled = */ false,
      /* disable_auth_if_no_access_token = */ true};
  request_sender.SendRequest(
      GURL("https://www.example.com"), std::string("request"),
      mock_response_callback_.Get(), autofill_assistant::RpcType::GET_ACTIONS);
}

// TODO(b/170934170): Add tests for full unit test coverage of
// service_request_sender.

}  // namespace
}  // namespace autofill_assistant
