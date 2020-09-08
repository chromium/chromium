// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/url_lookup_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"

#if defined(OS_ANDROID)
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#endif

using ::testing::_;

namespace safe_browsing {

namespace {
constexpr char kTestEmail[] = "test@example.com";
constexpr char kRealTimeLookupUrlPrefix[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/realtime";
}  // namespace

class RealTimeUrlLookupServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(test_pref_service_.registry());
    task_environment_ = CreateTestTaskEnvironment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */,
        false /* restore_session */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get());

    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    rt_service_ = std::make_unique<RealTimeUrlLookupService>(
        test_shared_loader_factory_, cache_manager_.get(),
        identity_test_env_->identity_manager(), &test_sync_service_,
        &test_pref_service_, ChromeUserPopulation::NOT_MANAGED,
        /*is_under_advanced_protection=*/true,
        /*is_off_the_record=*/false, /*variations_service=*/nullptr);
  }

  void TearDown() override {
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  bool CanCheckUrl(const GURL& url) {
    return RealTimeUrlLookupServiceBase::CanCheckUrl(url);
  }
  void HandleLookupError() { rt_service_->HandleLookupError(); }
  void HandleLookupSuccess() { rt_service_->HandleLookupSuccess(); }
  bool IsInBackoffMode() { return rt_service_->IsInBackoffMode(); }
  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url) {
    return rt_service_->FillRequestProto(url);
  }
  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url) {
    return rt_service_->GetCachedRealTimeUrlVerdict(url);
  }

  void MayBeCacheRealTimeUrlVerdict(
      GURL url,
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    new_threat_info->set_verdict_type(verdict_type);
    new_threat_info->set_threat_type(threat_type);
    new_threat_info->set_cache_duration_sec(cache_duration_sec);
    new_threat_info->set_cache_expression_using_match_type(cache_expression);
    new_threat_info->set_cache_expression_match_type(
        cache_expression_match_type);
    rt_service_->MayBeCacheRealTimeUrlVerdict(url, response);
  }

  void SetUpRTLookupResponse(
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    threat_info.set_verdict_type(verdict_type);
    threat_info.set_threat_type(threat_type);
    threat_info.set_cache_duration_sec(cache_duration_sec);
    threat_info.set_cache_expression_using_match_type(cache_expression);
    threat_info.set_cache_expression_match_type(cache_expression_match_type);
    *new_threat_info = threat_info;
    std::string expected_response_str;
    response.SerializeToString(&expected_response_str);
    test_url_loader_factory_.AddResponse(kRealTimeLookupUrlPrefix,
                                         expected_response_str);
  }

  RealTimeUrlLookupService* rt_service() { return rt_service_.get(); }

  void EnableRealTimeUrlLookup(bool is_with_token_enabled) {
    unified_consent::UnifiedConsentService::RegisterPrefs(
        test_pref_service_.registry());
    test_pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        std::make_unique<base::Value>(true));
#if defined(OS_ANDROID)
    int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
    int memory_size_threshold = system_memory_size - 1;
    if (is_with_token_enabled) {
      feature_list_.InitWithFeaturesAndParameters(
          /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                                   { {
                                     kRealTimeUrlLookupMemoryThresholdMb,
                                     base::NumberToString(memory_size_threshold)
                                   } }},
                                  { kRealTimeUrlLookupEnabledWithToken,
                                    {} }},
          /* disabled_features */ {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          /* enabled_features */ {{
            kRealTimeUrlLookupEnabled,
            {
              { kRealTimeUrlLookupMemoryThresholdMb,
                base::NumberToString(memory_size_threshold) }
            }
          }},
          /* disabled_features */ {});
    }
#else
    if (is_with_token_enabled) {
      feature_list_.InitWithFeatures(
          {kRealTimeUrlLookupEnabled, kRealTimeUrlLookupEnabledWithToken}, {});
    } else {
      feature_list_.InitWithFeatures({kRealTimeUrlLookupEnabled},
                                     {kRealTimeUrlLookupEnabledWithToken});
    }
#endif
  }

  void SetupPrimaryAccount() {
    identity_test_env_->MakeUnconsentedPrimaryAccountAvailable(kTestEmail);
  }

  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      std::string token) {
    identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        token, base::Time::Max());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RealTimeUrlLookupService> rt_service_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  syncer::TestSyncService test_sync_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RealTimeUrlLookupServiceTest, TestFillRequestProto) {
  struct SanitizeUrlCase {
    const char* url;
    const char* expected_url;
  } sanitize_url_cases[] = {
      {"http://example.com/", "http://example.com/"},
      {"http://user:pass@example.com/", "http://example.com/"},
      {"http://%123:bar@example.com/", "http://example.com/"},
      {"http://example.com#123", "http://example.com/"}};
  for (size_t i = 0; i < base::size(sanitize_url_cases); i++) {
    GURL url(sanitize_url_cases[i].url);
    auto result = FillRequestProto(url);
    EXPECT_EQ(sanitize_url_cases[i].expected_url, result->url());
    EXPECT_EQ(RTLookupRequest::NAVIGATION, result->lookup_type());
    EXPECT_EQ(ChromeUserPopulation::SAFE_BROWSING,
              result->population().user_population());
    EXPECT_TRUE(result->population().is_history_sync_enabled());
    EXPECT_EQ(ChromeUserPopulation::NOT_MANAGED,
              result->population().profile_management_status());
#if BUILDFLAG(FULL_SAFE_BROWSING)
    EXPECT_TRUE(result->population().is_under_advanced_protection());
#endif
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffAndTimerReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 299 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(298));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 300 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffAndLookupSuccessReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestExponentialBackoff) {
  ///////////////////////////////
  // Initial backoff: 300 seconds
  ///////////////////////////////

  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 299 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(298));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 300 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  /////////////////////////////////////
  // Exponential backoff 1: 600 seconds
  /////////////////////////////////////

  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 599 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(598));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 600 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  //////////////////////////////////////
  // Exponential backoff 2: 1200 seconds
  //////////////////////////////////////

  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1199 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1198));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 1200 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  ///////////////////////////////////////////////////
  // Exponential backoff 3: 1800 seconds (30 minutes)
  ///////////////////////////////////////////////////

  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1799 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1798));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 1800 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  ///////////////////////////////////////////////////
  // Exponential backoff 4: 1800 seconds (30 minutes)
  ///////////////////////////////////////////////////

  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1799 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1798));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 1800 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestExponentialBackoffWithResetOnSuccess) {
  ///////////////////////////////
  // Initial backoff: 300 seconds
  ///////////////////////////////

  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 299 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(298));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 300 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  /////////////////////////////////////
  // Exponential backoff 1: 600 seconds
  /////////////////////////////////////

  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 599 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(598));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 600 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  // The next lookup is a success. This should reset the backoff duration to
  // |kMinBackOffResetDurationInSeconds|
  HandleLookupSuccess();

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 299 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(298));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 300 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestGetSBThreatTypeForRTThreatType) {
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::WEB_MALWARE));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING));
  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE));
  EXPECT_EQ(SB_THREAT_TYPE_BILLING,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNCLEAR_BILLING));
}

