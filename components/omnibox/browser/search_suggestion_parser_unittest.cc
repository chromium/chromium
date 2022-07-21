// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  absl::optional<base::Value> manifest_value =
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
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
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
        "google:verbatimrelevance": 851,
        "google:experimentstats": [
          {"2":"0:67","4":10001},
          {"2":"54:67","4":10002},
          {"2":"0:54","4":10003}
          ]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"chris", metrics::OmniboxEventProto::NTP,
                          scheme_classifier);
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
  ASSERT_EQ(2U, results.suggest_results.size());
  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(u"christmas", suggestion_result.suggestion());
    ASSERT_EQ(u"", suggestion_result.annotation());
    // This entry has no image.
    ASSERT_EQ("", suggestion_result.image_dominant_color());
    ASSERT_EQ(GURL(), suggestion_result.image_url());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(u"christopher doe", suggestion_result.suggestion());
    ASSERT_EQ(u"American author", suggestion_result.annotation());
    ASSERT_EQ("#424242", suggestion_result.image_dominant_color());
    ASSERT_EQ(GURL("http://example.com/a.png"), suggestion_result.image_url());
  }
  ASSERT_EQ(3U, results.experiment_stats_v2s.size());
  {
    const auto& experiment_stats_v2 = results.experiment_stats_v2s[0];
    ASSERT_EQ(10001, experiment_stats_v2.type_int());
    ASSERT_EQ("0:67", experiment_stats_v2.string_value());
  }
  {
    const auto& experiment_stats_v2 = results.experiment_stats_v2s[1];
    ASSERT_EQ(10002, experiment_stats_v2.type_int());
    ASSERT_EQ("54:67", experiment_stats_v2.string_value());
  }
  {
    const auto& experiment_stats_v2 = results.experiment_stats_v2s[2];
    ASSERT_EQ(10003, experiment_stats_v2.type_int());
    ASSERT_EQ("0:54", experiment_stats_v2.string_value());
  }
}

// Tests that prerender hints can be parsed correctly.
TEST(SearchSuggestionParserTest, ParsePrerenderSuggestion) {
  std::string json_data = R"([
      "pre",
      ["prefetch","prerender"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "pre": 1
        }
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"pre", metrics::OmniboxEventProto::BLANK,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));
  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(u"prefetch", suggestion_result.suggestion());
    EXPECT_FALSE(suggestion_result.should_prerender());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(u"prerender", suggestion_result.suggestion());
    EXPECT_TRUE(suggestion_result.should_prerender());
  }
}

// Tests that both prefetch and prerender hints can be parsed correctly.
TEST(SearchSuggestionParserTest, ParseBothPrefetchAndPrerenderSuggestion) {
  std::string json_data = R"([
      "pre",
      ["prefetch","prerender"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "phi": 0,
          "pre": 1
        }
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"pre", metrics::OmniboxEventProto::BLANK,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));
  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(u"prefetch", suggestion_result.suggestion());
    EXPECT_FALSE(suggestion_result.should_prerender());
    EXPECT_TRUE(suggestion_result.should_prefetch());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(u"prerender", suggestion_result.suggestion());
    EXPECT_TRUE(suggestion_result.should_prerender());
    EXPECT_FALSE(suggestion_result.should_prefetch());
  }
}

TEST(SearchSuggestionParserTest, SuggestClassification) {
  SearchSuggestionParser::SuggestResult result(
      u"foobar", AutocompleteMatchType::SEARCH_SUGGEST, {}, false, 400, true,
      std::u16string());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());

  // Nothing should be bolded for ZeroSuggest classified input.
  result.ClassifyMatchContents(true, std::u16string());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kNone = {
      {0, AutocompleteMatch::ACMatchClassification::NONE}};
  EXPECT_EQ(kNone, result.match_contents_class());

  // Test a simple case of bolding half the text.
  result.ClassifyMatchContents(false, u"foo");
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kHalfBolded = {
      {0, AutocompleteMatch::ACMatchClassification::NONE},
      {3, AutocompleteMatch::ACMatchClassification::MATCH}};
  EXPECT_EQ(kHalfBolded, result.match_contents_class());

  // Test the edge case that if we forbid bolding all, and then reclassifying
  // would otherwise bold-all, we leave the existing classifications alone.
  // This is weird, but it's in the function contract, and is useful for
  // flicker-free search suggestions as the user types.
  result.ClassifyMatchContents(false, u"apple");
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  EXPECT_EQ(kHalfBolded, result.match_contents_class());

  // And finally, test the case where we do allow bolding-all.
  result.ClassifyMatchContents(true, u"apple");
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kBoldAll = {
      {0, AutocompleteMatch::ACMatchClassification::MATCH}};
  EXPECT_EQ(kBoldAll, result.match_contents_class());
}

