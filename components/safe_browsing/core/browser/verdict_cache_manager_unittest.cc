// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/verdict_cache_manager.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_sync_observer.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using testing::SizeIs;

const char kArtificialHashRealTimeUnsafeUrl[] = "https://example.test";

class MockSafeBrowsingSyncObserver : public SafeBrowsingSyncObserver {
 public:
  MockSafeBrowsingSyncObserver() : SafeBrowsingSyncObserver() {}

  ~MockSafeBrowsingSyncObserver() override = default;

  void ObserveHistorySyncStateChanged(
      SafeBrowsingSyncObserver::Callback callback) override {
    callback_ = std::move(callback);
  }

  void OnSyncStateChanged() { callback_.Run(); }

 private:
  Callback callback_;
};

}  // namespace

class VerdictCacheManagerTest : public ::testing::Test {
 public:
  VerdictCacheManagerTest() {}

  void SetUp() override {
    test_pref_service_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingEnabled, true);
    test_pref_service_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingEnhanced, false);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session */,
        false /* should_record_metrics */);
    auto sync_observer = std::make_unique<MockSafeBrowsingSyncObserver>();
    raw_sync_observer_ = sync_observer.get();
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get(), &test_pref_service_,
        std::move(sync_observer));
  }

  void TearDown() override {
    cache_manager_->Shutdown();
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

  void CacheHashPrefixRealTimeLookupResult(int cache_duration_seconds,
                                           std::string hash_prefix) {
    V5::Duration duration;
    duration.set_seconds(cache_duration_seconds);
    cache_manager_->CacheHashPrefixRealTimeLookupResults(
        {hash_prefix}, {V5::FullHash()}, duration);
  }
  void ConfirmHashPrefixRealTimeLookupCacheContent(std::string hash_prefix,
                                                   bool should_expect_entry) {
    // We cannot call the public SearchCache function because that automatically
    // filters out expired results. We want to confirm that the cache contents
    // themselves have been cleaned up as expected, so we access |cache_|
    // directly.
    EXPECT_EQ(base::Contains(cache_manager_->hash_realtime_cache_->cache_,
                             hash_prefix),
              should_expect_entry);
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

  ChromeUserPopulation::PageLoadToken CreatePageLoadToken(
      int token_time_msec,
      std::string token_value) {
    ChromeUserPopulation::PageLoadToken token;
    token.set_token_source(
        ChromeUserPopulation::PageLoadToken::CLIENT_GENERATION);
    token.set_token_time_msec(token_time_msec);
    token.set_token_value(token_value);
    return token;
  }

 protected:
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  raw_ptr<MockSafeBrowsingSyncObserver, DanglingUntriaged> raw_sync_observer_ =
      nullptr;
};

class ArtificialHashRealTimeVerdictCacheManagerTest
    : public VerdictCacheManagerTest {
 public:
  ArtificialHashRealTimeVerdictCacheManagerTest() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        safe_browsing::kArtificialCachedHashPrefixRealTimeVerdictFlag,
        kArtificialHashRealTimeUnsafeUrl);
  }
  void TearDown() override {
    VerdictCacheManagerTest::TearDown();
    VerdictCacheManager::ResetHasArtificialCachedUrlForTesting();
  }
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
  verdict_serialized = base::Base64Encode(verdict_serialized);

  base::Value::Dict verdict_entry;
  verdict_entry.Set("cache_creation_time", "invalid_time");
  verdict_entry.Set("verdict_proto", std::move(verdict_serialized));
  base::Value::Dict verdict_dictionary;
  verdict_dictionary.Set("www.google.com/", std::move(verdict_entry));
  base::Value::Dict cache_dictionary;
  cache_dictionary.Set("2", std::move(verdict_dictionary));

  content_setting_map_->SetWebsiteSettingDefaultScope(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION,
      base::Value(std::move(cache_dictionary)));

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

