// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/features.h"

#include "base/test/scoped_feature_list.h"
#include "components/sync_preferences/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desktop_to_mobile_promos {

class MobilePromoOnDesktopFeaturesTest : public testing::Test {
 public:
  MobilePromoOnDesktopFeaturesTest() = default;
  ~MobilePromoOnDesktopFeaturesTest() override = default;

  void SetUp() override {
    // Enable the dependency feature.
    feature_list_.InitWithFeatures(
        {sync_preferences::features::kEnableCrossDevicePrefTracker}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MobilePromoOnDesktopFeaturesTest, DefaultAllPromos) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kMobilePromoOnDesktopWithReminder,
                             kMobilePromoOnDesktopWithReminderWave1},
                            {});
  // No param set -> All promos enabled.
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kLensPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kESBPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kAutofillPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kPriceTracking));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kTabGroups));
}

TEST_F(MobilePromoOnDesktopFeaturesTest, SingleInteger) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMobilePromoOnDesktopWithReminderWave1,
      {{kMobilePromoOnDesktopPromoTypeParam, "1"}});  // kLensPromo = 1

  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kLensPromo));
  EXPECT_FALSE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kPriceTracking));
  EXPECT_FALSE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kTabGroups));
}

TEST_F(MobilePromoOnDesktopFeaturesTest, CommaSeparatedList) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMobilePromoOnDesktopWithReminder,
      {{kMobilePromoOnDesktopPromoTypeParam, "2,3"}});  // ESB(2), Autofill(3)

  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kESBPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kAutofillPromo));
}

TEST_F(MobilePromoOnDesktopFeaturesTest, CommaSeparatedListWithWhitespace) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMobilePromoOnDesktopWithReminder,
      {{kMobilePromoOnDesktopPromoTypeParam, " 2 , 3 "}});

  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kESBPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kAutofillPromo));
}

TEST_F(MobilePromoOnDesktopFeaturesTest, ListWithZero) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMobilePromoOnDesktopWithReminder,
      {{kMobilePromoOnDesktopPromoTypeParam, "0,2"}});  // 0 = All

  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kESBPromo));
  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kAutofillPromo));
}

}  // namespace desktop_to_mobile_promos