TEST(SearchSuggestionParserTest, NavigationClassification) {
  TestSchemeClassifier scheme_classifier;
  SearchSuggestionParser::NavigationResult result(
      scheme_classifier, GURL("https://news.google.com/"),
      AutocompleteMatchType::Type::NAVSUGGEST, {}, std::u16string(),
      std::string(), false, 400, true, u"google");
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kBoldMiddle = {
      {0, AutocompleteMatch::ACMatchClassification::URL},
      {5, AutocompleteMatch::ACMatchClassification::URL |
              AutocompleteMatch::ACMatchClassification::MATCH},
      {11, AutocompleteMatch::ACMatchClassification::URL}};
  EXPECT_EQ(kBoldMiddle, result.match_contents_class());

  // Reclassifying in a way that would cause bold-none if it's disallowed should
  // do nothing.
  result.CalculateAndClassifyMatchContents(false, u"term not found");
  EXPECT_EQ(kBoldMiddle, result.match_contents_class());

  // Test the allow bold-nothing case too.
  result.CalculateAndClassifyMatchContents(true, u"term not found");
  const ACMatchClassifications kAnnotateUrlOnly = {
      {0, AutocompleteMatch::ACMatchClassification::URL}};
  EXPECT_EQ(kAnnotateUrlOnly, result.match_contents_class());

  // Nothing should be bolded for ZeroSuggest classified input.
  result.CalculateAndClassifyMatchContents(true, std::u16string());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kNone = {
      {0, AutocompleteMatch::ACMatchClassification::NONE}};
  EXPECT_EQ(kNone, result.match_contents_class());
}

