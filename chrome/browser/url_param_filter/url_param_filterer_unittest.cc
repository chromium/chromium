// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filterer.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class UrlParamFiltererTest : public ::testing::Test {};

TEST_F(UrlParamFiltererTest, FilterUrlEmptyClassifications) {
  GURL source = GURL{"http://source.xyz"};
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  // If no classifications are passed in, don't modify the destination URL.
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, expected, url_param_filter::ClassificationMap(),
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 0);
}

TEST_F(UrlParamFiltererTest, FilterUrlNoChanges) {
  GURL source = GURL{"http://source.xyz"};
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // If classifications are passed in, but the destination URL doesn't contain
  // any blocked params, don't modify it.
  url_param_filter::FilterResult result =
      url_param_filter::FilterUrl(source, expected, source_classification_map,
                                  destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 0);
}

TEST_F(UrlParamFiltererTest, FilterUrlSourceBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{"https://destination.xyz?plzblock=123&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);

  // Navigations from source.xyz with a param called plzblock should have that
  // param removed, regardless of destination.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 1);
}

TEST_F(UrlParamFiltererTest, FilterUrlMultipleSourceBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock", "plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);

  // Navigations from source.xyz with a param called plzblock or plzblock1
  // should have those params removed, regardless of destination.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{"https://destination.xyz?plzblock=123&nochange=asdf"};
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // Navigations to destination.xyz with a param called plzblock should have
  // that param removed, regardless of source.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, url_param_filter::ClassificationMap(),
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 1);
}

TEST_F(UrlParamFiltererTest, FilterUrlMultipleDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock", "plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // Navigations to destination.xyz with a param called plzblock and/or
  // plzblock1 should have those param removed, regardless of source.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, url_param_filter::ClassificationMap(),
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlSourceAndDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // Both source and destination have associated URL param filtering rules. Only
  // nochange should remain.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest,
       FilterUrlSourceAndDestinationBlockedCheckOrderingPreserved) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "destination.xyz?plzblock=123&plzblock1=321&nochange=asdf&laternochange="
      "fdsa"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // Both source and destination have associated URL param filtering rules. Only
  // nochange should remain.
  GURL expected =
      GURL{"https://destination.xyz?nochange=asdf&laternochange=fdsa"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlSubdomainsApplied) {
  GURL source = GURL{"https://subdomain.source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  GURL expected = GURL{"https://subdomain.destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlCaseIgnored) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?PlZbLoCk=123&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // The disallowed params PlZbLoCk and PLZBLOCK1 should be removed.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlWithNestedUrl) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fplzblock1%"
      "3D123%26nochange%3Dasdf&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}, {"source.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // The nested URL pattern is commonly observed; we do not want the parameter
  // to leak.
  GURL expected = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 2);
}

TEST_F(UrlParamFiltererTest, FilterUrlWithNestedUrlNotNeedingFiltering) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}, {"source.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // The nested URL does not have filtered parameters and should be left alone.
  GURL expected = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 1);
}
TEST_F(UrlParamFiltererTest, FilterUrlWithNestedUrlAndDuplicates) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fplzblock1%"
      "3D123%26nochange%3Dasdf%26plzblock1%3D123&PLZBLOCK1=321&nochange=asdf&"
      "PLZBLOCK1=321"};
  url_param_filter::ClassificationMap source_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"source.xyz", {"plzblock"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_SOURCE);
  url_param_filter::ClassificationMap destination_classification_map =
      url_param_filter::CreateClassificationMapForTesting(
          {{"destination.xyz", {"plzblock1"}}, {"source.xyz", {"plzblock1"}}},
          url_param_filter::FilterClassification_SiteRole::
              FilterClassification_SiteRole_DESTINATION);

  // The nested URL pattern is commonly observed; we do not want the parameter
  // to leak.
  GURL expected = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&nochange=asdf"};
  url_param_filter::FilterResult result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      destination_classification_map);
  ASSERT_EQ(result.filtered_url, expected);
  ASSERT_EQ(result.filtered_param_count, 4);
}

TEST_F(UrlParamFiltererTest, FeatureDeactivated) {
  GURL source = GURL{"http://source.xyz"};
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  // When the feature is not explicitly activated, the 2-parameter version of
  // the function should be inert.
  GURL result = url_param_filter::FilterUrl(source, expected);

  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FeatureActivatedSourceAndDestinationRemoval) {
  GURL source = GURL{"http://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=1&plzblock1=2&nochange=asdf"};

  std::string encoded_classification =
      url_param_filter::CreateBase64EncodedFilterParamClassificationForTesting(
          {{"source.xyz", {"plzblock"}}}, {{"destination.xyz", {"plzblock1"}}});

  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled,
      {{"classifications", encoded_classification}});

  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination);

  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FeatureActivatedMetricsWritten) {
  base::HistogramTester histograms;
  const std::string histogram_name =
      "Navigation.UrlParamFilter.FilteredParamCountExperimental";
  std::string encoded_classification =
      url_param_filter::CreateBase64EncodedFilterParamClassificationForTesting(
          {{"source.xyz", {"plzblock"}}}, {{"destination.xyz", {"plzblock1"}}});

  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled,
      {{"classifications", encoded_classification}});

  GURL source = GURL{"http://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=1&plzblock1=2&nochange=asdf"};

  // The histogram should start off empty.
  histograms.ExpectTotalCount(histogram_name, 0);
  url_param_filter::FilterUrl(source, destination);
  // We filtered two parameters.
  ASSERT_EQ(histograms.GetTotalSum(histogram_name), 2);
  url_param_filter::FilterUrl(source, destination);
  // We filtered two more.
  ASSERT_EQ(histograms.GetTotalSum(histogram_name), 4);
  destination = GURL{"https://destination.xyz?plzblock=1&nochange=asdf"};
  url_param_filter::FilterUrl(source, destination);

  // This time just one more.
  ASSERT_EQ(histograms.GetTotalSum(histogram_name), 5);
  destination = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::FilterUrl(source, destination);
  // This time we didn't filter any.
  ASSERT_EQ(histograms.GetTotalSum(histogram_name), 5);
  // The number of samples should be 4 (four calls to FilterUrl).
  histograms.ExpectTotalCount(histogram_name, 4);
}
