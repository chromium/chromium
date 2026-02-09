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
        {sync_preferences::features::kEnableCrossDevicePrefTracker,
         kMobilePromoOnDesktopWithReminder},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MobilePromoOnDesktopFeaturesTest, DefaultAllPromos) {
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
      kMobilePromoOnDesktopWithReminder,
      {{kMobilePromoOnDesktopPromoTypeParam, "1"}});  // kLensPromo = 1

  EXPECT_TRUE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kLensPromo));
  EXPECT_FALSE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kESBPromo));
}

TEST_F(MobilePromoOnDesktopFeaturesTest, CommaSeparatedList) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMobilePromoOnDesktopWithReminder,
      {{kMobilePromoOnDesktopPromoTypeParam, "2,3"}});  // ESB(2), Autofill(3)

  EXPECT_FALSE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kLensPromo));
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

  EXPECT_FALSE(MobilePromoOnDesktopTypeEnabled(
      MobilePromoOnDesktopPromoType::kLensPromo));
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

}  // namespace desktop_to_mobile_promos
