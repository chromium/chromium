// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/geolocation_setting_delegate.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

TEST(GeolocationSettingDelegateTest, ParseGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  auto geo_result = *delegate.FromValue(base::Value(
      base::Value::Dict()
          .Set("approximate", static_cast<int>(PermissionOption::kAllowed))
          .Set("precise", static_cast<int>(PermissionOption::kDenied))));
  GeolocationSetting setting;
  setting.approximate = PermissionOption::kAllowed;
  setting.precise = PermissionOption::kDenied;
  EXPECT_EQ(PermissionSetting{setting}, geo_result);
}

TEST(GeolocationSettingDelegateTest, ParseInvalidGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  EXPECT_FALSE(delegate.FromValue(
      base::Value(static_cast<int>(PermissionOption::kAllowed))));
  EXPECT_FALSE(delegate.FromValue(base::Value(base::Value::Dict().Set(
      "approximate", static_cast<int>(PermissionOption::kDenied)))));
  EXPECT_FALSE(delegate.FromValue(base::Value(base::Value::Dict().Set(
      "precise", static_cast<int>(PermissionOption::kDenied)))));
  EXPECT_FALSE(delegate.FromValue(base::Value(
      base::Value::Dict()
          .Set("approximate", 999)
          .Set("precise", static_cast<int>(PermissionOption::kDenied)))));
}

TEST(GeolocationSettingDelegateTest, ValidateGeolocationSetting) {
  GeolocationSettingDelegate delegate;

  GeolocationSetting setting;
  setting.approximate = PermissionOption::kAllowed;
  setting.precise = PermissionOption::kDenied;
  EXPECT_TRUE(delegate.IsValid(setting));

  setting.approximate = PermissionOption::kDenied;
  EXPECT_TRUE(delegate.IsValid(setting));

  setting.precise = PermissionOption::kAsk;
  EXPECT_FALSE(delegate.IsValid(setting));

  setting.approximate = PermissionOption::kAsk;
  EXPECT_TRUE(delegate.IsValid(setting));
}

}  // namespace content_settings
