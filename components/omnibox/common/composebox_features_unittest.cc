// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/composebox_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

TEST(ComposeboxFeaturesTest, DynamicAimFulfillmentOffByDefault) {
  EXPECT_FALSE(IsDynamicAimFulfillmentEnabledForRealbox());
  EXPECT_FALSE(IsDynamicAimFulfillmentEnabledForOmnibox());
}

TEST(ComposeboxFeaturesTest, DynamicAimFulfillmentNeedsMainSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kDynamicAimFulfillmentRealbox, kDynamicAimFulfillmentOmnibox},
      {kDynamicAimFulfillment});
  EXPECT_FALSE(IsDynamicAimFulfillmentEnabledForRealbox());
  EXPECT_FALSE(IsDynamicAimFulfillmentEnabledForOmnibox());
}

TEST(ComposeboxFeaturesTest, DynamicAimFulfillmentNeedsSurfaceFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kDynamicAimFulfillment, kDynamicAimFulfillmentRealbox},
      {kDynamicAimFulfillmentOmnibox});
  EXPECT_TRUE(IsDynamicAimFulfillmentEnabledForRealbox());
  EXPECT_FALSE(IsDynamicAimFulfillmentEnabledForOmnibox());
}

TEST(ComposeboxFeaturesTest, DynamicAimFulfillmentParamDefaults) {
  base::test::ScopedFeatureList feature_list(kDynamicAimFulfillment);
  EXPECT_EQ(kDynamicAimFulfillmentSrpInputPhase.Get(), "single_image");
  EXPECT_FALSE(kDynamicAimFulfillmentUseServerInputRules.Get());
}

}  // namespace omnibox
