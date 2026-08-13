// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_features.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

TEST(PageContentAnnotationsFeaturesTest, InvalidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {{"num_bits_for_rappor_metrics", "-1"},
       {"noise_prob_for_rappor_metrics", "-.5"}});
  EXPECT_EQ(1, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(0.0, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(PageContentAnnotationsFeaturesTest, ValidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {{"num_bits_for_rappor_metrics", "2"},
       {"noise_prob_for_rappor_metrics", ".2"}});
  EXPECT_EQ(2, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(.2, features::NoiseProbabilityForRAPPORMetrics());
}

#if defined(ARCH_CPU_ARMEL)
#define MAYBE_ShouldExecutePageVisibilityModelOnPageContent \
  DISABLED_ShouldExecutePageVisibilityModelOnPageContent
#else
#define MAYBE_ShouldExecutePageVisibilityModelOnPageContent \
  ShouldExecutePageVisibilityModelOnPageContent
#endif
TEST(PageContentAnnotationsFeaturesTest,
     MAYBE_ShouldExecutePageVisibilityModelOnPageContent) {
  // These are default enabled values.
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-AU"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-CA"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-GB"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("fr"));

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("zh-CN"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent("de"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent(""));
}

TEST(PageContentAnnotationsFeaturesTest,
     IsSupportedCountryForFeatureEmptyParams) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Empty params.
  scoped_feature_list.InitAndEnableFeature(
      features::kPageContentAnnotationsValidation);
  // Allow for both "" and "*" as |default_value|.
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "US", features::kPageContentAnnotationsValidation,
      /*default_value=*/""));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kPageContentAnnotationsValidation,
      /*default_value=*/"*"));
}

TEST(PageContentAnnotationsFeaturesTest,
     IsSupportedCountryForFeatureParamsOverride) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Specified params should override defaults.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {{"supported_countries", "*"}});
  // All countries allowed by param, ignoring default_value allowlist.
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "US", features::kPageContentAnnotationsValidation,
      /*default_value=*/""));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kPageContentAnnotationsValidation,
      /*default_value=*/"*"));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kPageContentAnnotationsValidation,
      /*default_value=*/"US"));
}

}  // namespace
}  // namespace page_content_annotations
