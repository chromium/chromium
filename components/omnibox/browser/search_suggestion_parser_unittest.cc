// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

////////////////////////////////////////////////////////////////////////////////
// DeserializeJsonData:

TEST(SearchSuggestionParserTest, DeserializeNonListJsonIsInvalid) {
  std::string json_data = "{}";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeMalformedJsonIsInvalid) {
  std::string json_data = "} malformed json {";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeJsonData) {
  std::string json_data = R"([{"one": 1}])";
  base::Optional<base::Value> manifest_value =
      base::JSONReader::Read(json_data);
  ASSERT_TRUE(manifest_value);
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
  ASSERT_EQ(*manifest_value, *result);
}

TEST(SearchSuggestionParserTest, DeserializeWithXssiGuard) {
  // For XSSI protection, non-json may precede the actual data.
  // Parsing fails at:       v         v
  std::string json_data = R"([non-json [prefix [{"one": 1}])";
  // Parsing succeeds at:                      ^

  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);

  // Specifically, we precede JSON with )]}'\n.
  json_data = ")]}'\n[{\"one\": 1}]";
  result = SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
}

TEST(SearchSuggestionParserTest, DeserializeWithTrailingComma) {
  // The comma in this string makes this badly formed JSON, but we explicitly
  // allow for this error in the JSON data.
  std::string json_data = R"([{"one": 1},])";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
}

////////////////////////////////////////////////////////////////////////////////
// ExtractJsonData:

// TODO(crbug.com/831283): Add some ExtractJsonData tests.

////////////////////////////////////////////////////////////////////////////////
// ParseSuggestResults:

TEST(SearchSuggestionParserTest, ParseEmptyValueIsInvalid) {
  base::Value root_val;
  AutocompleteInput input;
  TestSchemeClassifier scheme_classifier;
  int default_result_relevance = 0;
  bool is_keyword_result = false;
  SearchSuggestionParser::Results results;
  ASSERT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      root_val, input, scheme_classifier, default_result_relevance,
      is_keyword_result, &results));
}

TEST(SearchSuggestionParserTest, ParseNonSuggestionValueIsInvalid) {
  std::string json_data = R"([{"one": 1}])";
  base::Optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  AutocompleteInput input;
  TestSchemeClassifier scheme_classifier;
  int default_result_relevance = 0;
  bool is_keyword_result = false;
  SearchSuggestionParser::Results results;
  ASSERT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, default_result_relevance,
      is_keyword_result, &results));
}

TEST(SearchSuggestionParserTest, ParseSuggestResults) {
  std::string json_data = R"([
      "chris",
      ["christmas", "christopher doe"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:fieldtrialtriggered": true,
        "google:suggestdetail": [{
          }, {
            "a": "American author",
            "dc": "#424242",
            "i": "http://example.com/a.png",
            "q": "gs_ssp=abc",
            "t": "Christopher Doe"
          }],
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "ENTITY"],
        "google:verbatimrelevance": 851
      }])";
  base::Optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(base::ASCIIToUTF16("chris"),
                          metrics::OmniboxEventProto::NTP, scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));
  // We have "google:suggestrelevance".
  ASSERT_EQ(true, results.relevances_from_server);
  // We have "google:fieldtrialtriggered".
  ASSERT_EQ(true, results.field_trial_triggered);
  // The "google:verbatimrelevance".
  ASSERT_EQ(851, results.verbatim_relevance);
  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(base::ASCIIToUTF16("christmas"), suggestion_result.suggestion());
    ASSERT_EQ(base::ASCIIToUTF16(""), suggestion_result.annotation());
    // This entry has no image.
    ASSERT_EQ("", suggestion_result.image_dominant_color());
    ASSERT_EQ("", suggestion_result.image_url());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(base::ASCIIToUTF16("christopher doe"),
              suggestion_result.suggestion());
    ASSERT_EQ(base::ASCIIToUTF16("American author"),
              suggestion_result.annotation());
    ASSERT_EQ("#424242", suggestion_result.image_dominant_color());
    ASSERT_EQ("http://example.com/a.png", suggestion_result.image_url());
  }
}
