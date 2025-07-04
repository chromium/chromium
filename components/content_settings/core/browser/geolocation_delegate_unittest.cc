// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/geolocation_setting_delegate.h"
#include "components/content_settings/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

TEST(GeolocationSettingDelegateTest, ParseGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  auto geo_result = *delegate.FromValue(
      base::Value(base::Value::Dict()
                      .Set("approximate", CONTENT_SETTING_ALLOW)
                      .Set("precise", CONTENT_SETTING_BLOCK)));
  GeolocationSetting setting;
  setting.approximate = CONTENT_SETTING_ALLOW;
  setting.precise = CONTENT_SETTING_BLOCK;
  EXPECT_EQ(PermissionSetting{setting}, geo_result);
}

TEST(GeolocationSettingDelegateTest, ParseInvalidGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  EXPECT_FALSE(delegate.FromValue(base::Value(CONTENT_SETTING_ALLOW)));
  EXPECT_FALSE(delegate.FromValue(base::Value(
      base::Value::Dict().Set("approximate", CONTENT_SETTING_BLOCK))));
  EXPECT_FALSE(delegate.FromValue(
      base::Value(base::Value::Dict().Set("precise", CONTENT_SETTING_BLOCK))));
  EXPECT_FALSE(delegate.FromValue(
      base::Value(base::Value::Dict()
                      .Set("approximate", 999)
                      .Set("precise", CONTENT_SETTING_BLOCK))));
}

TEST(GeolocationSettingDelegateTest, ValidateGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  GeolocationSetting setting;
  setting.approximate = CONTENT_SETTING_ALLOW;
  setting.precise = CONTENT_SETTING_BLOCK;
  EXPECT_TRUE(delegate.IsValid(setting));

  setting.approximate = CONTENT_SETTING_BLOCK;
  EXPECT_TRUE(delegate.IsValid(setting));

  setting.precise = CONTENT_SETTING_ASK;
  EXPECT_FALSE(delegate.IsValid(setting));

  setting.approximate = CONTENT_SETTING_ASK;
  EXPECT_TRUE(delegate.IsValid(setting));
}

}  // namespace content_settings
