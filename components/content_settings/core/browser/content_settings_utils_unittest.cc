// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <string>

#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

namespace {

const char* const kContentSettingNames[] = {
  "default",
  "allow",
  "block",
  "ask",
  "session_only",
  "detect_important_content",
};
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
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK));
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_SESSION_ONLY));

  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ASK));
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, CONTENT_SETTING_ASK));
  EXPECT_TRUE(IsMorePermissive(
      CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(IsMorePermissive(CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));
  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(IsMorePermissive(CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK));

  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_ASK, CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_ASK, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));

  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, CONTENT_SETTING_ALLOW));

  EXPECT_FALSE(IsMorePermissive(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW));

  // Check that all possible ContentSettings except CONTENT_SETTING_DEFAULT are
  // handled.
  for (int i = 1; i < CONTENT_SETTING_NUM_SETTINGS; ++i) {
    auto s = static_cast<ContentSetting>(i);
    EXPECT_FALSE(IsMorePermissive(s, s));
  }
}

}  // namespace content_settings
