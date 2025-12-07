// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_features.h"

#include <limits>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

TEST(PageContentAnnotationsFeaturesTest, InvalidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "-1"},
       {"noise_prob_for_rappor_metrics", "-.5"}});
  EXPECT_EQ(1, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(0.0, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(PageContentAnnotationsFeaturesTest, ValidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "2"},
       {"noise_prob_for_rappor_metrics", ".2"}});
  EXPECT_EQ(2, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(.2, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(PageContentAnnotationsFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndDisableFeature(
      features::kPageVisibilityPageContentAnnotations);

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
}

TEST(PageContentAnnotationsFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentEmptyAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(
      features::kPageVisibilityPageContentAnnotations);

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

TEST(PageContentAnnotationsFeaturesTest, RemotePageMetadataEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemotePageMetadata,
      {{"supported_locales", "en-US,en-CA"}, {"supported_countries", "US,CA"}});

  EXPECT_TRUE(features::RemotePageMetadataEnabled("en-US", "CA"));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("", ""));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("en-US", "badcountry"));
  EXPECT_FALSE(features::RemotePageMetadataEnabled("badlocale", "US"));
}

TEST(PageContentAnnotationsFeaturesTest, RemotePageMetadataEnabledWildcard) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemotePageMetadata,
      {{"supported_locales", "*"}, {"supported_countries", "*"}});

  EXPECT_TRUE(features::RemotePageMetadataEnabled("en-US", "CA"));
  EXPECT_TRUE(features::RemotePageMetadataEnabled("", ""));
  EXPECT_TRUE(features::RemotePageMetadataEnabled("en-US", "badcountry"));
  EXPECT_TRUE(features::RemotePageMetadataEnabled("badlocale", "US"));
}

TEST(PageContentAnnotationsFeaturesTest,
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

TEST(PageContentAnnotationsFeaturesTest, ShouldPersistSalientImageMetadata) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Mobile should accept all locales and countries.
  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("en-US", "CA"));
  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("fr-CH", "CH"));
#else
  // Desktop should only accept en-US, US.
  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("en-US", "US"));
  // Tests case-insensitivity.
  EXPECT_TRUE(features::ShouldPersistSalientImageMetadata("en-US", "uS"));
  EXPECT_FALSE(features::ShouldPersistSalientImageMetadata("", ""));
  EXPECT_FALSE(
      features::ShouldPersistSalientImageMetadata("en-US", "badcountry"));
  EXPECT_FALSE(features::ShouldPersistSalientImageMetadata("badlocale", "US"));
#endif
}

TEST(PageContentAnnotationsFeaturesTest,
     IsSupportedLocaleOrCountryForFeatureEmptyParams) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Empty params.
  scoped_feature_list.InitAndEnableFeature(features::kRemotePageMetadata);
  // Allow for both "" and "*" as |default_value|.
  EXPECT_TRUE(features::IsSupportedLocaleForFeature(
      "en-US", features::kRemotePageMetadata,
      /*default_value=*/""));
  EXPECT_TRUE(
      features::IsSupportedLocaleForFeature("it", features::kRemotePageMetadata,
                                            /*default_value=*/"*"));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "US", features::kRemotePageMetadata,
      /*default_value=*/""));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kRemotePageMetadata,
      /*default_value=*/"*"));
}

TEST(PageContentAnnotationsFeaturesTest,
     IsSupportedLocaleOrCountryForFeatureParamsOverride) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Specified params should override defaults.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemotePageMetadata,
      {{"supported_locales", "en-US,en-CA,fr"}, {"supported_countries", "*"}});
  // All countries allowed by param, ignoring default_value allowlist.
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "US", features::kRemotePageMetadata,
      /*default_value=*/""));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kRemotePageMetadata,
      /*default_value=*/"*"));
  EXPECT_TRUE(features::IsSupportedCountryForFeature(
      "CA", features::kRemotePageMetadata,
      /*default_value=*/"US"));
  // Locales only allow en-US,en-CA specifically respecting param.
  EXPECT_TRUE(features::IsSupportedLocaleForFeature(
      "en-CA", features::kRemotePageMetadata,
      /*default_value=*/"*"));
  EXPECT_TRUE(features::IsSupportedLocaleForFeature(
      "en-US", features::kRemotePageMetadata,
      /*default_value=*/"*"));
  // en locale is less specific than allowlist so it doesn't match.
  EXPECT_FALSE(
      features::IsSupportedLocaleForFeature("en", features::kRemotePageMetadata,
                                            /*default_value=*/""));
  // More specific than allowlist is allowed.
  EXPECT_TRUE(
      features::IsSupportedLocaleForFeature("fr", features::kRemotePageMetadata,
                                            /*default_value=*/"*"));
  EXPECT_TRUE(features::IsSupportedLocaleForFeature(
      "fr-CA", features::kRemotePageMetadata,
      /*default_value=*/"*"));
  EXPECT_FALSE(
      features::IsSupportedLocaleForFeature("it", features::kRemotePageMetadata,
                                            /*default_value=*/""));
  EXPECT_FALSE(features::IsSupportedLocaleForFeature(
      "zh-TW", features::kRemotePageMetadata,
      /*default_value=*/""));
}

}  // namespace
}  // namespace page_content_annotations
