// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/geolocation_setting_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

class GeolocationSettingDelegateTest : public testing::Test {
 public:
  GeolocationSettingDelegateTest()
      : delegate_(content_settings::GeolocationSettingDelegate()) {}

  content_settings::GeolocationSettingDelegate delegate() { return delegate_; }

 private:
  content_settings::GeolocationSettingDelegate delegate_;
};

TEST_F(GeolocationSettingDelegateTest, ParseGeolocationSetting) {
  auto geo_result = delegate().FromValue(base::Value(
      base::Value::Dict()
          .Set("approximate", static_cast<int>(PermissionOption::kAllowed))
          .Set("precise", static_cast<int>(PermissionOption::kDenied))));
  GeolocationSetting setting;
  setting.approximate = PermissionOption::kAllowed;
  setting.precise = PermissionOption::kDenied;
  EXPECT_EQ(PermissionSetting{setting}, geo_result);
}

TEST_F(GeolocationSettingDelegateTest, ParseInvalidGeolocationSetting) {
  EXPECT_FALSE(delegate().FromValue(
      base::Value(static_cast<int>(PermissionOption::kAllowed))));
  EXPECT_FALSE(delegate().FromValue(base::Value(base::Value::Dict().Set(
      "approximate", static_cast<int>(PermissionOption::kDenied)))));
  EXPECT_FALSE(delegate().FromValue(base::Value(base::Value::Dict().Set(
      "precise", static_cast<int>(PermissionOption::kDenied)))));
  EXPECT_FALSE(delegate().FromValue(base::Value(
      base::Value::Dict()
          .Set("approximate", 999)
          .Set("precise", static_cast<int>(PermissionOption::kDenied)))));
}

TEST_F(GeolocationSettingDelegateTest, ValidateGeolocationSetting) {
  GeolocationSetting setting;

  setting.approximate = PermissionOption::kAllowed;
  setting.precise = PermissionOption::kDenied;
  EXPECT_TRUE(delegate().IsValid(setting));

  setting.approximate = PermissionOption::kDenied;
  EXPECT_TRUE(delegate().IsValid(setting));

  setting.precise = PermissionOption::kAsk;
  EXPECT_FALSE(delegate().IsValid(setting));

  setting.approximate = PermissionOption::kAsk;
  EXPECT_TRUE(delegate().IsValid(setting));
}

TEST_F(GeolocationSettingDelegateTest, VerifyCoalescingEphemeralState) {
  {
    auto coalesced =
        std::get<GeolocationSetting>(delegate().CoalesceEphemeralState(
            GeolocationSetting(PermissionOption::kAsk, PermissionOption::kAsk),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed)));
    EXPECT_EQ(coalesced, GeolocationSetting(PermissionOption::kAllowed,
                                            PermissionOption::kAllowed));
  }

  {
    auto coalesced =
        std::get<GeolocationSetting>(delegate().CoalesceEphemeralState(
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAsk),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed)));
    EXPECT_EQ(coalesced, GeolocationSetting(PermissionOption::kAllowed,
                                            PermissionOption::kAllowed));
  }

  {
    auto coalesced = std::get<GeolocationSetting>(
        delegate().InheritInIncognito(GeolocationSetting(
            PermissionOption::kAllowed, PermissionOption::kAsk)));
    EXPECT_EQ(coalesced, GeolocationSetting(PermissionOption::kAsk,
                                            PermissionOption::kAsk));
  }

  {
    auto coalesced =
        std::get<GeolocationSetting>(delegate().CoalesceEphemeralState(
            GeolocationSetting(PermissionOption::kDenied,
                               PermissionOption::kAsk),
            GeolocationSetting(PermissionOption::kAsk,
                               PermissionOption::kAllowed)));
    EXPECT_EQ(coalesced, GeolocationSetting(PermissionOption::kDenied,
                                            PermissionOption::kAllowed));
  }
}

TEST_F(GeolocationSettingDelegateTest, VerifyOnlyBlocksInheritedToIncognito) {
  {
    auto derived = std::get<GeolocationSetting>(delegate().InheritInIncognito(
        GeolocationSetting(PermissionOption::kAsk, PermissionOption::kAsk)));

    EXPECT_EQ(derived, GeolocationSetting(PermissionOption::kAsk,
                                          PermissionOption::kAsk));
  }

  {
    auto derived = std::get<GeolocationSetting>(
        delegate().InheritInIncognito(GeolocationSetting(
            PermissionOption::kAllowed, PermissionOption::kAsk)));

    EXPECT_EQ(derived, GeolocationSetting(PermissionOption::kAsk,
                                          PermissionOption::kAsk));
  }

  {
    auto derived = std::get<GeolocationSetting>(
        delegate().InheritInIncognito(GeolocationSetting(
            PermissionOption::kAllowed, PermissionOption::kAllowed)));

    EXPECT_EQ(derived, GeolocationSetting(PermissionOption::kAsk,
                                          PermissionOption::kAsk));
  }

  {
    auto derived = std::get<GeolocationSetting>(delegate().InheritInIncognito(
        GeolocationSetting(PermissionOption::kDenied, PermissionOption::kAsk)));

    EXPECT_EQ(derived, GeolocationSetting(PermissionOption::kDenied,
                                          PermissionOption::kAsk));
  }

  {
    auto derived = std::get<GeolocationSetting>(
        delegate().InheritInIncognito(GeolocationSetting(
            PermissionOption::kDenied, PermissionOption::kDenied)));
    EXPECT_EQ(derived,

              GeolocationSetting(PermissionOption::kDenied,
                                 PermissionOption::kDenied));
  }
}

TEST_F(GeolocationSettingDelegateTest, ApplyPermissionEmbargo) {
  // A default setting should be embargoed.
  {
    auto embargoed = std::get<GeolocationSetting>(
        delegate().ApplyPermissionEmbargo(GeolocationSetting(
            PermissionOption::kAsk, PermissionOption::kAsk)));
    EXPECT_EQ(embargoed, GeolocationSetting(PermissionOption::kDenied,
                                            PermissionOption::kDenied));
  }
  // A partially decided setting should only have the undecided part embargoed.
  {
    auto embargoed = std::get<GeolocationSetting>(
        delegate().ApplyPermissionEmbargo(GeolocationSetting(
            PermissionOption::kAllowed, PermissionOption::kAsk)));
    EXPECT_EQ(embargoed, GeolocationSetting(PermissionOption::kAllowed,
                                            PermissionOption::kDenied));
  }
  // A fully decided setting should not be affected by embargo.
  {
    auto embargoed = std::get<GeolocationSetting>(
        delegate().ApplyPermissionEmbargo(GeolocationSetting(
            PermissionOption::kAllowed, PermissionOption::kAllowed)));
    EXPECT_EQ(embargoed, GeolocationSetting(PermissionOption::kAllowed,
                                            PermissionOption::kAllowed));
  }
}

}  // namespace content_settings
