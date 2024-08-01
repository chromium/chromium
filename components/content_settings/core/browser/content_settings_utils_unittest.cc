// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

namespace {

// clang-format off
const char* const kContentSettingNames[] = {
  "default",
  "allow",
  "block",
  "ask",
  "session_only",
  "detect_important_content",
};
// clang-format on

static_assert(std::size(kContentSettingNames) == CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingNames has an unexpected number of elements");

}  // namespace

TEST(ContentSettingsUtilsTest, ParsePatternString) {
  PatternPair pattern_pair;

  pattern_pair = ParsePatternString(std::string());
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = ParsePatternString(",");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = ParsePatternString("http://www.foo.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_EQ(pattern_pair.second, ContentSettingsPattern::Wildcard());

  // This inconsistency is to recover from some broken code.
  pattern_pair = ParsePatternString("http://www.foo.com,");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = ParsePatternString("http://www.foo.com,http://www.bar.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_TRUE(pattern_pair.second.IsValid());

  pattern_pair = ParsePatternString("http://www.foo.com,http://www.bar.com,");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = ParsePatternString(
      "http://www.foo.com,http://www.bar.com,http://www.error.com");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());
}

TEST(ContentSettingsUtilsTest, ContentSettingsStringMap) {
  std::string setting_string =
      ContentSettingToString(CONTENT_SETTING_NUM_SETTINGS);
  EXPECT_TRUE(setting_string.empty());

  for (size_t i = 0; i < std::size(kContentSettingNames); ++i) {
    ContentSetting setting = static_cast<ContentSetting>(i);
    setting_string = ContentSettingToString(setting);
    EXPECT_EQ(kContentSettingNames[i], setting_string);

    ContentSetting converted_setting;
    EXPECT_TRUE(
        ContentSettingFromString(kContentSettingNames[i], &converted_setting));
    EXPECT_EQ(setting, converted_setting);
  }
}

TEST(ContentSettingsUtilsTest, IsMorePermissive) {
  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK));
  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_ALLOW,
                               CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));
  EXPECT_TRUE(
      IsMorePermissive(CONTENT_SETTING_ALLOW, CONTENT_SETTING_SESSION_ONLY));

  EXPECT_TRUE(
      IsMorePermissive(CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ASK));
  EXPECT_TRUE(
      IsMorePermissive(CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
                               CONTENT_SETTING_ASK));
  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
                               CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_BLOCK,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));
  EXPECT_FALSE(
      IsMorePermissive(CONTENT_SETTING_BLOCK, CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK));

  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(
      IsMorePermissive(CONTENT_SETTING_ASK, CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_ASK,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));

  EXPECT_FALSE(
      IsMorePermissive(CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
                                CONTENT_SETTING_ALLOW));

  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW));

  // Check that all possible ContentSettings except CONTENT_SETTING_DEFAULT are
  // handled.
  for (int i = 1; i < CONTENT_SETTING_NUM_SETTINGS; ++i) {
    auto s = static_cast<ContentSetting>(i);
    EXPECT_FALSE(IsMorePermissive(s, s));
  }
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST(ContentSettingsUtilsTest, CanBeAutoRevoked) {
  EXPECT_TRUE(CanBeAutoRevoked(ContentSettingsType::GEOLOCATION,
                               ContentSetting::CONTENT_SETTING_ALLOW));

  // One-time grants should not be auto revoked.
  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::GEOLOCATION,
                                ContentSetting::CONTENT_SETTING_ALLOW, true));

  // Only allowed permissions should be auto revoked.
  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::GEOLOCATION,
                                ContentSetting::CONTENT_SETTING_DEFAULT));

  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::GEOLOCATION,
                                ContentSetting::CONTENT_SETTING_ASK));

  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::GEOLOCATION,
                                ContentSetting::CONTENT_SETTING_BLOCK));

  // Notification permissions should not be auto revoked.
  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::NOTIFICATIONS,
                                ContentSetting::CONTENT_SETTING_ALLOW));

  // Permissions that are not ask by default should not be auto revoked. IMAGES
  // permission is allowed by default, and ADS  permission is blocked by
  // default.
  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::IMAGES,
                                ContentSetting::CONTENT_SETTING_ALLOW));

  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::ADS,
                                ContentSetting::CONTENT_SETTING_ALLOW));

  // Chooser permissions that are allowlisted should be auto-revoked.
  EXPECT_TRUE(
      CanBeAutoRevoked(ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
                       base::Value("foo")));

  // Chooser permissions that are allowlisted but without any value
  // should not be auto-revoked.
  EXPECT_FALSE(CanBeAutoRevoked(
      ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA, base::Value()));

  // Chooser permissions that are not allowlisted should not be auto-revoked.
  EXPECT_FALSE(CanBeAutoRevoked(ContentSettingsType::USB_CHOOSER_DATA,
                                base::Value("foo")));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

class ContentSettingsUtilsFlagTest : public testing::TestWithParam<bool> {
 public:
  ContentSettingsUtilsFlagTest() {
    if (IsNoDelayForTestingEnabled()) {
      features_.InitWithFeaturesAndParameters(
          {{content_settings::features::kSafetyCheckUnusedSitePermissions,
            {{"unused-site-permissions-no-delay-for-testing", "true"}}}},
          {});
    }
  }

  bool IsNoDelayForTestingEnabled() const { return GetParam(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(ContentSettingsUtilsFlagTest, GetCoarseVisitedTime) {
  base::Time now = base::Time::Now();
  for (int i = 0; i < 20; i++) {
    base::Time time = now + base::Days(i);
    if (IsNoDelayForTestingEnabled()) {
      EXPECT_EQ(GetCoarseVisitedTime(time), time);
      EXPECT_EQ(GetCoarseVisitedTime(time),
                time - GetCoarseVisitedTimePrecision());
    } else {
      EXPECT_LE(GetCoarseVisitedTime(time), time);
      EXPECT_GE(GetCoarseVisitedTime(time),
                time - GetCoarseVisitedTimePrecision());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ContentSettingsUtilsFlagTest,
                         testing::Bool());

}  // namespace content_settings
