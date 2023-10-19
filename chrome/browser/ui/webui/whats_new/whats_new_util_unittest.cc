// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

TEST(WhatsNewUtil, GetServerURL) {
  const std::string expected_no_redirect = base::StringPrintf(
      "https://www.google.com/chrome/whats-new/m%d?internal=true",
      CHROME_VERSION_MAJOR);
  const std::string expected_redirect = base::StringPrintf(
      "https://www.google.com/chrome/whats-new/?version=%d&internal=true",
      CHROME_VERSION_MAJOR);

  EXPECT_EQ(expected_no_redirect,
            whats_new::GetServerURL(false).possibly_invalid_spec());
  EXPECT_EQ(expected_redirect,
            whats_new::GetServerURL(true).possibly_invalid_spec());
}

class WhatsNewUtilTests : public testing::Test {
 public:
  enum class EnableMode { kNotEnabled, kViewsRefreshOnly, kEnabled };

  WhatsNewUtilTests(const WhatsNewUtilTests&) = delete;
  WhatsNewUtilTests& operator=(const WhatsNewUtilTests&) = delete;

  void SetUp() override {
    ToggleRefresh(EnableMode::kEnabled);
    prefs_.registry()->RegisterBooleanPref(prefs::kHasShownRefreshWhatsNew,
                                           false);
    prefs_.registry()->RegisterIntegerPref(prefs::kLastWhatsNewVersion,
                                           CHROME_VERSION_MAJOR);
  }

  void ToggleRefresh(EnableMode mode) {
    scoped_feature_list_.Reset();
    switch (mode) {
      case EnableMode::kEnabled:
        scoped_feature_list_.InitWithFeatures(
            {whats_new::kForceEnabled, features::kChromeRefresh2023,
             features::kChromeWebuiRefresh2023},
            {});
        break;
      case EnableMode::kViewsRefreshOnly:
        scoped_feature_list_.InitWithFeatures(
            {whats_new::kForceEnabled, features::kChromeRefresh2023},
            {features::kChromeWebuiRefresh2023});
        break;
      case EnableMode::kNotEnabled:
        scoped_feature_list_.InitWithFeatures(
            {whats_new::kForceEnabled},
            {features::kChromeRefresh2023, features::kChromeWebuiRefresh2023});
    }
  }

  void SetHasShownRefresh(bool has_shown) {
    prefs()->SetBoolean(prefs::kHasShownRefreshWhatsNew, has_shown);
  }

  // The check in whats_new_util.cc compares the CHROME_VERSION_MAJOR
  // macro to the value stored in the kLastWhatsNewVersion pref. This
  // method sets whether that check should pass (should show milestone
  // WNP) or fail (current milestone WNP has already been shown).
  void SetHasNewWhatsNewVersion(const bool& has_new_version) {
    prefs_.SetInteger(
        prefs::kLastWhatsNewVersion,
        has_new_version ? CHROME_VERSION_MAJOR - 1 : CHROME_VERSION_MAJOR);
  }

  PrefService* prefs() { return &prefs_; }

 protected:
  WhatsNewUtilTests() = default;
  ~WhatsNewUtilTests() override = default;

 private:
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WhatsNewUtilTests, ShouldShowRefresh) {
  // Set version to a refresh-compatible version.
  whats_new::SetChromeVersionForTests(119);

  // Refresh page should only be shown when
  // kChromeRefresh2023=enabled && hasShownRefreshWhatsNew=false
  EXPECT_TRUE(whats_new::ShouldShowRefresh(prefs()));

  // kChromeRefresh2023=enabled && hasShownRefreshWhatsNew=true
  SetHasShownRefresh(true);
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));

  // Disable Refresh 2023 feature2
  ToggleRefresh(EnableMode::kNotEnabled);
  // kChromeRefresh2023=disabled && hasShownRefreshWhatsNew=true
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));

  // kChromeRefresh2023=disabled && hasShownRefreshWhatsNew=false
  SetHasShownRefresh(false);
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));

  // Enable only Views refresh.
  ToggleRefresh(EnableMode::kViewsRefreshOnly);
  // kChromeRefresh2023=disabled && hasShownRefreshWhatsNew=true
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));
}

TEST_F(WhatsNewUtilTests, ShouldShowForStateUsesChromeVersionForRefresh) {
  // kChromeRefresh2023=enabled - flags enabled.

  // M117
  whats_new::SetChromeVersionForTests(117);
  // User has not seen WN refresh yet.
  SetHasShownRefresh(false);
  // Refresh page should show.
  EXPECT_TRUE(whats_new::ShouldShowForState(prefs(), true));

  // User has seen WN refresh.
  SetHasShownRefresh(true);
  // Refresh page should not show again.
  EXPECT_FALSE(whats_new::ShouldShowForState(prefs(), true));

  // M116. Pre-refresh version.
  whats_new::SetChromeVersionForTests(116);
  // User has not seen WN refresh yet.
  SetHasShownRefresh(false);
  // Refresh page should not show previous to 117.
  EXPECT_FALSE(whats_new::ShouldShowForState(prefs(), true));

  // M119
  whats_new::SetChromeVersionForTests(119);
  // User has not seen WN refresh yet.
  SetHasShownRefresh(false);
  // Refresh page should show.
  EXPECT_TRUE(whats_new::ShouldShowForState(prefs(), true));

  // User has seen WN refresh.
  SetHasShownRefresh(true);
  // Nothing should show.
  EXPECT_FALSE(whats_new::ShouldShowForState(prefs(), true));

  // User has an unseen WNP.
  SetHasNewWhatsNewVersion(true);
  // Regular WNP should show.
  EXPECT_TRUE(whats_new::ShouldShowForState(prefs(), true));

  // M122
  whats_new::SetChromeVersionForTests(122);
  // User has not seen WN refresh yet.
  SetHasShownRefresh(false);
  // User does not have an unseen WNP.
  SetHasNewWhatsNewVersion(false);
  // Refresh page should not show after 121.
  EXPECT_FALSE(whats_new::ShouldShowForState(prefs(), true));
}
