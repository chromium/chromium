// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/verdict_cache_manager.h"

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class VerdictCacheManagerTest : public ::testing::Test {
 public:
  VerdictCacheManagerTest() {}

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    task_environment_ = CreateTestTaskEnvironment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */,
        false /* restore_session */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get());
  }

  void TearDown() override {
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  void CachePhishGuardVerdict(
      LoginReputationClientRequest::TriggerType trigger,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse::VerdictType verdict,
      int cache_duration_sec,
      const std::string& cache_expression,
      const base::Time& verdict_received_time) {
    ASSERT_FALSE(cache_expression.empty());
    LoginReputationClientResponse response;
    response.set_verdict_type(verdict);
    response.set_cache_expression(cache_expression);
    response.set_cache_duration_sec(cache_duration_sec);
    cache_manager_->CachePhishGuardVerdict(trigger, password_type, response,
                                           verdict_received_time);
  }

  void AddThreatInfoToResponse(
      RTLookupResponse& response,
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    new_threat_info->set_verdict_type(verdict_type);
    new_threat_info->set_threat_type(threat_type);
    new_threat_info->set_cache_duration_sec(cache_duration_sec);
    new_threat_info->set_cache_expression_using_match_type(cache_expression);
    new_threat_info->set_cache_expression_match_type(
        cache_expression_match_type);
  }

 protected:
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

 private:
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
};