// TODO(crbug.com/40203584): This test is flaky on device.
#if TARGET_OS_IOS && !TARGET_IPHONE_SIMULATOR
#define MAYBE_TestCleanUpExpiredVerdict DISABLED_TestCleanUpExpiredVerdict
#else
#define MAYBE_TestCleanUpExpiredVerdict TestCleanUpExpiredVerdict
#endif
TEST_F(VerdictCacheManagerTest, MAYBE_TestCleanUpExpiredVerdict) {
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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  ASSERT_EQ(2u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  // Prepare 2 page load tokens:
  // (1) "www.example.com" expired
  // (2) "www.example1.com" valid
  cache_manager_->SetPageLoadTokenForTesting(
      GURL("https://www.example.com"),
      CreatePageLoadToken((now - base::Hours(1)).InMillisecondsSinceUnixEpoch(),
                          "token1"));
  cache_manager_->SetPageLoadTokenForTesting(
      GURL("https://www.example1.com"),
      CreatePageLoadToken(now.InMillisecondsSinceUnixEpoch(), "token2"));

  CacheHashPrefixRealTimeLookupResult(/*cache_duration_seconds=*/0, "aaaa");
  CacheHashPrefixRealTimeLookupResult(/*cache_duration_seconds=*/300, "bbbb");
  // aaaa and bbbb should both be in the cache even though aaaa is expired.
  ConfirmHashPrefixRealTimeLookupCacheContent(/*hash_prefix=*/"aaaa",
                                              /*should_expect_entry=*/true);
  ConfirmHashPrefixRealTimeLookupCacheContent(/*hash_prefix=*/"bbbb",
                                              /*should_expect_entry=*/true);

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

  // token1 is cleaned up.
  EXPECT_FALSE(
      cache_manager_->GetPageLoadToken(GURL("https://www.example.com/"))
          .has_token_value());
  // token2 will be returned for www.example1.com.
  EXPECT_EQ("token2",
            cache_manager_->GetPageLoadToken(GURL("https://www.example1.com/"))
                .token_value());

  // aaaa is not in the cache because it was expired and has been cleaned up.
  ConfirmHashPrefixRealTimeLookupCacheContent(/*hash_prefix=*/"aaaa",
                                              /*should_expect_entry=*/false);
  // aaaa is still in the cache because it has not expired.
  ConfirmHashPrefixRealTimeLookupCacheContent(/*hash_prefix=*/"bbbb",
                                              /*should_expect_entry=*/true);
}

TEST_F(VerdictCacheManagerTest, TestCleanUpExpiredVerdictWithInvalidEntry) {
  // Directly save an invalid cache entry.
  LoginReputationClientResponse verdict;
  verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
  verdict.set_cache_expression("www.google.com/");
  verdict.set_cache_duration_sec(60);

  std::string verdict_serialized;
  verdict.SerializeToString(&verdict_serialized);
  verdict_serialized = base::Base64Encode(verdict_serialized);

  base::Value::Dict verdict_entry;
  verdict_entry.Set("cache_creation_time", "invalid_time");
  verdict_entry.Set("verdict_proto", std::move(verdict_serialized));
  base::Value::Dict verdict_dictionary;
  verdict_dictionary.Set("www.google.com/path", std::move(verdict_entry));
  base::Value::Dict cache_dictionary;
  cache_dictionary.Set("1", std::move(verdict_dictionary));

  content_setting_map_->SetWebsiteSettingDefaultScope(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION,
      base::Value(std::move(cache_dictionary)));

  ReusedPasswordAccountType password_type;
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  // Save one valid entry
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  // Verify we saved two entries under PasswordType PRIMARY_ACCOUNT_PASSWORD
  base::Value setting_entry = content_setting_map_->GetWebsiteSetting(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION, nullptr);
  const base::Value::Dict* setting_dict = setting_entry.GetIfDict();
  ASSERT_TRUE(setting_dict);
  EXPECT_THAT(*setting_dict->FindDict("1"), SizeIs(2u));

  cache_manager_->CleanUpExpiredVerdicts();

  // One should have been cleaned up
  setting_entry = content_setting_map_->GetWebsiteSetting(
      GURL("http://www.google.com/"), GURL(),
      ContentSettingsType::PASSWORD_PROTECTION, nullptr);
  ASSERT_TRUE(setting_entry.is_dict());
  setting_dict = setting_entry.GetIfDict();
  EXPECT_THAT(*setting_dict->FindDict("1"), SizeIs(1u));
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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
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

TEST_F(VerdictCacheManagerTest,
       TestCanRetrieveCachedRealTimeClientSideDetectionTypeCheck) {
  base::HistogramTester histograms;
  GURL url("https://www.example.com/path");

  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::SAFE,
                          RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED,
                          60, "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::SUSPICIOUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "www.example.com/path",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);

  response.set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::
          CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);

  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

  RTLookupResponse::ThreatInfo out_verdict;
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SUSPICIOUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  EXPECT_EQ("www.example.com/path",
            out_verdict.cache_expression_using_match_type());
  EXPECT_EQ(60, out_verdict.cache_duration_sec());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
            out_verdict.threat_type());
  EXPECT_EQ(static_cast<int>(safe_browsing::ClientSideDetectionType::
                                 CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED),
            cache_manager_->GetCachedRealTimeUrlClientSideDetectionType(url));

  GURL url2("https://www.example2.com/path2");

  RTLookupResponse response2;
  AddThreatInfoToResponse(response2, RTLookupResponse::ThreatInfo::SAFE,
                          RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED,
                          60, "www.example2.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);
  AddThreatInfoToResponse(response2, RTLookupResponse::ThreatInfo::SAFE,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                          "www.example2.com/path2",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);

  response2.set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  cache_manager_->CacheRealTimeUrlVerdict(response2, base::Time::Now());

  EXPECT_EQ(
      static_cast<int>(safe_browsing::ClientSideDetectionType::FORCE_REQUEST),
      cache_manager_->GetCachedRealTimeUrlClientSideDetectionType(url2));
}

TEST_F(VerdictCacheManagerTest, TestHostSuffixMatching) {
  // Password protection verdict.
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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

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
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

  RTLookupResponse::ThreatInfo out_verdict;
  // If |cache_expression_match_type| is not set, ignore this cache.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  histograms.ExpectBucketCount(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      /* sample */ 0, /* expected_count */ 1);

  new_threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  // Should be able to get the cache if |cache_expression_match_type| is set.
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_manager_->GetCachedRealTimeUrlVerdict(url, &out_verdict));
  histograms.ExpectBucketCount(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      /* sample */ 1, /* expected_count */ 1);
}

