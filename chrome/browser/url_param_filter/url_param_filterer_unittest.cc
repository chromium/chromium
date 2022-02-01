// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filterer.h"

#include <string>

#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

class UrlParamFiltererTest : public ::testing::Test {};

url_param_filter::ClassificationMap CreateClassificationMap(
    std::map<std::string, std::vector<std::string>> source) {
  url_param_filter::ClassificationMap result;
  for (auto i : source) {
    url_param_filter::FilterClassification classification;
    for (auto j : i.second) {
      url_param_filter::FilterParameter* parameter =
          classification.add_parameters();
      parameter->set_name(j);
    }
    result[i.first] = classification;
  }
  return result;
}
TEST_F(UrlParamFiltererTest, FilterUrlEmptyClassifications) {
  GURL source = GURL{"http://source.xyz"};
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  // If no classifications are passed in, don't modify the destination URL.
  GURL result = url_param_filter::FilterUrl(
      source, expected, url_param_filter::ClassificationMap(),
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlNoChanges) {
  GURL source = GURL{"http://source.xyz"};
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock1"}}});

  // If classifications are passed in, but the destination URL doesn't contain
  // any blocked params, don't modify it.
  GURL result =
      url_param_filter::FilterUrl(source, expected, source_classification_map,
                                  destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlSourceBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{"https://destination.xyz?plzblock=123&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});

  // Navigations from source.xyz with a param called plzblock should have that
  // param removed, regardless of destination.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlMultipleSourceBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock", "plzblock1"}}});

  // Navigations from source.xyz with a param called plzblock or plzblock1
  // should have those params removed, regardless of destination.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(
      source, destination, source_classification_map,
      url_param_filter::ClassificationMap());
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{"https://destination.xyz?plzblock=123&nochange=asdf"};
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock"}}});

  // Navigations to destination.xyz with a param called plzblock should have
  // that param removed, regardless of source.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(
      source, destination, url_param_filter::ClassificationMap(),
      destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlMultipleDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock", "plzblock1"}}});

  // Navigations to destination.xyz with a param called plzblock and/or
  // plzblock1 should have those param removed, regardless of source.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(
      source, destination, url_param_filter::ClassificationMap(),
      destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlSourceAndDestinationBlocked) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock1"}}});

  // Both source and destination have associated URL param filtering rules. Only
  // nochange should remain.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest,
       FilterUrlSourceAndDestinationBlockedCheckOrderingPreserved) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "destination.xyz?plzblock=123&plzblock1=321&nochange=asdf&laternochange="
      "fdsa"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock1"}}});

  // Both source and destination have associated URL param filtering rules. Only
  // nochange should remain.
  GURL expected =
      GURL{"https://destination.xyz?nochange=asdf&laternochange=fdsa"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlSubdomainsApplied) {
  GURL source = GURL{"https://subdomain.source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.destination.xyz?plzblock=123&plzblock1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock1"}}});

  GURL expected = GURL{"https://subdomain.destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlCaseIgnored) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination =
      GURL{"https://destination.xyz?PlZbLoCk=123&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap({{"destination.xyz", {"plzblock1"}}});

  // The disallowed params PlZbLoCk and PLZBLOCK1 should be removed.
  GURL expected = GURL{"https://destination.xyz?nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlWithNestedUrl) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fplzblock1%"
      "3D123%26nochange%3Dasdf&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap(
          {{"destination.xyz", {"plzblock1"}}, {"source.xyz", {"plzblock1"}}});

  // The nested URL pattern is commonly observed; we do not want the parameter
  // to leak.
  GURL expected = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}

TEST_F(UrlParamFiltererTest, FilterUrlWithNestedUrlNotNeedingFiltering) {
  GURL source = GURL{"https://source.xyz"};
  GURL destination = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&PLZBLOCK1=321&nochange=asdf"};
  url_param_filter::ClassificationMap source_classification_map =
      CreateClassificationMap({{"source.xyz", {"plzblock"}}});
  url_param_filter::ClassificationMap destination_classification_map =
      CreateClassificationMap(
          {{"destination.xyz", {"plzblock1"}}, {"source.xyz", {"plzblock1"}}});

  // The nested URL does not have filtered parameters and should be left alone.
  GURL expected = GURL{
      "https://"
      "subdomain.source.xyz?destination=https%3A%2F%2Fdestination.xyz%2F%"
      "3Fnochange%"
      "3Dasdf&nochange=asdf"};
  GURL result = url_param_filter::FilterUrl(source, destination,
                                            source_classification_map,
                                            destination_classification_map);
  ASSERT_EQ(result, expected);
}
