// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Optional;

namespace safe_browsing {

namespace {
constexpr char kTestOhttpKey[] = "TestOhttpKey";
constexpr char kEncodedTestOhttpKey[] = "VGVzdE9odHRwS2V5";
constexpr char kTestOldOhttpKey[] = "OldOhttpKey";
constexpr char kTestNewOhttpKey[] = "NewOhttpKey";
constexpr char kExpectedKeyFetchServerUrl[] =
    "https://safebrowsingohttpgateway.googleapis.com/v1/ohttp/hpkekeyconfig";

scoped_refptr<net::HttpResponseHeaders> CreateSuccessHeaders() {
  return net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
}

scoped_refptr<net::HttpResponseHeaders> CreateKeyRotatedHeaders() {
  return net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-OhttpPublickey-Rotated: yes\r\n");
}

}  // namespace

class OhttpKeyServiceTest : public ::testing::Test {
 public:
  OhttpKeyServiceTest() {
    feature_list_.InitAndDisableFeature(kSafeBrowsingLookupMechanismExperiment);
  }

  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    ohttp_key_service_ = std::make_unique<OhttpKeyService>(
        test_shared_loader_factory_, &pref_service_);
  }

  void TearDown() override { ohttp_key_service_->Shutdown(); }

 protected:
  void SetupSuccessResponse() {
    test_url_loader_factory_->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& resource_request) {
          ASSERT_EQ(kExpectedKeyFetchServerUrl, resource_request.url.spec());
          ASSERT_EQ(network::mojom::CredentialsMode::kOmit,
                    resource_request.credentials_mode);
        }));
    test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                          kTestOhttpKey);
  }

  // Set the current old key in memory, and a pending new key in url_loader. So
  // the next time the key is fetched, a new key will be returned.
  void SetupOldKeyAndPendingNewKey() {
    // Set the expiration time a little longer so the async workflow doesn't
    // update the key.
    ohttp_key_service_->set_ohttp_key_for_testing(
        {kTestOldOhttpKey, base::Time::Now() + base::Days(6)});
    test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                          kTestNewOhttpKey);
  }

  void FastForwardAndVerifyKeyValue(const std::string& expected_key_value) {
    // Key fetch triggered by server has a random delay up to 1 minute.
    // Wait for 1 minute to make sure the key fetch is completed.
    task_environment_.FastForwardBy(base::Minutes(1));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->key,
              expected_key_value);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<OhttpKeyService> ohttp_key_service_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Success) {
  SetupSuccessResponse();
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service_->get_ohttp_key_for_testing();
  EXPECT_TRUE(ohttp_key.has_value());
  EXPECT_EQ(ohttp_key.value().expiration, base::Time::Now() + base::Days(7));
  EXPECT_EQ(ohttp_key.value().key, kTestOhttpKey);
  EXPECT_EQ(pref_service_.GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
  EXPECT_EQ(pref_service_.GetTime(
                prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime),
            base::Time::Now() + base::Days(7));
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Failure) {
  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey, net::HTTP_FORBIDDEN);
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service_->get_ohttp_key_for_testing();
  // The key should not be cached if key fetch fails.
  EXPECT_FALSE(ohttp_key.has_value());
  EXPECT_EQ(pref_service_.GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            "");
  EXPECT_EQ(pref_service_.GetTime(
                prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime),
            base::Time());
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Backoff) {
  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey, net::HTTP_FORBIDDEN);
  // Wait for 2 minutes so the async workflow triggers the backoff mode.
  task_environment_.FastForwardBy(base::Minutes(2));
  task_environment_.RunUntilIdle();
  SetupSuccessResponse();
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  // Although the success response is set up, the key is empty because the
  // service is in backoff mode.
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_MultipleRequests) {
  base::MockCallback<OhttpKeyService::Callback> response_callback1;
  base::MockCallback<OhttpKeyService::Callback> response_callback2;
  EXPECT_CALL(response_callback1, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  EXPECT_CALL(response_callback2, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback1.Get());
  ohttp_key_service_->GetOhttpKey(response_callback2.Get());
  task_environment_.RunUntilIdle();

  SetupSuccessResponse();
  task_environment_.RunUntilIdle();
  // url_loader should only send one request
  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_WithValidCache) {
  SetupSuccessResponse();
  ohttp_key_service_->set_ohttp_key_for_testing(
      {kTestOldOhttpKey, base::Time::Now() + base::Hours(1)});

  base::MockCallback<OhttpKeyService::Callback> response_callback;
  // Should return the old key because it has not expired.
  EXPECT_CALL(response_callback, Run(Optional(std::string(kTestOldOhttpKey))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_WithExpiredCache) {
  SetupSuccessResponse();
  ohttp_key_service_->set_ohttp_key_for_testing(
      {kTestOldOhttpKey, base::Time::Now() - base::Hours(1)});

  base::MockCallback<OhttpKeyService::Callback> response_callback1;
  // The new key should be fetched because the old key has expired.
  EXPECT_CALL(response_callback1, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback1.Get());
  task_environment_.RunUntilIdle();

  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestNewOhttpKey);
  task_environment_.FastForwardBy(base::Days(5));
  base::MockCallback<OhttpKeyService::Callback> response_callback2;
  // The new key should not be fetched because the old key has not expired.
  EXPECT_CALL(response_callback2, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback2.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Disabled) {
  SetupSuccessResponse();
  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::NO_SAFE_BROWSING);
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, PopulateKeyFromPref_ValidKey) {
  pref_service_.SetString(prefs::kSafeBrowsingHashRealTimeOhttpKey,
                          kEncodedTestOhttpKey);
  pref_service_.SetTime(prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime,
                        base::Time::Now() + base::Days(10));

  auto ohttp_key_service = std::make_unique<OhttpKeyService>(
      test_shared_loader_factory_, &pref_service_);

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service->get_ohttp_key_for_testing();
  EXPECT_TRUE(ohttp_key.has_value());
  EXPECT_EQ(ohttp_key.value().expiration, base::Time::Now() + base::Days(10));
  EXPECT_EQ(ohttp_key.value().key, kTestOhttpKey);

  pref_service_.SetTime(prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime,
                        base::Time::Now() - base::Days(10));

  ohttp_key_service = std::make_unique<OhttpKeyService>(
      test_shared_loader_factory_, &pref_service_);
  ohttp_key = ohttp_key_service->get_ohttp_key_for_testing();
  EXPECT_FALSE(ohttp_key.has_value());
}

TEST_F(OhttpKeyServiceTest, PopulateKeyFromPref_EmptyKey) {
  pref_service_.SetTime(prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime,
                        base::Time::Now() + base::Days(10));

  auto ohttp_key_service = std::make_unique<OhttpKeyService>(
      test_shared_loader_factory_, &pref_service_);

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service->get_ohttp_key_for_testing();
  EXPECT_FALSE(ohttp_key.has_value());
}

TEST_F(OhttpKeyServiceTest, AsyncFetch) {
  SetupSuccessResponse();

  task_environment_.RunUntilIdle();
  auto original_expiration = base::Time::Now() + base::Days(7);
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  task_environment_.FastForwardBy(base::Days(5));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  task_environment_.FastForwardBy(base::Days(1));
  task_environment_.RunUntilIdle();
  // OHTTP key is extended by async fetch.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration + base::Days(6));
}

TEST_F(OhttpKeyServiceTest, AsyncFetch_PrefChanges) {
  SetupSuccessResponse();

  task_environment_.RunUntilIdle();
  auto original_expiration = base::Time::Now() + base::Days(7);
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::NO_SAFE_BROWSING);
  task_environment_.FastForwardBy(base::Days(6));
  task_environment_.RunUntilIdle();
  // The expiration is not extended because the service is disabled.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::ENHANCED_PROTECTION);
  task_environment_.FastForwardBy(base::Days(6));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  task_environment_.RunUntilIdle();
  // The service is re-enabled, so the expiration date is updated.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            base::Time::Now() + base::Days(7));
}

TEST_F(OhttpKeyServiceTest, AsyncFetch_Backoff) {
  auto forward_and_check = [this](base::TimeDelta forward,
                                  absl::optional<std::string> expected_key) {
    task_environment_.FastForwardBy(forward);
    task_environment_.RunUntilIdle();
    ASSERT_EQ(expected_key.has_value(),
              ohttp_key_service_->get_ohttp_key_for_testing().has_value());
    if (expected_key) {
      EXPECT_EQ(expected_key.value(),
                ohttp_key_service_->get_ohttp_key_for_testing()->key);
    }
  };

  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey, net::HTTP_FORBIDDEN);

  // Try to fetch a new key three times in a row with the minimum wait time
  // before entering backoff mode.
  forward_and_check(base::Minutes(0), /*expected_key=*/absl::nullopt);
  forward_and_check(base::Minutes(1), /*expected_key=*/absl::nullopt);
  forward_and_check(base::Minutes(1), /*expected_key=*/absl::nullopt);

  // Enter the backoff mode, wait for 5 minutes before retrying.
  forward_and_check(base::Minutes(5), /*expected_key=*/absl::nullopt);
  // Try two more times after with the minimum wait time after exiting the
  // backoff mode.
  forward_and_check(base::Minutes(1), /*expected_key=*/absl::nullopt);
  forward_and_check(base::Minutes(1), /*expected_key=*/absl::nullopt);

  // Set up a successful response.
  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey);
  // Enter the backoff mode again with a longer duration.
  forward_and_check(base::Minutes(9), /*expected_key=*/absl::nullopt);
  // The key is succesfully fetched after exiting the backoff mode.
  forward_and_check(base::Minutes(1), /*expected_key=*/kTestOhttpKey);

  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestNewOhttpKey);
  // After exiting the backoff mode, a new key should be fetched based on the
  // key expiration date.
  forward_and_check(base::Days(5), /*expected_key=*/kTestOhttpKey);
  forward_and_check(base::Days(1),
                    /*expected_key=*/kTestNewOhttpKey);
}

