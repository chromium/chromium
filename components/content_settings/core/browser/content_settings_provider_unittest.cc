// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
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
      pattern, pattern, ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      PartitionKey::GetDefaultForTesting());

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&mock_provider, url, url,
                                   ContentSettingsType::NOTIFICATIONS, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &mock_provider, url, url, ContentSettingsType::NOTIFICATIONS, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&mock_provider, url, url,
                                   ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &mock_provider, url, url,
                               ContentSettingsType::GEOLOCATION, false));

  bool owned = mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(owned);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      TestUtils::GetContentSetting(&mock_provider, url, url,
                                   ContentSettingsType::NOTIFICATIONS, false));

  mock_provider.set_read_only(true);
  owned = mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(owned);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      TestUtils::GetContentSetting(&mock_provider, url, url,
                                   ContentSettingsType::NOTIFICATIONS, false));

  EXPECT_TRUE(mock_provider.read_only());

  mock_provider.set_read_only(false);
  owned = mock_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(owned);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&mock_provider, url, url,
                                   ContentSettingsType::NOTIFICATIONS, false));
}

}  // namespace content_settings
