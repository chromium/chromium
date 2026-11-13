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
  EXPECT_EQ(kDynamicAimFulfillmentSrpInputMode.Get(), "single_image");
  EXPECT_FALSE(kDynamicAimFulfillmentUseServerInputRules.Get());
}

TEST(ComposeboxFeaturesTest, ImageFileSizeLimitHelpersDefaultAndFallback) {
  constexpr uint64_t kAttachmentMax = 100ULL * 1024 * 1024;
  constexpr uint64_t kDefaultImageMax = 20ULL * 1024 * 1024;

  EXPECT_TRUE(IsComposeboxHeifMimeType("image/heic"));
  EXPECT_TRUE(IsComposeboxHeifMimeType("IMAGE/HEIF"));
  EXPECT_FALSE(IsComposeboxHeifMimeType("image/png"));

  // Default (enabled, limit_all_images=true): all image/* MIME types (including
  // unsupported ones like image/gif) use 20 MiB; PDFs keep 100 MiB.
  EXPECT_EQ(GetComposeboxMaxFileSizeBytes("image/png", /*is_image=*/false,
                                          kAttachmentMax),
            kDefaultImageMax);
  EXPECT_EQ(GetComposeboxMaxFileSizeBytes("image/gif", /*is_image=*/false,
                                          kAttachmentMax),
            kDefaultImageMax);
  EXPECT_EQ(GetComposeboxMaxFileSizeBytes("image/heic", /*is_image=*/false,
                                          kAttachmentMax),
            kDefaultImageMax);
  EXPECT_EQ(GetComposeboxMaxFileSizeBytes("application/pdf", /*is_image=*/false,
                                          kAttachmentMax),
            kAttachmentMax);

  // Fallback (limit_all_images=false): only HEIC/HEIF is capped at 20 MiB.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kComposeboxImageFileSizeLimit, {{"limit_all_images", "false"}});
    EXPECT_EQ(GetComposeboxMaxFileSizeBytes("image/heic", /*is_image=*/true,
                                            kAttachmentMax),
              kDefaultImageMax);
    EXPECT_EQ(GetComposeboxMaxFileSizeBytes("image/png", /*is_image=*/true,
                                            kAttachmentMax),
              kAttachmentMax);
  }
}

}  // namespace omnibox
