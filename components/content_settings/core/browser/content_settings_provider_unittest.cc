// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

TEST(ContentSettingsProviderTest, Mock) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]youtube.com");
  GURL url("http://www.youtube.com");

  MockProvider mock_provider(false);
  mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::PLUGINS, "java_plugin",
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::PLUGINS,
                                         "java_plugin", false));
  std::unique_ptr<base::Value> value_ptr(TestUtils::GetContentSettingValue(
      &mock_provider, url, url, ContentSettingsType::PLUGINS, "java_plugin",
      false));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::PLUGINS,
                                         "flash_plugin", false));
  EXPECT_EQ(nullptr, TestUtils::GetContentSettingValue(
                         &mock_provider, url, url, ContentSettingsType::PLUGINS,
                         "flash_plugin", false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));
  EXPECT_EQ(nullptr,
            TestUtils::GetContentSettingValue(&mock_provider, url, url,
                                              ContentSettingsType::GEOLOCATION,
                                              std::string(), false));

  bool owned = mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::PLUGINS, "java_plugin",
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::PLUGINS,
                                         "java_plugin", false));

  mock_provider.set_read_only(true);
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_BLOCK));
  owned = mock_provider.SetWebsiteSetting(pattern, pattern,
                                          ContentSettingsType::PLUGINS,
                                          "java_plugin", std::move(value));
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::PLUGINS,
                                         "java_plugin", false));

  EXPECT_TRUE(mock_provider.read_only());

  mock_provider.set_read_only(false);
  owned = mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::PLUGINS, "java_plugin",
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&mock_provider, url, url,
                                         ContentSettingsType::PLUGINS,
                                         "java_plugin", false));
}

}  // namespace content_settings
