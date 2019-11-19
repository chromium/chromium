// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_ephemeral_provider.h"

#include <set>

#include "base/test/simple_test_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

class ContentSettingsEphemeralProviderTest : public testing::Test {
 public:
  ContentSettingsEphemeralProviderTest() {
    persistent_type_ = static_cast<ContentSettingsType>(1);
    ephemeral_types_[0] = static_cast<ContentSettingsType>(0);
    ephemeral_types_[1] = static_cast<ContentSettingsType>(2);
    Reset();
  }

  void Reset() {
    provider_.reset(new EphemeralProvider(true));

    std::set<ContentSettingsType> supported_types;
    supported_types.insert(ephemeral_types_[0]);
    supported_types.insert(ephemeral_types_[1]);
    provider()->SetSupportedTypesForTesting(supported_types);
  }

  EphemeralProvider* provider() { return provider_.get(); }
  ContentSettingsType persistent_type() { return persistent_type_; }
  ContentSettingsType ephemeral_type(int index) {
    return ephemeral_types_[index];
  }

 private:
  std::unique_ptr<EphemeralProvider> provider_;
  ContentSettingsType persistent_type_;
  ContentSettingsType ephemeral_types_[2];
};

// Tests if the ephemeral provider starts with an empty state.
TEST_F(ContentSettingsEphemeralProviderTest, EmptyStart) {
  EXPECT_EQ((size_t)0, provider()->GetCountForTesting());
}

// Tests if an ephemeral preference is stored and retrieved.
TEST_F(ContentSettingsEphemeralProviderTest, EphemeralTypeStorageAndRetrieval) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  EXPECT_TRUE(provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW)));

  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_NE(nullptr, rule_iterator);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), rule.value);

  // Overwrite previous value.
  EXPECT_TRUE(provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK)));

  rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_NE(nullptr, rule_iterator);
  EXPECT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule.value);
}

// Tests if storage of a persistent preference is rejected.
TEST_F(ContentSettingsEphemeralProviderTest, PersistentTypeRejection) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  std::unique_ptr<base::Value> value(new base::Value(false));
  EXPECT_FALSE(provider()->SetWebsiteSetting(site_pattern, site_pattern,
                                             persistent_type(), std::string(),
                                             std::move(value)));
  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(persistent_type(), std::string(), false);
  EXPECT_EQ(nullptr, rule_iterator);
}

// Tests if the last modified time of a stored preference is correctly returned.
TEST_F(ContentSettingsEphemeralProviderTest, LastModifiedTime) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  provider()->SetClockForTesting(&test_clock);
  base::Time t1 = test_clock.Now();

  provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  base::Time last_modified = provider()->GetWebsiteSettingLastModified(
      site_pattern, site_pattern, ephemeral_type(0), std::string());
  EXPECT_EQ(t1, last_modified);
}

// Tests if clearing all rules results in deletion of all stored preference.
TEST_F(ContentSettingsEphemeralProviderTest, ClearAll) {
  ContentSettingsPattern site_pattern1 =
      ContentSettingsPattern::FromString("https://example1.com");
  ContentSettingsPattern site_pattern2 =
      ContentSettingsPattern::FromString("https://example2.com");

  provider()->SetWebsiteSetting(
      site_pattern1, site_pattern1, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  provider()->SetWebsiteSetting(
      site_pattern2, site_pattern2, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  provider()->ClearAllContentSettingsRules(ephemeral_type(0));
  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_EQ(nullptr, rule_iterator);
}

// Tests if clearing all rules of one type doesn't effect other types.
TEST_F(ContentSettingsEphemeralProviderTest, SelectiveClear) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(1), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  provider()->ClearAllContentSettingsRules(ephemeral_type(0));
  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(1), std::string(), false);
  EXPECT_NE(nullptr, rule_iterator);
}

// Tests if the stored preference is ephemeral.
TEST_F(ContentSettingsEphemeralProviderTest, StorageIsEphemeral) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  Reset();
  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_EQ(nullptr, rule_iterator);
}

// Tests if a pattern can be deleted by passing null value.
TEST_F(ContentSettingsEphemeralProviderTest, DeleteValueByPassingNull) {
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  provider()->SetWebsiteSetting(
      site_pattern, site_pattern, ephemeral_type(0), std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  std::unique_ptr<RuleIterator> rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_NE(nullptr, rule_iterator);

  provider()->SetWebsiteSetting(site_pattern, site_pattern, ephemeral_type(0),
                                std::string(), nullptr);
  rule_iterator =
      provider()->GetRuleIterator(ephemeral_type(0), std::string(), false);
  EXPECT_EQ(nullptr, rule_iterator);
}

}  // namespace content_settings