TEST_F(OhttpKeyServiceTest, AsyncFetch_RescheduledBasedOnBackoffRemainingTime) {
  SetupSuccessResponse();

  // The next async fetch is set to 1 hour. Attempt to make the service enter
  // the backoff mode at 55 minutes 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(55 * 60 + 30));
  task_environment_.RunUntilIdle();
  ohttp_key_service_->set_ohttp_key_for_testing(
      {kTestOldOhttpKey, base::Time::Now() - base::Hours(1)});
  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey, net::HTTP_FORBIDDEN);
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(3);
  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();

  SetupSuccessResponse();
  // Async fetch is triggered, but the service is in backoff mode, so it is
  // rescheduled in 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(4 * 60 + 30));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->key,
            kTestOldOhttpKey);

  // The service exits the backoff mode and a new key is fetched.
  task_environment_.FastForwardBy(base::Seconds(30));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->key,
            kTestOhttpKey);
}

TEST_F(OhttpKeyServiceTest, NotifyLookupResponse_SuccessFetch) {
  SetupOldKeyAndPendingNewKey();
  ohttp_key_service_->NotifyLookupResponse(kTestOldOhttpKey, net::HTTP_OK,
                                           CreateSuccessHeaders());
  FastForwardAndVerifyKeyValue(kTestOldOhttpKey);
}