TEST_F(VerdictCacheManagerTest, TestCleanUpExpiredVerdictInBackground) {
  RTLookupResponse response;
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 0,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);

  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  task_environment_.FastForwardBy(base::Seconds(119));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The first cleanup task should happen at 120 seconds after construction.
  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  task_environment_.FastForwardBy(base::Seconds(1798));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The second cleanup task should happen at 120 + 1800 seconds after
  // construction.
  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());

  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  task_environment_.FastForwardBy(base::Seconds(1798));
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // The third cleanup task should happen at 120 + 1800 + 1800 seconds after
  // construction.
  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
}

TEST_F(VerdictCacheManagerTest, TestCleanUpVerdictOlderThanUpperBound) {
  RTLookupResponse response;
  // Set the cache duration to 20 days.
  AddThreatInfoToResponse(response, RTLookupResponse::ThreatInfo::DANGEROUS,
                          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                          /* cache_duration_sec */ 20 * 24 * 60 * 60,
                          "www.example.com/",
                          RTLookupResponse::ThreatInfo::EXACT_MATCH);

  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());
  ASSERT_EQ(1u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
  // Fast forward by 8 days.
  task_environment_.FastForwardBy(base::Seconds(8 * 24 * 60 * 60));
  // Although the cache duration is set to 20 days, it is stored longer than the
  // upper bound(7 days). The cache should be cleaned up.
  ASSERT_EQ(0u, cache_manager_->GetStoredRealTimeUrlCheckVerdictCount());
}

TEST_F(VerdictCacheManagerTest, TestGetPageLoadToken) {
  GURL url1("https://www.example.com/path1");
  GURL url2("http://www.example.com/path2");  // different scheme and path
  GURL url3("https://www.example1.com/path1");
  cache_manager_->CreatePageLoadToken(url1);
  ChromeUserPopulation::PageLoadToken token1 =
      cache_manager_->GetPageLoadToken(url1);
  ChromeUserPopulation::PageLoadToken token2 =
      cache_manager_->GetPageLoadToken(url2);
  // token1 and token2 are the same, because the hostname is the same.
  ASSERT_TRUE(token1.has_token_value());
  ASSERT_TRUE(token2.has_token_value());
  ASSERT_EQ(token1.token_value(), token2.token_value());

  cache_manager_->CreatePageLoadToken(url1);
  ChromeUserPopulation::PageLoadToken token3 =
      cache_manager_->GetPageLoadToken(url1);
  // token1 and token3 are different, because CreatePageLoadToken should
  // create a new token for the hostname.
  ASSERT_TRUE(token3.has_token_value());
  ASSERT_NE(token1.token_value(), token3.token_value());
  ChromeUserPopulation::PageLoadToken token4 =
      cache_manager_->GetPageLoadToken(url3);
  // token4 should be empty, because url3 has a different hostname.
  ASSERT_FALSE(token4.has_token_value());
}

