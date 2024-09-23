// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"

#include <optional>
#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const int64_t kMockTimestamp = 123456;
const std::string kMockId1 = "111";
const std::string kMockId2 = "222";
const std::string kMockOfferId = "333";
const long kMockPrice = 100;
const std::string kMockCountry = "us";
const std::string kMockLocale = "en-US";

const char kServiceUrlForManage[] =
    "https://memex-pa.googleapis.com/v1/shopping/subscriptions"
    "?requestSnapshotParams.subscriptionType=PRICE_TRACK";
const char kServiceUrlForGet[] =
    "https://memex-pa.googleapis.com/v1/shopping/subscriptions"
    "?requestParams.subscriptionType=PRICE_TRACK";

const std::string kExpectedPostDataForCreate =
    "{\"createShoppingSubscriptionsParams\":{\"subscriptions\":[{"
    "\"identifier\":\"111\",\"identifierType\":\"PRODUCT_CLUSTER_ID\","
    "\"managementType\":\"USER_MANAGED\",\"type\":"
    "\"PRICE_TRACK\"},{\"identifier\":\"222\",\"identifierType\":\"PRODUCT_"
    "CLUSTER_ID\",\"managementType\":\"USER_MANAGED\","
    "\"type\":\"PRICE_TRACK\",\"userSeenOffer\":{\"countryCode\":\"us\","
    "\"languageCode\":\"en-US\","
    "\"offerId\":\"333\","
    "\"seenPriceMicros\":\"100\"}}]}}";
const std::string kExpectedPostDataForDelete =
    "{\"removeShoppingSubscriptionsParams\":{\"eventTimestampMicros\":["
    "\"123456\"]}}";
const std::string kResponseSucceeded =
    "{ \"status\": { \"code\": 0 },  \"subscriptions\":[{"
    "\"identifier\":\"111\",\"identifierType\":\"PRODUCT_CLUSTER_ID\","
    "\"managementType\":\"USER_MANAGED\",\"type\":"
    "\"PRICE_TRACK\",\"eventTimestampMicros\":\"123456\"}]}";
const std::string kResponseFailed = "{ \"status\": { \"code\": 1 } }";
const std::string kValidGetResponse =
    "{\"subscriptions\":[{"
    "\"identifier\":\"111\",\"identifierType\":\"PRODUCT_CLUSTER_ID\","
    "\"managementType\":\"USER_MANAGED\",\"type\":"
    "\"PRICE_TRACK\",\"eventTimestampMicros\":\"123456\"}]}";

// Build a subscription list consisting of two subscriptions.
std::unique_ptr<std::vector<commerce::CommerceSubscription>>
BuildValidSubscriptions() {
  auto subscriptions =
      std::make_unique<std::vector<commerce::CommerceSubscription>>();
  // The first one has a valid timestamp but doesn't contain a UserSeenOffer.
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId1,
      commerce::ManagementType::kUserManaged, kMockTimestamp));
  // The second one contains a UserSeenOffer but doesn't have a valid timestamp.
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId2,
      commerce::ManagementType::kUserManaged,
      commerce::kUnknownSubscriptionTimestamp,
      std::make_optional<commerce::UserSeenOffer>(kMockOfferId, kMockPrice,
                                                  kMockCountry, kMockLocale)));
  return subscriptions;
}

// Build an empty subscription list.
std::unique_ptr<std::vector<commerce::CommerceSubscription>>
BuildEmptySubscriptions() {
  return std::make_unique<std::vector<commerce::CommerceSubscription>>();
}

}  // namespace

namespace commerce {

class SpySubscriptionsServerProxy : public SubscriptionsServerProxy {
 public:
  SpySubscriptionsServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : SubscriptionsServerProxy(identity_manager,
                                 std::move(url_loader_factory)) {}
  SpySubscriptionsServerProxy(const SpySubscriptionsServerProxy&) = delete;
  SpySubscriptionsServerProxy operator=(const SpySubscriptionsServerProxy&) =
      delete;
  ~SpySubscriptionsServerProxy() override = default;

  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
               const std::string& http_method,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag),
              (override));
};

class SubscriptionsServerProxyTest : public testing::Test {
 public:
  SubscriptionsServerProxyTest() = default;
  ~SubscriptionsServerProxyTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        commerce::kPriceTrackingSubscriptionServiceLocaleKey);
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    server_proxy_ = std::make_unique<SpySubscriptionsServerProxy>(
        identity_test_env_.identity_manager(),
        std::move(test_url_loader_factory));
    ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([this]() {
      return std::move(fetcher_);
    });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  std::unique_ptr<SpySubscriptionsServerProxy> server_proxy_;
};

TEST_F(SubscriptionsServerProxyTest, TestCreate) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Create(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId1, subscription.id);
            ASSERT_EQ(kMockTimestamp, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_EmptyList) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  EXPECT_CHECK_DEATH(server_proxy_->Create(
      BuildEmptySubscriptions(),
      base::BindOnce(
          [](SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
          })));
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_ServerFailed) {
  fetcher_->SetFetchResponse(kResponseFailed);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Create(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerInternalError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_WrongHttpCode) {
  fetcher_->SetFetchResponse(kResponseSucceeded, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Create(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerParseError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_EmptyResponse) {
  fetcher_->SetFetchResponse("");
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Create(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerInternalError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestDelete) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForDelete, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Delete(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId1, subscription.id);
            ASSERT_EQ(kMockTimestamp, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestDelete_EmptyList) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  EXPECT_CHECK_DEATH(server_proxy_->Delete(
      BuildEmptySubscriptions(),
      base::BindOnce(
          [](SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
          })));
}

TEST_F(SubscriptionsServerProxyTest, TestDelete_ServerFailed) {
  fetcher_->SetFetchResponse(kResponseFailed);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForManage), kPostHttpMethod,
                                    kExpectedPostDataForDelete, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Delete(
      BuildValidSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerInternalError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet) {
  fetcher_->SetFetchResponse(kValidGetResponse);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId1, subscription.id);
            ASSERT_EQ(kMockTimestamp, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_WrongType) {
  fetcher_->SetFetchResponse(kValidGetResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kTypeUnspecified,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kInvalidArgument, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_WrongHttpCode) {
  fetcher_->SetFetchResponse(kValidGetResponse, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerParseError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_FetchError) {
  fetcher_->SetFetchResponse(
      kValidGetResponse, net::HTTP_OK,
      std::make_optional<FetchErrorType>(FetchErrorType::kNetError));
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kServerParseError, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_NoSubscriptions) {
  fetcher_->SetFetchResponse("");
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace commerce
