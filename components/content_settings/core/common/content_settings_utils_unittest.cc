// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

TEST(ContentSettingsTest, ParseContentSetting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kApproximateGeolocationPermission);

  auto cookie_result = *ValueToPermissionSetting(
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(PermissionSetting{CONTENT_SETTING_ALLOW}, cookie_result);

  // Geolocation permissions should be parsed as ContentSetting when
  // kApproximateGeolocationPermission is disabled.
  auto geo_result = *ValueToPermissionSetting(
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(PermissionSetting{CONTENT_SETTING_ALLOW}, geo_result);
}

TEST(ContentSettingsTest, ParseInvalidContentSetting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kApproximateGeolocationPermission);

  EXPECT_FALSE(ValueToPermissionSetting(ContentSettingsType::COOKIES,
                                        base::Value(base::Value::Dict())));

  // Geolocation permissions should be parsed as ContentSetting when
  // kApproximateGeolocationPermission is disabled.
  EXPECT_FALSE(ValueToPermissionSetting(
      ContentSettingsType::GEOLOCATION,
      base::Value(base::Value::Dict()
                      .Set("approximate", CONTENT_SETTING_ALLOW)
                      .Set("precise", CONTENT_SETTING_BLOCK))));
}

TEST(ContentSettingsTest, ParseGeolocationSetting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kApproximateGeolocationPermission);

  auto geo_result = *ValueToPermissionSetting(
      ContentSettingsType::GEOLOCATION,
      base::Value(base::Value::Dict()
                      .Set("approximate", CONTENT_SETTING_ALLOW)
                      .Set("precise", CONTENT_SETTING_BLOCK)));
  GeolocationSetting setting;
  setting.approximate = CONTENT_SETTING_ALLOW;
  setting.precise = CONTENT_SETTING_BLOCK;
  EXPECT_EQ(PermissionSetting{setting}, geo_result);
}

TEST(ContentSettingsTest, ParseInvalidGeolocationSetting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kApproximateGeolocationPermission);

  EXPECT_FALSE(ValueToPermissionSetting(ContentSettingsType::GEOLOCATION,
                                        base::Value(CONTENT_SETTING_ALLOW)));
  EXPECT_FALSE(
      ValueToPermissionSetting(ContentSettingsType::GEOLOCATION,
                               base::Value(base::Value::Dict().Set(
                                   "approximate", CONTENT_SETTING_BLOCK))));
  EXPECT_FALSE(ValueToPermissionSetting(
      ContentSettingsType::GEOLOCATION,
      base::Value(base::Value::Dict().Set("precise", CONTENT_SETTING_BLOCK))));
  EXPECT_FALSE(ValueToPermissionSetting(
      ContentSettingsType::GEOLOCATION,
      base::Value(base::Value::Dict()
                      .Set("approximate", 999)
                      .Set("precise", CONTENT_SETTING_BLOCK))));
}

}  // namespace
}  // namespace content_settings