TEST_F(OhttpKeyServiceTest, NotifyLookupResponse_HeaderHint) {
  SetupOldKeyAndPendingNewKey();
  ohttp_key_service_->NotifyLookupResponse(kTestOldOhttpKey, net::HTTP_OK,
                                           CreateKeyRotatedHeaders());
  // Header hint is soft failure, the key is not immediately cleared.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->key,
            kTestOldOhttpKey);

  FastForwardAndVerifyKeyValue(kTestNewOhttpKey);
}

TEST_F(OhttpKeyServiceTest, NotifyLookupResponse_HeaderHintOnDifferentKey) {
  SetupOldKeyAndPendingNewKey();
  ohttp_key_service_->NotifyLookupResponse(kTestOhttpKey, net::HTTP_OK,
                                           CreateKeyRotatedHeaders());

  // The key is not updated because the server hint is on a different key.
  FastForwardAndVerifyKeyValue(kTestOldOhttpKey);
}

TEST_F(OhttpKeyServiceTest, NotifyLookupResponse_HeaderHintWithError) {
  SetupOldKeyAndPendingNewKey();

  ohttp_key_service_->NotifyLookupResponse(
      kTestOldOhttpKey, net::HTTP_FORBIDDEN, CreateKeyRotatedHeaders());
  // Header hint should only take effect when the response code is 200.
  FastForwardAndVerifyKeyValue(kTestOldOhttpKey);
}

TEST_F(OhttpKeyServiceTest, NotifyLookupResponse_KeyRelatedHttpFailure) {
  SetupOldKeyAndPendingNewKey();

  ohttp_key_service_->NotifyLookupResponse(
      kTestOldOhttpKey, net::HTTP_UNPROCESSABLE_CONTENT, /*headers=*/nullptr);
  // HTTP status error is a hard failure, the key should be cleared immediately.
  EXPECT_FALSE(ohttp_key_service_->get_ohttp_key_for_testing().has_value());

  FastForwardAndVerifyKeyValue(kTestNewOhttpKey);
}

TEST_F(OhttpKeyServiceTest, Shutdown) {
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  // Pending callbacks should be run during shutdown.
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  ohttp_key_service_->Shutdown();
  task_environment_.RunUntilIdle();
}

class OhttpKeyServiceLookupMechnismEnabledTest : public OhttpKeyServiceTest {
 public:
  OhttpKeyServiceLookupMechnismEnabledTest() {
    feature_list_.InitAndEnableFeature(kSafeBrowsingLookupMechanismExperiment);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OhttpKeyServiceLookupMechnismEnabledTest, AsyncFetch_PrefChanges) {
  SetupSuccessResponse();

  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::ENHANCED_PROTECTION);
  task_environment_.RunUntilIdle();
  auto original_expiration = base::Time::Now() + base::Days(7);
  // New key should be fetched because the service is enabled when enhanced
  // protection and the lookup mechanism experiment are both enabled.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);

  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  task_environment_.FastForwardBy(base::Days(6));
  task_environment_.RunUntilIdle();

  // The expiration is not extended because the service is disabled when
  // standard protection and the experiment are both enabled.
  EXPECT_EQ(ohttp_key_service_->get_ohttp_key_for_testing()->expiration,
            original_expiration);
}

}  // namespace safe_browsing
