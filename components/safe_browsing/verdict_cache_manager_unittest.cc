// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/verdict_cache_manager.h"

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class VerdictCacheManagerTest : public ::testing::Test {
 public:
  VerdictCacheManagerTest() {}

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */,
        false /* migrate_requesting_and_top_level_origin_settings */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get());
  }

  void TearDown() override {
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  void CachePhishGuardVerdict(
      const GURL& url,
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
    cache_manager_->CachePhishGuardVerdict(url, trigger, password_type,
                                           response, verdict_received_time);
  }

 protected:
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;

 private:
  content::BrowserTaskEnvironment task_environment_;
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

  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
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

  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
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
  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            cache_manager_->GetCachedPhishGuardVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                password_type, &cached_verdict));
}

TEST_F(VerdictCacheManagerTest, TestGetStoredPhishGuardVerdictCount) {
  GURL url("https://www.google.com/");

  LoginReputationClientResponse cached_verdict;
  cached_verdict.set_cache_expression("www.google.com/");
  EXPECT_EQ(0u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ReusedPasswordAccountType password_type;
  password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  password_type.set_is_account_syncing(true);
  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::SAFE, 60,
                         "www.google.com/", base::Time::Now());

  EXPECT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  CachePhishGuardVerdict(url,
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
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
      GURL("http://foo.com/abc/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CachePhishGuardVerdict(
      GURL("http://foo.com/abc/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  CachePhishGuardVerdict(GURL("http://bar.com/index.jsp"),
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         600, "bar.com", now);
  ASSERT_EQ(3u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  CachePhishGuardVerdict(
      GURL("http://foo.com/abc/index.jsp"),
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  CachePhishGuardVerdict(GURL("http://bar.com/index.jsp"),
                         LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
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
      GURL("https://foo.com/abc/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 600, "foo.com/abc/", now);
  CachePhishGuardVerdict(
      GURL("https://foo.com/def/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, password_type,
      LoginReputationClientResponse::LOW_REPUTATION, 0, "foo.com/def/", now);
  CachePhishGuardVerdict(GURL("https://bar.com/abc/index.jsp"),
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/abc/", now);
  CachePhishGuardVerdict(GURL("https://bar.com/def/index.jsp"),
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/def/", now);
  ASSERT_EQ(4u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Prepare 2 verdicts for UNFAMILIAR_LOGIN_PAGE:
  // (1) "bar.com/def/" valid
  // (2) "bar.com/xyz/" expired
  password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  CachePhishGuardVerdict(GURL("https://bar.com/def/index.jsp"),
                         LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::SAFE,
                         600, "bar.com/def/", now);
  CachePhishGuardVerdict(GURL("https://bar.com/xyz/index.jsp"),
                         LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                         password_type, LoginReputationClientResponse::PHISHING,
                         0, "bar.com/xyz/", now);
  ASSERT_EQ(2u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  cache_manager_->CleanUpExpiredVerdicts();

  ASSERT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1u, cache_manager_->GetStoredPhishGuardVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
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
  CachePhishGuardVerdict(GURL("https://www.google.com"),
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
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

}  // namespace safe_browsing
