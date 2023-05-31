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
  WhatsNewUtilTests(const WhatsNewUtilTests&) = delete;
  WhatsNewUtilTests& operator=(const WhatsNewUtilTests&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeRefresh2023);
    prefs_.registry()->RegisterBooleanPref(prefs::kHasShownRefreshWhatsNew,
                                           false);
  }

  void ToggleRefresh(bool enabled) {
    scoped_feature_list_.Reset();
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(features::kChromeRefresh2023);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kChromeRefresh2023);
    }
  }

  void ToggleHasShownRefresh(bool has_shown) {
    prefs()->SetBoolean(prefs::kHasShownRefreshWhatsNew, has_shown);
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
  // Refresh page should only be shown when
  // kChromeRefresh2023=enabled && hasShownRefreshWhatsNew=false
  EXPECT_TRUE(whats_new::ShouldShowRefresh(prefs()));

  // kChromeRefresh2023=enabled && hasShownRefreshWhatsNew=true
  ToggleHasShownRefresh(true);
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));

  // Disable Refresh 2023 feature
  ToggleRefresh(false);
  // kChromeRefresh2023=disabled && hasShownRefreshWhatsNew=true
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));

  // kChromeRefresh2023=disabled && hasShownRefreshWhatsNew=false
  ToggleHasShownRefresh(false);
  EXPECT_FALSE(whats_new::ShouldShowRefresh(prefs()));
}