TEST_F(VerdictCacheManagerTest, TestCanRetrieveCachedVerdict) {
  GURL url("https://www.google.com/");
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);
  LoginReputationClientResponse cached_verdict;
  cached_verdict.set_cache_expression("www.google.com/");
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestCacheSplitByTriggerType) {
  GURL url("https://www.google.com/");
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);
  LoginReputationClientResponse cached_verdict;
  cached_verdict.set_cache_expression("www.google.com/");
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));

  CachePhishGuardVerdict(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestCacheSplitByPasswordType) {
  GURL url("https://www.google.com/");
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);
  LoginReputationClientResponse cached_verdict;
  cached_verdict.set_cache_expression("www.google.com/");
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));

  password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CachePhishGuardVerdict(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestGetStoredPhishGuardVerdictCount) {
  LoginReputationClientResponse cached_verdict;
  cached_verdict.set_cache_expression("www.google.com/");
  EXPECT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  password_type.set_is_account_syncing(true);
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/path", base::Time::Now());

  EXPECT_EQ(2u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_F(VerdictCacheManagerTest, TestParseInvalidVerdictEntry) {
  // Directly save an invalid cache entry.
  LoginReputationClientResponse verdict;
  verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
  verdict.set_cache_expression("www.google.com/");
  verdict.set_cache_duration_sec(60);

  std::string verdict_serialized;
  verdict.SerializeToString(&verdict_serialized);
  base::Base64Encode(verdict_serialized, &verdict_serialized);

  auto cache_dictionary = std::make_unique<base::DictionaryValue>();
  auto* verdict_dictionary =
      cache_dictionary->SetKey("2", base::Value(base::Value::Type::DICTIONARY));
  auto* verdict_entry = verdict_dictionary->SetKey(
      "www.google.com/", base::Value(base::Value::Type::DICTIONARY));
  verdict_entry->SetStringKey("cache_creation_time", "invalid_time");
  verdict_entry->SetStringKey("verdict_proto", verdict_serialized);

  content_setting_map_->SetWebsiteSettingDefaultScope(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION, std::string(),
      std::move(cache_dictionary));

  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);

  LoginReputationClientResponse cached_verdict;
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://www.google.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestRemoveCachedVerdictOnURLsDeleted) {
  ASSERT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  // Prepare 5 verdicts. Three are for origin "http://foo.com", and the others
  // are for "http://bar.com".
  base::Time now = base::Time::Now();
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  CachePhishGuardVerdict(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CachePhishGuardVerdict(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         600, "bar.com", now);
  ASSERT_EQ(3u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  CachePhishGuardVerdict(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  CachePhishGuardVerdict(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::PHISHING,
                         600, "bar.com", now);
  ASSERT_EQ(2u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Delete a bar.com URL. Corresponding content setting keyed on
  // origin "http://bar.com" should be removed,
  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(GURL("http://bar.com")));

  // Delete an arbitrary data URL, to ensure the service is robust against
  // filtering only http/s URLs. See crbug.com/709758.
  deleted_urls.push_back(history::URLRow(GURL("data:text/html, <p>hellow")));

  cache_manager_->RemoveContentSettingsOnURLsDeleted(false /* all_history */,
                                                     deleted_urls);
  EXPECT_EQ(2u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  LoginReputationClientResponse actual_verdict;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("http://bar.com"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &actual_verdict));
  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("http://bar.com"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                password_type, &actual_verdict));

  // If delete all history. All password protection content settings should be
  // gone.
  cache_manager_->RemoveContentSettingsOnURLsDeleted(true /* all_history */,
                                                     history::URLRows());
  EXPECT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_F(VerdictCacheManagerTest, TestCleanUpExpiredVerdict) {
  // Prepare 4 verdicts for PASSWORD_REUSE_EVENT with SIGN_IN_PASSWORD type:
  // (1) "foo.com/abc/" valid
  // (2) "foo.com/def/" expired
  // (3) "bar.com/abc/" expired
  // (4) "bar.com/def/" expired
  base::Time now = base::Time::Now();
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  CachePhishGuardVerdict(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  CachePhishGuardVerdict(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 0, "foo.com/def/", now);
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/abc/", now);
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/def/", now);
  ASSERT_EQ(4u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Prepare 2 verdicts for UNFAMILIAR_LOGIN_PAGE:
  // (1) "bar.com/def/" valid
  // (2) "bar.com/xyz/" expired
  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  CachePhishGuardVerdict(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::SAFE,
                         600, "bar.com/def/", now);
  CachePhishGuardVerdict(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/xyz/", now);
  ASSERT_EQ(2u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Prepare 2 verdicts for SAFE_BROWSING_URL_CHECK_DATA:
  // (1) "www.example.com/" expired
  // (2) "www.example.com/path" valid
  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 0,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(GURL("https://www.example.com/"),
                                          response, base::Time::Now(),
                                          /* store_old_cache */ false);
  ASSERT_EQ(2u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  cache_manager_->CleanUpExpiredVerdicts();

  ASSERT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  LoginReputationClientResponse actual_verdict;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  // Has cached PASSWORD_REUSE_EVENT verdict for foo.com/abc/.
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://foo.com/abc/test.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for foo.com/def.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://foo.com/def/index.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for bar.com/abc.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://bar.com/abc/index.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for bar.com/def.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://bar.com/def/index.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &actual_verdict));

  // Has cached UNFAMILIAR_LOGIN_PAGE verdict for bar.com/def.
  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://bar.com/def/index.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                password_type, &actual_verdict));

  // No cached UNFAMILIAR_LOGIN_PAGE verdict for bar.com/xyz.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://bar.com/xyz/index.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                password_type, &actual_verdict));

  RTLookupResponse::ThreatInfo actual_real_time_threat_info;
  // No cached SAFE_BROWSING_URL_CHECK_DATA verdict for www.example.com/.
  EXPECT_EQ(
      RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
      cache_manager_->GetCachedRealTimeUrlVerdict(
          GURL("https://www.example.com/"), &actual_real_time_threat_info));
  // Has cached SAFE_BROWSING_URL_CHECK_DATA verdict for www.example.com/path.
  EXPECT_EQ(
      RTLookupResponse::ThreatInfo::DANGEROUS,
      cache_manager_->GetCachedRealTimeUrlVerdict(
          GURL("https://www.example.com/path"), &actual_real_time_threat_info));
}

TEST_F(VerdictCacheManagerTest, TestCleanUpExpiredVerdictWithInvalidEntry) {
  // Directly save an invalid cache entry.
  LoginReputationClientResponse verdict;
  verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
  verdict.set_cache_expression("www.google.com/");
  verdict.set_cache_duration_sec(60);

  std::string verdict_serialized;
  verdict.SerializeToString(&verdict_serialized);
  base::Base64Encode(verdict_serialized, &verdict_serialized);

  auto cache_dictionary = std::make_unique<base::DictionaryValue>();
  auto* verdict_dictionary =
      cache_dictionary->SetKey("1", base::Value(base::Value::Type::DICTIONARY));
  auto* verdict_entry = verdict_dictionary->SetKey(
      "www.google.com/path", base::Value(base::Value::Type::DICTIONARY));
  verdict_entry->SetStringKey("cache_creation_time", "invalid_time");
  verdict_entry->SetStringKey("verdict_proto", verdict_serialized);

  content_setting_map_->SetWebsiteSettingDefaultScope(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION, std::string(),
      std::move(cache_dictionary));

  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  // Save one valid entry
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  // Verify we saved two entries under PasswordType PRIMARY_ACCOUNT_PASSWORD
  EXPECT_EQ(2U,
            content_setting_map_
                ->GetWebsiteSetting(GURL("http://www.google.com/"), GURL(),
                                    ContentSettingsType::PASSWORD_PROTECTION,
                                    std::string(), nullptr)
                ->FindDictKey("1")
                ->DictSize());

  cache_manager_->CleanUpExpiredVerdicts();

  // One should have been cleaned up
  EXPECT_EQ(1U,
            content_setting_map_
                ->GetWebsiteSetting(GURL("http://www.google.com/"), GURL(),
                                    ContentSettingsType::PASSWORD_PROTECTION,
                                    std::string(), nullptr)
                ->FindDictKey("1")
                ->DictSize());
}

TEST_F(VerdictCacheManagerTest, TestCanRetrieveCachedRealTimeUrlCheckVerdict) {
  base::HistogramTester histograms;
  GURL url("https://www.example.com/path");

  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::SAFE,
                          RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED,
                          60, "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);

  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  EXPECT_EQ("www.example.com/path",
            out_verdict.cache_expression_using_match_type());
  EXPECT_EQ(60, out_verdict.cache_duration_sec());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
            out_verdict.threat_type());
  histograms.ExpectUniqueSample(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      /* sample */ 2, /* expected_count */ 1);
}

TEST_F(VerdictCacheManagerTest,
       TestCanRetrieveCachedRealTimeUrlCheckVerdictWithMultipleThreatInfos) {
  GURL url1("https://www.example.com/");
  GURL url2("https://www.example.com/path");

  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE, 60,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::UNCLEAR_BILLING, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url2, response, base::Time::Now(),
                                          /* store_old_cache */ false);

  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url1, &out_verdict));
  EXPECT_EQ("www.example.com/",
            out_verdict.cache_expression_using_match_type());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
            out_verdict.threat_type());

  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url2, &out_verdict));
  EXPECT_EQ("www.example.com/path",
            out_verdict.cache_expression_using_match_type());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE,
            out_verdict.threat_type());
}

TEST_F(VerdictCacheManagerTest,
       TestCannotRetrieveRealTimeUrlCheckExpiredVerdict) {
  GURL url("https://www.example.com/path");

  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 0,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);

  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
}

