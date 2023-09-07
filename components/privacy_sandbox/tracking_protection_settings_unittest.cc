// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"
#include <memory>
#include <utility>
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class TrackingProtectionSettingsTest : public testing::Test {
 public:
  TrackingProtectionSettingsTest() {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(prefs());
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionSettings> tracking_protection_settings_;
};

TEST_F(TrackingProtectionSettingsTest, ReturnsCustomTrackingProtectionLevel) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionLevel(),
            tracking_protection::TrackingProtectionLevel::kCustom);
  EXPECT_TRUE(
      tracking_protection_settings()->IsCustomTrackingProtectionLevel());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsStandardTrackingProtectionLevel) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(
          tracking_protection::TrackingProtectionLevel::kStandard));
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionLevel(),
            tracking_protection::TrackingProtectionLevel::kStandard);
  EXPECT_TRUE(
      tracking_protection_settings()->IsStandardTrackingProtectionLevel());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsDoNotTrackStatus) {
  EXPECT_FALSE(tracking_protection_settings()->IsDoNotTrackEnabled());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  EXPECT_TRUE(tracking_protection_settings()->IsDoNotTrackEnabled());
}

}  // namespace
}  // namespace privacy_sandbox