TEST_F(VerdictCacheManagerTest, TestGetExpiredPageLoadToken) {
  GURL url("https://www.example.com/path");
  cache_manager_->CreatePageLoadToken(url);
  ChromeUserPopulation::PageLoadToken token =
      cache_manager_->GetPageLoadToken(url);
  ASSERT_TRUE(token.has_token_value());

  task_environment_.FastForwardBy(base::Minutes(11));
  token = cache_manager_->GetPageLoadToken(url);
  // Token is not found because it has already expired.
  ASSERT_FALSE(token.has_token_value());
}

TEST_F(VerdictCacheManagerTest, TestClearTokenOnSafeBrowsingStateChanged) {
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::STANDARD_PROTECTION);
  GURL url("https://www.example.com/path");
  cache_manager_->CreatePageLoadToken(url);
  ChromeUserPopulation::PageLoadToken token =
      cache_manager_->GetPageLoadToken(url);
  ASSERT_TRUE(token.has_token_value());

  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::NO_SAFE_BROWSING);
  token = cache_manager_->GetPageLoadToken(url);
  // Token is not found because the Safe Browsing state has changed.
  ASSERT_FALSE(token.has_token_value());
}

TEST_F(VerdictCacheManagerTest, TestClearTokenOnSyncStateChanged) {
  GURL url("https://www.example.com/path");
  cache_manager_->CreatePageLoadToken(url);
  ChromeUserPopulation::PageLoadToken token =
      cache_manager_->GetPageLoadToken(url);
  ASSERT_TRUE(token.has_token_value());

  raw_sync_observer_->OnSyncStateChanged();
  token = cache_manager_->GetPageLoadToken(url);
  // Token is not found because the sync state has changed.
  ASSERT_FALSE(token.has_token_value());
}

TEST_F(VerdictCacheManagerTest, TestShutdown) {
  cache_manager_->Shutdown();
  RTLookupResponse rt_response;
  // Call to cache_manager after shutdown should not cause a crash.
  cache_manager_->CacheRealTimeUrlVerdict(rt_response, base::Time::Now());
  RTLookupResponse::ThreatInfo out_rt_verdict;
  cache_manager_->GetCachedRealTimeUrlVerdict(
      GURL("https://www.example.com/path"), &out_rt_verdict);
  LoginReputationClientResponse pg_response;
  ReusedPasswordAccountType password_type;
  cache_manager_->CachePhishGuardVerdict(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, password_type,
      pg_response, base::Time::Now());
  cache_manager_->GetStoredPhishGuardVerdictCount(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE);
  LoginReputationClientResponse out_pg_verdict;
  cache_manager_->GetCachedPhishGuardVerdict(
      GURL("https://www.example.com/path"),
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, password_type,
      &out_pg_verdict);
}

TEST_F(VerdictCacheManagerTest, TestHashPrefixRealTimeLookupCaching) {
  // Basic test ensuring that the cache manager calls are propagating as
  // expected to the HashRealTimeCache.
  EXPECT_TRUE(
      cache_manager_->GetCachedHashPrefixRealTimeLookupResults({"aaaa", "bbbb"})
          .empty());
  CacheHashPrefixRealTimeLookupResult(/*cache_duration_seconds=*/300, "aaaa");
  CacheHashPrefixRealTimeLookupResult(/*cache_duration_seconds=*/300, "bbbb");
  auto cache_results = cache_manager_->GetCachedHashPrefixRealTimeLookupResults(
      {"aaaa", "bbbb", "cccc"});
  EXPECT_EQ(cache_results.size(), 2u);
  EXPECT_TRUE(base::Contains(cache_results, "aaaa"));
  EXPECT_TRUE(base::Contains(cache_results, "bbbb"));
}

TEST_F(ArtificialHashRealTimeVerdictCacheManagerTest, TestCachePopulated) {
  ASSERT_TRUE(VerdictCacheManager::has_artificial_cached_url_);

  std::vector<FullHashStr> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(GURL(kArtificialHashRealTimeUnsafeUrl),
                                         &full_hashes);
  ASSERT_EQ(full_hashes.size(), 1u);
  FullHashStr full_hash = full_hashes[0];

  std::string hash_prefix = hash_realtime_utils::GetHashPrefix(full_hash);
  auto cache_results =
      cache_manager_->GetCachedHashPrefixRealTimeLookupResults({hash_prefix});
  EXPECT_EQ(cache_results[hash_prefix][0].full_hash(), full_hash);
}

}  // namespace safe_browsing