TEST_F(VerdictCacheManagerTest,
       TestRemoveRealTimeUrlCheckCachedVerdictOnURLsDeleted) {
  GURL url("https://www.example.com/path");

  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);
  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));

  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(GURL("https://www.example.com/path")));

  cache_manager_->RemoveContentSettingsOnURLsDeleted(false /* all_history */,
                                                     deleted_urls);
  EXPECT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
}

TEST_F(VerdictCacheManagerTest, TestHostSuffixMatching) {
  // Password protection verdict.
  GURL url("https://a.example.test/path");
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);
  LoginReputationClientResponse cached_verdict;

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         60, "example.test/path/", base::Time::Now());

  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://b.example.test/path/path2"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));

  // Real time url check verdict.
  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "example.test/path/",
                          RTLookupResponse::ThreatInfo::COVERING_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);
  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(
                GURL("https://b.example.test/path/path2"), &out_verdict));
}

TEST_F(VerdictCacheManagerTest, TestHostSuffixMatchingMostExactMatching) {
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  password_type.set_is_account_syncing(true);
  LoginReputationClientResponse cached_verdict;

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         60, "example.test/", base::Time::Now());

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "b1.b.example.test/", base::Time::Now());

  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type,
                         LoginReputationClientResponse::LOW_REPUTATION, 60,
                         "b.example.test/", base::Time::Now());

  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            cache_manager_->GetCachedPhishGuardVerdict(
                GURL("https://b1.b.example.test/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestExactMatching) {
  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "a.example.test/path1/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(
      GURL("https://a.example.test/path1/path2"), response, base::Time::Now(),
      /* store_old_cache */ false);

  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(
                GURL("https://a.example.test/path1/"), &out_verdict));
  // Since |cache_expression_exact_matching| is set to EXACT_MATCH, cache is not
  // found.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedRealTimeUrlVerdict(
                GURL("https://a.example.test/path1/path2"), &out_verdict));
}

