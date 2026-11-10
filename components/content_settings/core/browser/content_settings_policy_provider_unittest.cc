// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

class ContentSettingsPolicyProviderTest : public testing::Test {
 public:
  void SetUp() override {
    ContentSettingsRegistry::GetInstance();
    PolicyProvider::RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override {
    if (provider_) {
      provider_->ShutdownOnUIThread();
    }
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<PolicyProvider> provider_;
};

// Tests that setting `prefs::kManagedDefaultLegacyCookieScope` before
// initializing `PolicyProvider` (such as at browser startup via the
// `LegacyCookieScopeEnabled` policy) correctly configures the default content
// setting for `ContentSettingsType::LEGACY_COOKIE_SCOPE` without hitting
// `NOTREACHED()` (crbug.com/582263720).
TEST_F(ContentSettingsPolicyProviderTest,
       ManagedDefaultLegacyCookieScopeSetting) {
  prefs_.SetManagedPref(prefs::kManagedDefaultLegacyCookieScope,
                        base::Value(CONTENT_SETTING_ALLOW));

  provider_ = std::make_unique<PolicyProvider>(&prefs_);

  {
    std::unique_ptr<RuleIterator> rule_iterator = provider_->GetRuleIterator(
        ContentSettingsType::LEGACY_COOKIE_SCOPE, /*incognito=*/false);
    ASSERT_TRUE(rule_iterator);
    ASSERT_TRUE(rule_iterator->HasNext());
    std::unique_ptr<Rule> rule = rule_iterator->Next();
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
    EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), rule->value);
    EXPECT_FALSE(rule_iterator->HasNext());
  }

  GURL url("https://example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW, TestUtils::GetContentSetting(
                                       provider_.get(), url, url,
                                       ContentSettingsType::LEGACY_COOKIE_SCOPE,
                                       /*include_incognito=*/false));
}

// Tests that setting `prefs::kManagedLegacyCookieScopeForDomains` correctly
// configures pattern-based rules for
// `ContentSettingsType::LEGACY_COOKIE_SCOPE`.
TEST_F(ContentSettingsPolicyProviderTest, ManagedLegacyCookieScopeForDomains) {
  base::ListValue domain_list;
  domain_list.Append("[*.]example.com");
  prefs_.SetManagedPref(prefs::kManagedLegacyCookieScopeForDomains,
                        base::Value(std::move(domain_list)));

  provider_ = std::make_unique<PolicyProvider>(&prefs_);

  {
    std::unique_ptr<RuleIterator> rule_iterator = provider_->GetRuleIterator(
        ContentSettingsType::LEGACY_COOKIE_SCOPE, /*incognito=*/false);
    ASSERT_TRUE(rule_iterator);
    ASSERT_TRUE(rule_iterator->HasNext());
    std::unique_ptr<Rule> rule = rule_iterator->Next();
    EXPECT_EQ(ContentSettingsPattern::FromString("[*.]example.com"),
              rule->primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
    EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), rule->value);
    EXPECT_FALSE(rule_iterator->HasNext());
  }

  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      TestUtils::GetContentSetting(provider_.get(), GURL("https://example.com"),
                                   GURL("https://example.com"),
                                   ContentSettingsType::LEGACY_COOKIE_SCOPE,
                                   /*include_incognito=*/false));
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(provider_.get(), GURL("https://other.com"),
                                   GURL("https://other.com"),
                                   ContentSettingsType::LEGACY_COOKIE_SCOPE,
                                   /*include_incognito=*/false));
}

}  // namespace
}  // namespace content_settings