TEST_F(RealTimeUrlLookupServiceTest, TestCanCheckUrl) {
  struct CanCheckUrlCases {
    const char* url;
    bool can_check;
  } can_check_url_cases[] = {{"ftp://example.test/path", false},
                             {"http://localhost/path", false},
                             {"http://localhost.localdomain/path", false},
                             {"http://127.0.0.1/path", false},
                             {"http://127.0.0.1:2222/path", false},
                             {"http://192.168.1.1/path", false},
                             {"http://172.16.2.2/path", false},
                             {"http://10.1.1.1/path", false},
                             {"http://10.1.1.1.1/path", true},
                             {"http://example.test/path", true},
                             {"https://example.test/path", true}};
  for (size_t i = 0; i < base::size(can_check_url_cases); i++) {
    GURL url(can_check_url_cases[i].url);
    bool expected_can_check = can_check_url_cases[i].can_check;
    EXPECT_EQ(expected_can_check, CanCheckUrl(url));
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestCacheNotInCacheManager) {
  GURL url("https://a.example.test/path1/path2");
  ASSERT_EQ(nullptr, GetCachedRealTimeUrlVerdict(url));
}

TEST_F(RealTimeUrlLookupServiceTest, TestCacheInCacheManager) {
  GURL url("https://a.example.test/path1/path2");
  MayBeCacheRealTimeUrlVerdict(url, RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "a.example.test/path1/path2",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_->RunUntilIdle();

  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  ASSERT_NE(nullptr, cache_response);
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_response->threat_info(0).verdict_type());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
            cache_response->threat_info(0).threat_type());
}

TEST_F(RealTimeUrlLookupServiceTest, TestStartLookup_ResponseIsAlreadyCached) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup(/* is_with_token_enabled */ false);
  GURL url("http://example.test/");
  MayBeCacheRealTimeUrlVerdict(url, RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_->RunUntilIdle();

  base::MockCallback<RTLookupRequestCallback> request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  rt_service()->StartLookup(url, request_callback.Get(),
                            response_callback.Get());

  // |request_callback| should not be called.
  EXPECT_CALL(request_callback, Run(_, _)).Times(0);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ true, _));

  task_environment_->RunUntilIdle();

  // This metric is not recorded because the response is obtained from the
  // cache.
  histograms.ExpectUniqueSample("SafeBrowsing.RT.ThreatInfoSize",
                                /* sample */ 0,
                                /* expected_count */ 0);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_AttachTokenWhenWithTokenIsEnabled) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup(/* is_with_token_enabled */ true);
  SetupPrimaryAccount();
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  rt_service()->StartLookup(
      url,
      base::BindOnce(
          [](std::unique_ptr<RTLookupRequest> request, std::string token) {
            EXPECT_FALSE(request->has_dm_token());
            // Check token is attached.
            EXPECT_EQ("access_token_string", token);
          }),
      response_callback.Get());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_string");
  task_environment_->RunUntilIdle();

  // Check the response is cached.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  EXPECT_NE(nullptr, cache_response);

  histograms.ExpectUniqueSample("SafeBrowsing.RT.ThreatInfoSize",
                                /* sample */ 1,
                                /* expected_count */ 1);
}

TEST_F(RealTimeUrlLookupServiceTest, TestStartLookup_NoTokenWhenNotSignedIn) {
  EnableRealTimeUrlLookup(/* is_with_token_enabled */ true);
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  rt_service()->StartLookup(
      url,
      base::BindOnce(
          [](std::unique_ptr<RTLookupRequest> request, std::string token) {
            // Check the token field is empty.
            EXPECT_EQ("", token);
          }),
      response_callback.Get());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  task_environment_->RunUntilIdle();

  // Check the response is cached.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  EXPECT_NE(nullptr, cache_response);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_NoTokenWhenWithTokenIsDisabled) {
  EnableRealTimeUrlLookup(/* is_with_token_enabled */ false);
  SetupPrimaryAccount();
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  rt_service()->StartLookup(
      url,
      base::BindOnce(
          [](std::unique_ptr<RTLookupRequest> request, std::string token) {
            // Check the token field is empty.
            EXPECT_EQ("", token);
          }),
      response_callback.Get());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  task_environment_->RunUntilIdle();

  // Check the response is cached.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  EXPECT_NE(nullptr, cache_response);
}

}  // namespace safe_browsing