TEST_F(VerdictCacheManagerTest, TestMatchingTypeNotSet) {
  base::HistogramTester histograms;
  std::string cache_expression = "a.example.test/path1";
  GURL url("https://a.example.test/path1");

  RTLookupResponse response;
  RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
  new_threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  new_threat_info->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  new_threat_info->set_cache_duration_sec(60);
  new_threat_info->set_cache_expression_using_match_type(cache_expression);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);

  RTLookupResponse::ThreatInfo out_verdict;
  // If |cache_expression_match_type| is not set, ignore this cache.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  histograms.ExpectBucketCount(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      /* sample */ 0, /* expected_count */ 1);

  new_threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);
  // Should be able to get the cache if |cache_expression_match_type| is set.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  histograms.ExpectBucketCount(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      /* sample */ 1, /* expected_count */ 1);
}

TEST_F(VerdictCacheManagerTest, TestReadOldRealTimeUrlCheckCacheNotCrash) {
  std::string cache_expression = "a.example.test/path1/path2";
  GURL url("https://a.example.test/path1/path2");

  // Store an old cache to disk.
  RTLookupResponse response;
  RTLookupResponse::ThreatInfo* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  threat_info->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  threat_info->set_cache_duration_sec(60);
  threat_info->set_cache_expression(cache_expression);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ true);

  RTLookupResponse::ThreatInfo out_verdict;
  // Should not crash when reading old cache.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));

  threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::SAFE);
  threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  threat_info->set_cache_expression_using_match_type(cache_expression);
  cache_manager_->CacheRealTimeUrlVerdict(url, response, base::Time::Now(),
                                          /* store_old_cache */ false);
  // Should be able to read the new cache.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SAFE,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
}

TEST_F(VerdictCacheManagerTest, TestCleanUpExpiredVerdictInBackground) {
  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 0,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);

  cache_manager_->CacheRealTimeUrlVerdict(GURL("https://www.example.com/"),
                                          response, base::Time::Now(),
                                          /* store_old_cache */ false);
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(119));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The first cleanup task should happen at 120 seconds after construction.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  cache_manager_->CacheRealTimeUrlVerdict(GURL("https://www.example.com/"),
                                          response, base::Time::Now(),
                                          /* store_old_cache */ false);
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1798));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The second cleanup task should happen at 120 + 1800 seconds after
  // construction.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  cache_manager_->CacheRealTimeUrlVerdict(GURL("https://www.example.com/"),
                                          response, base::Time::Now(),
                                          /* store_old_cache */ false);
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(1798));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The third cleanup task should happen at 120 + 1800 + 1800 seconds after
  // construction.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
}

}  // namespace safe_browsing
