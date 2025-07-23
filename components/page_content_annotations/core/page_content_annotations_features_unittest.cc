// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_features.h"

namespace page_content_annotations {

namespace {

TEST(OptimizationGuideFeaturesTest, InvalidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "-1"},
       {"noise_prob_for_rappor_metrics", "-.5"}});
  EXPECT_EQ(1, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(0.0, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(OptimizationGuideFeaturesTest, ValidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "2"},
       {"noise_prob_for_rappor_metrics", ".2"}});
  EXPECT_EQ(2, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(.2, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndDisableFeature(
      features::kPageVisibilityPageContentAnnotations);

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentEmptyAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(
      features::kPageVisibilityPageContentAnnotations);

  // These are the default enabled values.
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-AU"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-CA"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-GB"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("zh-CN"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent("fr"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent(""));
}

TEST(OptimizationGuideFeaturesTest, RemotePageMetadataEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemotePageMetadata,
      {{"supported_locales", "en-US,en-CA"}, {"supported_countries", "US,CA"}});

  EXPECT_TRUE(features::RemotePageMetadataEnabled("en-US", "CA"));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("", ""));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("en-US", "badcountry"));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("badlocale", "US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentWithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageVisibilityPageContentAnnotations,
      {{"supported_locales", "en,zh-TW"}});

  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent(""));
  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("zh-CN"));
}

TEST(OptimizationGuideFeaturesTest, ShouldPersistSalientImageMetadata) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsPersistSalientImageMetadata,
      {{"supported_locales", "en-US,en-CA"}, {"supported_countries", "US,CA"}});

  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("en-US", "CA"));
  // Tests case-insensitivity.
  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("en-US", "cA"));
  EXPECT_FALSE(features::ShouldPersistSalientImageMetadata("", ""));
  EXPECT_FALSE(
      features::ShouldPersistSalientImageMetadata("en-US", "badcountry"));
  EXPECT_FALSE(features::ShouldPersistSalientImageMetadata("badlocale", "US"));
}

}  // namespace
}  // namespace page_content_annotations
