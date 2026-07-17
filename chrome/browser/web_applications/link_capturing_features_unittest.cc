// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_features.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps::features {

class LinkCapturingFeaturesTest : public testing::Test {};

TEST_F(LinkCapturingFeaturesTest, DefaultBehavior) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kPwaNavigationCapturing},
      /*disabled_features=*/{});

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GetNavigationCapturingDefaultState(),
            ::features::CapturingState::kReimplDefaultOff);
  EXPECT_FALSE(IsNavigationCapturingOnByDefault());
#else
  EXPECT_EQ(GetNavigationCapturingDefaultState(),
            ::features::CapturingState::kReimplDefaultOn);
  EXPECT_TRUE(IsNavigationCapturingOnByDefault());
#endif
}

TEST_F(LinkCapturingFeaturesTest, TestingOverrideEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {::features::kPwaNavigationCapturing, {}},
          {kPwaNavigationCapturingTestingOverride,
           {{"link_capturing_state", "reimpl_default_on"}}},
      },
      /*disabled_features=*/{});

  EXPECT_EQ(GetNavigationCapturingDefaultState(),
            ::features::CapturingState::kReimplDefaultOn);
  EXPECT_TRUE(IsNavigationCapturingOnByDefault());
}

TEST_F(LinkCapturingFeaturesTest, TestingOverrideClientMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {::features::kPwaNavigationCapturing, {}},
          {kPwaNavigationCapturingTestingOverride,
           {{"link_capturing_state", "reimpl_on_via_client_mode"}}},
      },
      /*disabled_features=*/{});

  EXPECT_EQ(GetNavigationCapturingDefaultState(),
            ::features::CapturingState::kReimplOnViaClientMode);
  EXPECT_TRUE(IsNavigationCapturingOnByDefault());
}

TEST_F(LinkCapturingFeaturesTest, TestingOverrideWhenMainFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {kPwaNavigationCapturingTestingOverride,
           {{"link_capturing_state", "reimpl_default_on"}}},
      },
      /*disabled_features=*/{::features::kPwaNavigationCapturing});

  EXPECT_EQ(GetNavigationCapturingDefaultState(),
            ::features::CapturingState::kReimplDefaultOn);
  EXPECT_FALSE(IsNavigationCapturingOnByDefault());
}

}  // namespace apps::features
