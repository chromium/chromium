// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::InSequence;

namespace {

const int64_t kTimeoutMs = 10000;
const char kPostData[] = "{\"preferences\":{\"price_track_email\":true}}";

}  // namespace

namespace commerce {

class SpyAccountChecker : public AccountChecker {
 public:
  SpyAccountChecker(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : AccountChecker(pref_service,
                       identity_manager,
                       std::move(url_loader_factory)) {}
  SpyAccountChecker(const SpyAccountChecker&) = delete;
  SpyAccountChecker operator=(const SpyAccountChecker&) = delete;
  ~SpyAccountChecker() override = default;

  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const std::string& oauth_consumer_name,
               const GURL& url,
               const std::string& http_method,
               const std::string& content_type,
               const std::vector<std::string>& scopes,
               int64_t timeout_ms,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag),
              (override));

 protected:
  friend class AccountCheckerTest;
};

class AccountCheckerTest : public testing::Test {
 public:
  AccountCheckerTest() = default;
  ~AccountCheckerTest() override = default;

  void SetUp() override {
    test_features_.InitAndEnableFeature(kShoppingList);
    RegisterPrefs(pref_service_.registry());
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    account_checker_ = std::make_unique<SpyAccountChecker>(
        &pref_service_, identity_test_env_.identity_manager(),
        std::move(test_url_loader_factory));
    ASSERT_EQ(false, account_checker_->IsSignedIn());

    ON_CALL(*account_checker_, CreateEndpointFetcher).WillByDefault([this]() {
      return std::move(fetcher_);
    });
  }

  void SetFetchResponse(std::string response) {
    fetcher_.reset();
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    fetcher_->SetFetchResponse(response);
  }

  void FetchPriceEmailPref() { account_checker_->FetchPriceEmailPref(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList test_features_;
  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  std::unique_ptr<SpyAccountChecker> account_checker_;
};

TEST_F(AccountCheckerTest, TestFetchWaaStatusOnAccountChanged) {
  const char waa_oauth_name[] = "web_history";
  const char waa_query_url[] =
      "https://history.google.com/history/api/lookup?client=web_app";
  const char waa_oauth_scope[] = "https://www.googleapis.com/auth/chromesync";
  const char waa_content_type[] = "application/json; charset=UTF-8";
  const char waa_get_method[] = "GET";
  const int64_t waa_timeout_ms = 30000;
  const char waa_post_data[] = "";

  EXPECT_CALL(*account_checker_,
              CreateEndpointFetcher(waa_oauth_name, GURL(waa_query_url),
                                    waa_get_method, waa_content_type,
                                    std::vector<std::string>{waa_oauth_scope},
                                    waa_timeout_ms, waa_post_data, _))
      .Times(1);

  ASSERT_EQ(true, account_checker_->IsWebAndAppActivityEnabled());
  SetFetchResponse("{ \"history_recording_enabled\": false }");
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSync);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, account_checker_->IsSignedIn());
  ASSERT_EQ(false, account_checker_->IsWebAndAppActivityEnabled());
}

TEST_F(AccountCheckerTest, TestFetchPriceEmailPref) {
  {
    InSequence s;
    // Fetch Waa.
    EXPECT_CALL(*account_checker_, CreateEndpointFetcher);
    // Fetch email pref.
    EXPECT_CALL(*account_checker_,
                CreateEndpointFetcher(kOAuthName, GURL(kNotificationsPrefUrl),
                                      kGetHttpMethod, kContentType,
                                      std::vector<std::string>{kOAuthScope},
                                      kTimeoutMs, kEmptyPostData, _));
  }

  ASSERT_EQ(false, pref_service_.GetBoolean(kPriceEmailNotificationsEnabled));
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSync);
  SetFetchResponse("{ \"preferences\": { \"price_track_email\" : true } }");
  FetchPriceEmailPref();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, pref_service_.GetBoolean(kPriceEmailNotificationsEnabled));
}

TEST_F(AccountCheckerTest, TestSendPriceEmailPrefOnPrefChange) {
  {
    InSequence s;
    // Fetch Waa.
    EXPECT_CALL(*account_checker_, CreateEndpointFetcher);
    // Send email pref.
    EXPECT_CALL(*account_checker_,
                CreateEndpointFetcher(kOAuthName, GURL(kNotificationsPrefUrl),
                                      kPostHttpMethod, kContentType,
                                      std::vector<std::string>{kOAuthScope},
                                      kTimeoutMs, kPostData, _));
  }

  ASSERT_EQ(false, pref_service_.GetBoolean(kPriceEmailNotificationsEnabled));
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSync);
  SetFetchResponse("{ \"preferences\": { \"price_track_email\" : true } }");
  pref_service_.SetBoolean(kPriceEmailNotificationsEnabled, true);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, pref_service_.GetBoolean(kPriceEmailNotificationsEnabled));
}

}  // namespace commerce