TEST(SearchSuggestionParserTest, ParseSuggestionGroupInfo) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  {
    std::string json_data = R"([
      "",
      ["los angeles", "san diego", "las vegas", "san francisco"],
      ["", "history", "", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:headertexts":{
          "a":{
            "40000":"Recent Searches",
            "40008":"Recommended for you",
            "garbage_non_int":"NOT RECOMMENDED FOR YOU"
          },
          "h":[40000, "40008", "garbage_non_int"]
        },
        "google:suggestdetail":[
          {
          },
          {
            "zl":40000
          },
          {
            "zl":40008
          },
          {
            "zl":40009
          }
        ],
        "google:suggestrelevance": [607, 606, 605, 604],
        "google:suggesttype": ["QUERY", "PERSONALIZED_QUERY", "QUERY", "QUERY"]
      }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Suggestion group headers, original group ids, priorities, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    ASSERT_EQ(
        u"Recent Searches",
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .header);
    ASSERT_EQ(
        40000,
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .original_group_id.value());
    ASSERT_EQ(
        SuggestionGroupPriority::kRemoteZeroSuggest1,
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .priority);
    ASSERT_TRUE(
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .hidden);

    ASSERT_EQ(u"Recommended for you",
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                  .header);
    ASSERT_EQ(40008, results
                         .suggestion_groups_map
                             [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                         .original_group_id.value());
    ASSERT_FALSE(results
                     .suggestion_groups_map
                         [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                     .hidden);
    ASSERT_EQ(SuggestionGroupPriority::kRemoteZeroSuggest2,
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                  .priority);

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(SuggestionGroupId::kPersonalizedZeroSuggest,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest2,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest3,
              results.suggest_results[3].suggestion_group_id());
  }
  {
    std::string json_data = R"([
      "",
      ["los angeles", "san diego", "las vegas", "san francisco"],
      ["", "", "history", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:headertexts":{
          "a":{
            "40000":"Recent Searches",
            "40008":"Recommended for you",
            "garbage_non_int":"NOT RECOMMENDED FOR YOU"
          },
          "h":[40000, "40008", "garbage_non_int"]
        },
        "google:suggestdetail":[
          {
            "zl":40008
          },
          {
            "zl":40008
          },
          {
            "zl":40000
          },
          {
            "zl":40009
          }
        ],
        "google:suggestrelevance": [607, 606, 605, 604],
        "google:suggesttype": ["QUERY", "QUERY", "PERSONALIZED_QUERY", "QUERY"]
      }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Suggestion group headers, original group ids, priorities, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    ASSERT_EQ(u"Recommended for you",
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                  .header);
    ASSERT_EQ(40008, results
                         .suggestion_groups_map
                             [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                         .original_group_id.value());
    ASSERT_FALSE(results
                     .suggestion_groups_map
                         [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                     .hidden);
    ASSERT_EQ(SuggestionGroupPriority::kRemoteZeroSuggest1,
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                  .priority);

    ASSERT_EQ(
        u"Recent Searches",
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .header);
    ASSERT_EQ(
        40000,
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .original_group_id.value());
    ASSERT_EQ(
        SuggestionGroupPriority::kRemoteZeroSuggest2,
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .priority);
    ASSERT_TRUE(
        results
            .suggestion_groups_map[SuggestionGroupId::kPersonalizedZeroSuggest]
            .hidden);

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest1,
              *results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest1,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(SuggestionGroupId::kPersonalizedZeroSuggest,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest3,
              results.suggest_results[3].suggestion_group_id());
  }
  {
    std::string json_data = R"([
      "",
      ["los angeles", "san diego", "las vegas", "san francisco"],
      ["", "", "", "history"],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:headertexts":{
          "a":{
            "40007":"Related Searches",
            "40008":"Recommended for you",
            "garbage_non_int":"NOT RECOMMENDED FOR YOU"
          },
          "h":[40007, "40008", "garbage_non_int"]
        },
        "google:suggestdetail":[
          {
          },
          {
            "zl":40007
          },
          {
            "zl":40008
          },
          {
            "zl":40000
          }
        ],
        "google:suggestrelevance": [607, 606, 605, 604],
        "google:suggesttype": ["QUERY", "QUERY", "QUERY", "PERSONALIZED_QUERY"]
      }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Suggestion group headers, original group ids, priorities, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    ASSERT_EQ(u"Related Searches",
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                  .header);
    ASSERT_EQ(40007, results
                         .suggestion_groups_map
                             [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                         .original_group_id.value());
    ASSERT_EQ(SuggestionGroupPriority::kRemoteZeroSuggest1,
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                  .priority);
    ASSERT_TRUE(results
                    .suggestion_groups_map
                        [SuggestionGroupId::kNonPersonalizedZeroSuggest1]
                    .hidden);

    ASSERT_EQ(u"Recommended for you",
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                  .header);
    ASSERT_EQ(40008, results
                         .suggestion_groups_map
                             [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                         .original_group_id.value());
    ASSERT_FALSE(results
                     .suggestion_groups_map
                         [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                     .hidden);
    ASSERT_EQ(SuggestionGroupPriority::kRemoteZeroSuggest2,
              results
                  .suggestion_groups_map
                      [SuggestionGroupId::kNonPersonalizedZeroSuggest2]
                  .priority);

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest1,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(SuggestionGroupId::kNonPersonalizedZeroSuggest2,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(SuggestionGroupId::kPersonalizedZeroSuggest,
              results.suggest_results[3].suggestion_group_id());
  }
}

TEST(SearchSuggestionParserTest, ParseValidSubtypes) {
  std::string json_data = R"([
      "",
      ["one", "two", "three", "four"],
      ["", "", "", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": [[1], [21, 22], [31, 32, 33], [44]],
        "google:suggestrelevance": [607, 606, 605, 604],
        "google:suggesttype": ["QUERY", "QUERY", "QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(u"one", suggestion_result.suggestion());
    ASSERT_THAT(suggestion_result.subtypes(), testing::ElementsAre(1));
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(u"two", suggestion_result.suggestion());
    ASSERT_THAT(suggestion_result.subtypes(), testing::ElementsAre(21, 22));
  }
  {
    const auto& suggestion_result = results.suggest_results[2];
    ASSERT_EQ(u"three", suggestion_result.suggestion());
    ASSERT_THAT(suggestion_result.subtypes(), testing::ElementsAre(31, 32, 33));
  }
  {
    const auto& suggestion_result = results.suggest_results[3];
    ASSERT_EQ(u"four", suggestion_result.suggestion());
    ASSERT_THAT(suggestion_result.subtypes(), testing::ElementsAre(44));
  }
}

TEST(SearchSuggestionParserTest, IgnoresExcessiveSubtypeEntries) {
  using testing::ElementsAre;
  std::string json_data = R"([
      "",
      ["one", "two"],
      ["", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": [[1], [2], [3]],
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_THAT(results.suggest_results[0].subtypes(), testing::ElementsAre(1));
  ASSERT_THAT(results.suggest_results[1].subtypes(), testing::ElementsAre(2));
}

TEST(SearchSuggestionParserTest, IgnoresMissingSubtypeEntries) {
  using testing::ElementsAre;
  std::string json_data = R"([
      "",
      ["one", "two", "three"],
      ["", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": [[1, 7]],
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_THAT(results.suggest_results[0].subtypes(),
              testing::ElementsAre(1, 7));
  ASSERT_TRUE(results.suggest_results[1].subtypes().empty());
  ASSERT_TRUE(results.suggest_results[2].subtypes().empty());
}

TEST(SearchSuggestionParserTest, IgnoresUnexpectedSubtypeValues) {
  using testing::ElementsAre;
  std::string json_data = R"([
      "",
      ["one", "two", "three", "four", "five"],
      ["", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": [[1, { "a":true} ], ["2", 7], 3, {}, [12]],
        "google:suggestrelevance": [607, 606, 605, 604, 603],
        "google:suggesttype": ["QUERY", "QUERY", "QUERY", "QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_THAT(results.suggest_results[0].subtypes(), testing::ElementsAre(1));
  ASSERT_THAT(results.suggest_results[1].subtypes(), testing::ElementsAre(7));
  ASSERT_TRUE(results.suggest_results[2].subtypes().empty());
  ASSERT_TRUE(results.suggest_results[3].subtypes().empty());
  ASSERT_THAT(results.suggest_results[4].subtypes(), testing::ElementsAre(12));
}

TEST(SearchSuggestionParserTest, IgnoresSubtypesIfNotAList) {
  using testing::ElementsAre;
  std::string json_data = R"([
      "",
      ["one", "two"],
      ["", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": { "a": 1, "b": 2 },
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_TRUE(results.suggest_results[0].subtypes().empty());
  ASSERT_TRUE(results.suggest_results[1].subtypes().empty());
}

TEST(SearchSuggestionParserTest, SubtypesWithEmptyArraysAreValid) {
  using testing::ElementsAre;
  std::string json_data = R"([
      "",
      ["one", "two"],
      ["", ""],
      [],
      {
        "google:clientdata": { "bpc": false, "tlw": false },
        "google:suggestsubtypes": [[], [3]],
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "QUERY"]
      }])";
  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_TRUE(results.suggest_results[0].subtypes().empty());
  ASSERT_THAT(results.suggest_results[1].subtypes(), testing::ElementsAre(3));
}

TEST(SearchSuggestionParserTest, FuzzTestCaseFailsGracefully) {
  // clang-format off
  std::string json_data = R"(["",[" "],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[{"t":"w","tt":4}]}},{"il":{"i":"","t":[{"t":"3","tt":1}]}}]},"ansb":"0"}]}])";
  // clang-format on

  // The original fuzz test case had a NUL (0) character at index 6 but it is
  // replaced with space (32) above for system interaction reasons (clipboard,
  // command line, and some editors shun null bytes). Test the fuzz case with
  // input that is byte-for-byte identical with https://crbug.com/1255312 data.
  json_data[6] = 0;

  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));
}

TEST(SearchSuggestionParserTest, BadAnswersFailGracefully) {
  // clang-format off
  std::vector<std::string> cases = {
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[{"t":"w","tt":4}]}},{"il":{"i":"","t":[{"t":"3","tt":1}]}}]},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[]}},{"il":{"i":"","t":[[]]}}]},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[]}},{"il":{"i":"","t":[[0]]}}]},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[]}},{"il":{"i":"","t":[""]}}]},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{"l":[{"il":{"t":[{"t":"w","tt":4}]}},{"il":{"i":"","t":[{"t":"3","tt":1}]}}]},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":[],"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":{},"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":0,"ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":"","ansb":"0"}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":"","ansb":{}}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":"","ansb":0}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":[{"ansa":"","ansb":[]}]}])",
    R"(["",[""],[],[],{"google:suggestdetail":""}])",
    R"(["",[""],[],[],{"google:suggestdetail":0}])",
    R"(["",[""],[],[],{"google:suggestdetail":{}}])",
  };
  // clang-format on
  for (std::string json_data : cases) {
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);
    TestSchemeClassifier scheme_classifier;
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                            scheme_classifier);
    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));
  }
}
