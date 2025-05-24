// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/content_setting_permission_resolver.h"

#include <optional>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class ContentSettingPermissionResolverTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::pair<ContentSettingsType, ContentSetting>> {};

/*
 * Param value format: <Setting, Default value>
 */
INSTANTIATE_TEST_SUITE_P(
    All,
    ContentSettingPermissionResolverTest,
    testing::Values(std::pair<ContentSettingsType, ContentSetting>(
                        ContentSettingsType::COOKIES,
                        CONTENT_SETTING_ALLOW),
                    std::pair<ContentSettingsType, ContentSetting>(
                        ContentSettingsType::ALL_SCREEN_CAPTURE,
                        CONTENT_SETTING_BLOCK),
                    std::pair<ContentSettingsType, ContentSetting>(
                        ContentSettingsType::GEOLOCATION,
                        CONTENT_SETTING_ASK)));

TEST_P(ContentSettingPermissionResolverTest, TestDeterminePermissionStatus) {
  ContentSettingsType type = GetParam().first;
  ContentSetting default_value = GetParam().second;
  ContentSettingPermissionResolver resolver(type);

  EXPECT_EQ(resolver.GetContentSettingsType(), type);
  EXPECT_EQ(resolver.default_value_, default_value);

  EXPECT_EQ(resolver.DeterminePermissionStatus(
                content_settings::ContentSettingToValue(
                    ContentSetting::CONTENT_SETTING_ALLOW)),
            blink::mojom::PermissionStatus::GRANTED);

  EXPECT_EQ(resolver.DeterminePermissionStatus(
                content_settings::ContentSettingToValue(
                    ContentSetting::CONTENT_SETTING_BLOCK)),
            blink::mojom::PermissionStatus::DENIED);

  EXPECT_EQ(resolver.DeterminePermissionStatus(
                content_settings::ContentSettingToValue(
                    ContentSetting::CONTENT_SETTING_ASK)),
            blink::mojom::PermissionStatus::ASK);

  EXPECT_EQ(resolver.DeterminePermissionStatus(
                content_settings::ContentSettingToValue(
                    ContentSetting::CONTENT_SETTING_DEFAULT)),
            PermissionUtil::ContentSettingToPermissionStatus(default_value));

  base::Value previous_setting(
      content_settings::ContentSettingToValue(CONTENT_SETTING_DEFAULT));

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_ALLOW, std::nullopt),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_BLOCK, std::nullopt),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_DEFAULT, std::nullopt),
            default_value);
}

TEST_P(ContentSettingPermissionResolverTest,
       TestComputePermissionDecisionResult) {
  ContentSettingsType type = GetParam().first;
  ContentSetting default_value = GetParam().second;

  ContentSettingPermissionResolver resolver(type);
  base::Value previous_setting(CONTENT_SETTING_DEFAULT);

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_ALLOW, std::nullopt),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_BLOCK, std::nullopt),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(resolver.ComputePermissionDecisionResult(
                previous_setting, CONTENT_SETTING_DEFAULT, std::nullopt),

            default_value);
}

}  // namespace permissions
