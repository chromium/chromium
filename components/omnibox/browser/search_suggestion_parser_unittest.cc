// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/entity_info.pb.h"

namespace {

std::string SerializeAndEncodeEntityInfo(
    const omnibox::EntityInfo& entity_info) {
  std::string serialized_entity_info;
  entity_info.SerializeToString(&serialized_entity_info);
  std::string encoded_entity_info;
  base::Base64Encode(serialized_entity_info, &encoded_entity_info);
  return encoded_entity_info;
}

std::string SerializeAndEncodeGroupsInfo(
    const omnibox::GroupsInfo& groups_info) {
  std::string serialized_groups_info;
  groups_info.SerializeToString(&serialized_groups_info);
  std::string encoded_groups_info;
  base::Base64Encode(serialized_groups_info, &encoded_groups_info);
  return encoded_groups_info;
}

// (Rudimentary) mechanism comparing two protobuf MessageLite objects.
// This mechanism should be sufficient as long as compared objects don't host
// any maps.
// TODO(ender): Improve the mechanism to be smarter about checking individual
// fields and their values.
bool ProtosAreEqual(const google::protobuf::MessageLite& actual,
                    const google::protobuf::MessageLite& expected) {
  return (actual.GetTypeName() == expected.GetTypeName()) &&
         (actual.SerializeAsString() == expected.SerializeAsString());
}

}  // namespace

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
            "zae": "/m/065xxm",
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
    // This entry has no entity data
    ASSERT_TRUE(ProtosAreEqual(suggestion_result.entity_info(),
                               omnibox::EntityInfo::default_instance()));
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(u"christopher doe", suggestion_result.suggestion());
    ASSERT_EQ(u"American author", suggestion_result.annotation());
    ASSERT_EQ("/m/065xxm", suggestion_result.entity_info().entity_id());
    ASSERT_EQ("#424242", suggestion_result.entity_info().dominant_color());
    ASSERT_EQ("http://example.com/a.png",
              suggestion_result.entity_info().image_url());
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

    // Suggestion group headers, original group ids, sections, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    ASSERT_EQ(
        "Recent Searches",
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .header_text());
    ASSERT_EQ(
        omnibox::SECTION_REMOTE_ZPS_1,
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .section());
    ASSERT_EQ(
        omnibox::GroupConfig_Visibility_HIDDEN,
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .visibility());

    ASSERT_EQ("Recommended for you",
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2,
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .section());

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(static_cast<omnibox::GroupId>(40009),
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
            "40009": 123
          },
          "h":[40000, "40008", 40009]
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

    // Suggestion group headers, original group ids, sections, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    ASSERT_EQ(
        "Recommended for you",
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .header_text());
    ASSERT_EQ(
        omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .visibility());
    ASSERT_EQ(
        omnibox::SECTION_REMOTE_ZPS_1,
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .section());

    ASSERT_EQ(
        "Recent Searches",
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .header_text());
    ASSERT_EQ(
        omnibox::SECTION_REMOTE_ZPS_2,
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .section());
    ASSERT_EQ(
        omnibox::GroupConfig_Visibility_HIDDEN,
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST]
            .visibility());

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
              *results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(static_cast<omnibox::GroupId>(40009),
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
            "40009":"NOT RECOMMENDED FOR YOU"
          },
          "h":[40007, "40008", "garbage_non_int"]
        },
        "google:suggestdetail":[
          {
            "zl":40008
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

    // Suggestion group headers, original group ids, sections, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(3U, results.suggestion_groups_map.size());

    ASSERT_EQ(
        "Recommended for you",
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .header_text());
    ASSERT_EQ(
        omnibox::SECTION_REMOTE_ZPS_1,
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .section());
    ASSERT_EQ(
        omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED]
            .visibility());

    ASSERT_EQ("Related Searches",
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_HIDDEN,
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2,
              results
                  .suggestion_groups_map
                      [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS]
                  .section());

    ASSERT_EQ(
        "NOT RECOMMENDED FOR YOU",
        results.suggestion_groups_map[omnibox::GROUP_TRENDS].header_text());
    ASSERT_EQ(
        omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
        results.suggestion_groups_map[omnibox::GROUP_TRENDS].visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_3,
              results.suggestion_groups_map[omnibox::GROUP_TRENDS].section());

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
              *results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(static_cast<omnibox::GroupId>(40000),
              results.suggest_results[3].suggestion_group_id());
  }
  {
    std::string json_data = R"([
    "",
    [
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7"
    ],
    [
      "",
      "",
      "",
      "",
      "",
      "",
      ""
    ],
    [],
    {
      "google:clientdata":{
        "bpc":false,
        "tlw":false
      },
      "google:headertexts":{
        "a":{
          "40000":"1",
          "40001":"2",
          "40002":"3",
          "40003":"4",
          "40004":"5",
          "40005":"6",
          "40006":"7"
        },
        "h":[
          40005,
          "40006",
          "garbage_non_int"
        ]
      },
      "google:suggestdetail":[
        {
          "zl":40000
        },
        {
          "zl":40001
        },
        {
          "zl":40002
        },
        {
          "zl":40003
        },
        {
          "zl":40004
        },
        {
          "zl":40005
        },
        {
          "zl":40006
        }
      ],
      "google:suggestrelevance":[
        611,
        610,
        609,
        608,
        607,
        606,
        605
      ],
      "google:suggesttype":[
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY"
      ]
    }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Suggestion group headers, original group ids, sections, and default
    // visibilities are correctly parsed and populated.
    ASSERT_EQ(6U, results.suggestion_groups_map.size());
    ASSERT_EQ(7U, results.suggest_results.size());
    ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
              *results.suggest_results[0].suggestion_group_id());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[1].suggestion_group_id());
    ASSERT_EQ(omnibox::GROUP_TRENDS_ENTITY_CHIPS,
              *results.suggest_results[3].suggestion_group_id());
    ASSERT_EQ(omnibox::GROUP_RELATED_QUERIES,
              *results.suggest_results[4].suggestion_group_id());
    ASSERT_EQ(omnibox::GROUP_VISITED_DOC_RELATED,
              *results.suggest_results[5].suggestion_group_id());
    ASSERT_EQ(static_cast<omnibox::GroupId>(40006),
              results.suggest_results[6].suggestion_group_id());
  }
}

TEST(SearchSuggestionParserTest, ParseSuggestionEntityInfo) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"the m", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  std::string json_data = R"([
    "the m",
    ["the menu", "the menu", "the midnight club"],
    ["", "", ""],
    [],
    {
      "google:clientdata": {
        "bpc": false,
        "tlw": false
      },
      "google:suggestdetail": [
        {},
        {
          "a": "2022 film",
          "dc": "#424242",
          "i": "https://encrypted-tbn0.gstatic.com/images?q=the+menu",
          "q": "gs_ssp=eJzj4tVP1zc0LCwoKssryyg3YPTiKMlIVchNzSsFAGrSCGQ",
          "t": "The Menu",
          "zae": "/g/11qprvnvhw"
        },
        {
          "a": "Thriller series",
          "dc": "#283e75",
          "i": "https://encrypted-tbn0.gstatic.com/images?q=the+midnight+club",
          "q": "gs_ssp=eJzj4tVP1zc0zMqrNCvJNkwyYPQSLMlIVcjNTMnLTM8oUUjOKU0CALmyCz8",
          "t": "The Midnight Club",
          "zae": "/g/11jny6tk1b"
        }
      ],
      "google:suggestrelevance": [701, 700, 553],
      "google:suggestsubtypes": [
        [512, 433, 131, 355],
        [131, 433, 512],
        [512, 433]
      ],
      "google:suggesttype": ["QUERY", "ENTITY", "ENTITY"],
      "google:verbatimrelevance": 851
    }])";

  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);

  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_EQ(3U, results.suggest_results.size());

  // For each suggestion, verify that the JSON fields were correctly parsed.
  ASSERT_EQ(u"the menu", results.suggest_results[0].suggestion());
  ASSERT_TRUE(ProtosAreEqual(results.suggest_results[0].entity_info(),
                             omnibox::EntityInfo::default_instance()));
  ASSERT_EQ(u"", results.suggest_results[0].annotation());
  // Empty "t" value from server results in suggestion being used instead.
  ASSERT_EQ(u"the menu", results.suggest_results[0].match_contents());

  ASSERT_EQ(u"the menu", results.suggest_results[1].suggestion());
  ASSERT_EQ(u"2022 film", results.suggest_results[1].annotation());
  ASSERT_EQ("#424242",
            results.suggest_results[1].entity_info().dominant_color());
  ASSERT_EQ(
      "https://encrypted-tbn0.gstatic.com/"
      "images?q=the+menu",
      results.suggest_results[1].entity_info().image_url());
  ASSERT_EQ(
      "gs_ssp=eJzj4tVP1zc0LCwoKssryyg3YPTiKMlIVchNzSsFAGrSCGQ",
      results.suggest_results[1].entity_info().suggest_search_parameters());
  ASSERT_EQ(u"The Menu", results.suggest_results[1].match_contents());
  ASSERT_EQ("/g/11qprvnvhw",
            results.suggest_results[1].entity_info().entity_id());

  ASSERT_EQ(u"the midnight club", results.suggest_results[2].suggestion());
  ASSERT_EQ(u"Thriller series", results.suggest_results[2].annotation());
  ASSERT_EQ("#283e75",
            results.suggest_results[2].entity_info().dominant_color());
  ASSERT_EQ(
      "https://encrypted-tbn0.gstatic.com/"
      "images?q=the+midnight+club",
      results.suggest_results[2].entity_info().image_url());
  ASSERT_EQ(
      "gs_ssp=eJzj4tVP1zc0zMqrNCvJNkwyYPQSLMlIVcjNTMnLTM8oUUjOKU0CALmyCz8",
      results.suggest_results[2].entity_info().suggest_search_parameters());
  ASSERT_EQ(u"The Midnight Club", results.suggest_results[2].match_contents());
  ASSERT_EQ("/g/11jny6tk1b",
            results.suggest_results[2].entity_info().entity_id());
}

TEST(SearchSuggestionParserTest, ParseSuggestionGroupInfo_FromProto) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  {
    omnibox::GroupsInfo groups_info;
    auto* group_configs_map = groups_info.mutable_group_configs();
    auto& group_config_1 = (*group_configs_map)
        [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    group_config_1.set_header_text("Related Entities");
    auto& group_config_2 = (*group_configs_map)[omnibox::GROUP_TRENDS];
    group_config_2.set_header_text("Trending Searches");
    group_config_2.set_visibility(omnibox::GroupConfig_Visibility_HIDDEN);

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
            "10000":"Related Entities",
            "10001":"Trending Searches",
            "40000":"Recent Searches"
          },
          "h":[10000, "10001"]
        },
        "google:groupsinfo": ")" +
                            SerializeAndEncodeGroupsInfo(groups_info) + R"(",
        "google:suggestdetail":[
          {
          },
          {
            "zl":10001
          },
          {
            "zl":10002
          },
          {
            "zl":40000
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

    // Ensure suggestion groups are correctly parsed from the serialized proto.
    ASSERT_EQ(2U, results.suggestion_groups_map.size());

    const auto& group_1 =
        results.suggestion_groups_map
            [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    ASSERT_EQ("Related Entities", group_1.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
              group_1.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_1, group_1.section());

    const auto& group_2 = results.suggestion_groups_map[omnibox::GROUP_TRENDS];
    ASSERT_EQ("Trending Searches", group_2.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_HIDDEN, group_2.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2, group_2.section());

    // Ensure suggestion group IDs are correctly set in the suggestions.
    ASSERT_EQ(4U, results.suggest_results.size());

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_TRENDS,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    ASSERT_EQ(static_cast<omnibox::GroupId>(40000),
              results.suggest_results[3].suggestion_group_id());
  }
  {
    omnibox::GroupsInfo groups_info;
    auto* group_configs_map = groups_info.mutable_group_configs();
    auto& group_config_1 = (*group_configs_map)
        [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    group_config_1.set_header_text("Related Entities");
    auto& group_config_2 = (*group_configs_map)[omnibox::GROUP_TRENDS];
    group_config_2.set_header_text("Trending Searches");
    group_config_2.set_visibility(omnibox::GroupConfig_Visibility_HIDDEN);
    auto& group_config_3 =
        (*group_configs_map)[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    group_config_3.set_header_text("Recent Searches");

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
            "10000":"Related Entities",
            "10001":"Trending Searches"
          },
          "h":[10000, "10001"]
        },
        "google:groupsinfo": ")" +
                            SerializeAndEncodeGroupsInfo(groups_info) + R"(",
        "google:suggestdetail":[
          {
          },
          {
            "zl":10002
          },
          {
            "zl":10001
          },
          {
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

    // Ensure suggestion groups are correctly parsed from the serialized proto.
    ASSERT_EQ(3U, results.suggestion_groups_map.size());

    const auto& group_1 = results.suggestion_groups_map[omnibox::GROUP_TRENDS];
    ASSERT_EQ("Trending Searches", group_1.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_HIDDEN, group_1.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_1, group_1.section());

    const auto& group_2 =
        results.suggestion_groups_map
            [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    ASSERT_EQ("Related Entities", group_2.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
              group_2.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2, group_2.section());

    const auto& group_3 =
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    ASSERT_EQ("Recent Searches", group_3.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_3, group_3.section());

    // Ensure suggestion group IDs are correctly set in the suggestions.
    ASSERT_EQ(4U, results.suggest_results.size());

    ASSERT_EQ(u"los angeles", results.suggest_results[0].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"san diego", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_TRENDS,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"las vegas", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"san francisco", results.suggest_results[3].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[3].suggestion_group_id());
  }
  {
    omnibox::GroupsInfo groups_info;
    auto* group_configs_map = groups_info.mutable_group_configs();
    auto& group_config_1 =
        (*group_configs_map)[omnibox::GROUP_PREVIOUS_SEARCH_RELATED];
    group_config_1.set_header_text("Related Searches");
    auto& group_config_2 = (*group_configs_map)
        [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    group_config_2.set_header_text("Related Entities");
    auto& group_config_3 = (*group_configs_map)[omnibox::GROUP_TRENDS];
    group_config_3.set_header_text("Trending Searches");
    auto& group_config_4 =
        (*group_configs_map)[omnibox::GROUP_TRENDS_ENTITY_CHIPS];
    group_config_4.set_header_text("Trending Entities");
    auto& group_config_5 = (*group_configs_map)[omnibox::GROUP_RELATED_QUERIES];
    group_config_5.set_header_text("Related Questions");
    auto& group_config_6 =
        (*group_configs_map)[omnibox::GROUP_VISITED_DOC_RELATED];
    group_config_6.set_header_text("Related To Websites");
    auto& group_config_7 =
        (*group_configs_map)[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    group_config_7.set_header_text("Recent Searches");
    auto& group_config_8 =
        (*group_configs_map)[omnibox::GROUP_POLARIS_RESERVED_MAX];
    group_config_8.set_header_text("Uknown Group");

    std::string json_data = R"([
    "",
    [
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8"
    ],
    [
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      ""
    ],
    [],
    {
      "google:clientdata":{
        "bpc":false,
        "tlw":false
      },
      "google:groupsinfo": ")" +
                            SerializeAndEncodeGroupsInfo(groups_info) + R"(",
      "google:suggestdetail":[
        {
          "zl":10000
        },
        {
          "zl":10001
        },
        {
          "zl":10002
        },
        {
          "zl":10003
        },
        {
          "zl":10004
        },
        {
          "zl":10005
        },
        {
          "zl":40000
        },
        {
        }
      ],
      "google:suggestrelevance":[
        611,
        610,
        609,
        608,
        607,
        606,
        605,
        604
      ],
      "google:suggesttype":[
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY",
        "QUERY"
      ]
    }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Ensure suggestion groups are correctly parsed from the serialized proto.
    ASSERT_EQ(8U, results.suggestion_groups_map.size());

    const auto& group_1 =
        results.suggestion_groups_map[omnibox::GROUP_PREVIOUS_SEARCH_RELATED];
    ASSERT_EQ("Related Searches", group_1.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_1, group_1.section());

    const auto& group_2 =
        results.suggestion_groups_map
            [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    ASSERT_EQ("Related Entities", group_2.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2, group_2.section());

    const auto& group_3 = results.suggestion_groups_map[omnibox::GROUP_TRENDS];
    ASSERT_EQ("Trending Searches", group_3.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_3, group_3.section());

    const auto& group_4 =
        results.suggestion_groups_map[omnibox::GROUP_TRENDS_ENTITY_CHIPS];
    ASSERT_EQ("Trending Entities", group_4.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_4, group_4.section());

    const auto& group_5 =
        results.suggestion_groups_map[omnibox::GROUP_RELATED_QUERIES];
    ASSERT_EQ("Related Questions", group_5.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_5, group_5.section());

    const auto& group_6 =
        results.suggestion_groups_map[omnibox::GROUP_VISITED_DOC_RELATED];
    ASSERT_EQ("Related To Websites", group_6.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_6, group_6.section());

    const auto& group_7 =
        results.suggestion_groups_map[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    ASSERT_EQ("Recent Searches", group_7.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_7, group_7.section());

    const auto& group_8 =
        results.suggestion_groups_map[omnibox::GROUP_POLARIS_RESERVED_MAX];
    ASSERT_EQ("Uknown Group", group_8.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_8, group_8.section());

    // Ensure suggestion group IDs are correctly set in the suggestions.
    ASSERT_EQ(8U, results.suggest_results.size());

    ASSERT_EQ(u"1", results.suggest_results[0].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
              *results.suggest_results[0].suggestion_group_id());

    ASSERT_EQ(u"2", results.suggest_results[1].suggestion());
    ASSERT_EQ(omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
              *results.suggest_results[1].suggestion_group_id());

    ASSERT_EQ(u"3", results.suggest_results[2].suggestion());
    ASSERT_EQ(omnibox::GROUP_TRENDS,
              *results.suggest_results[2].suggestion_group_id());

    ASSERT_EQ(u"4", results.suggest_results[3].suggestion());
    ASSERT_EQ(omnibox::GROUP_TRENDS_ENTITY_CHIPS,
              *results.suggest_results[3].suggestion_group_id());

    ASSERT_EQ(u"5", results.suggest_results[4].suggestion());
    ASSERT_EQ(omnibox::GROUP_RELATED_QUERIES,
              *results.suggest_results[4].suggestion_group_id());

    ASSERT_EQ(u"6", results.suggest_results[5].suggestion());
    ASSERT_EQ(omnibox::GROUP_VISITED_DOC_RELATED,
              *results.suggest_results[5].suggestion_group_id());

    ASSERT_EQ(u"7", results.suggest_results[6].suggestion());
    ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
              *results.suggest_results[6].suggestion_group_id());

    ASSERT_EQ(u"8", results.suggest_results[7].suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(absl::nullopt, results.suggest_results[7].suggestion_group_id());
  }
}

TEST(SearchSuggestionParserTest, ParseSuggestionEntityInfo_FromProto) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"the m", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  // Parse EntityInfo data from properly encoded (base64) proto field.
  {
    omnibox::EntityInfo first_entity_info;
    first_entity_info.set_annotation("2022 film");
    first_entity_info.set_dominant_color("#424242");
    first_entity_info.set_image_url(
        "https://encrypted-tbn0.gstatic.com/"
        "images?q=the+menu");
    first_entity_info.set_suggest_search_parameters(
        "gs_ssp=eJzj4tVP1zc0LCwoKssryyg3YPTiKMlIVchNzSsFAGrSCGQ");
    first_entity_info.set_name("The Menu");
    first_entity_info.set_entity_id("/g/11qprvnvhw");

    omnibox::EntityInfo second_entity_info;
    second_entity_info.set_annotation("Thriller series");
    second_entity_info.set_dominant_color("#283e75");
    second_entity_info.set_image_url(
        "https://encrypted-tbn0.gstatic.com/"
        "images?q=the+midnight+club");
    second_entity_info.set_suggest_search_parameters(
        "gs_ssp=eJzj4tVP1zc0zMqrNCvJNkwyYPQSLMlIVcjNTMnLTM8oUUjOKU0CALmyCz8");
    second_entity_info.set_name("The Midnight Club");
    second_entity_info.set_entity_id("/g/11jny6tk1b");

    std::string json_data = R"([
      "the m",
      ["the menu", "the menu", "the midnight club"],
      ["", "", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:suggestdetail": [
          {},
          {
            "google:entityinfo": ")" +
                            SerializeAndEncodeEntityInfo(first_entity_info) +
                            R"("
          },
          {
            "google:entityinfo": ")" +
                            SerializeAndEncodeEntityInfo(second_entity_info) +
                            R"("
          }
        ],
        "google:suggestrelevance": [701, 700, 553],
        "google:suggestsubtypes": [
          [512, 433, 131, 355],
          [131, 433, 512],
          [512, 433]
        ],
        "google:suggesttype": ["QUERY", "ENTITY", "ENTITY"],
        "google:verbatimrelevance": 851
      }])";

    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    ASSERT_EQ(3U, results.suggest_results.size());

    // For each suggestion, verify that the JSON fields were correctly parsed.
    ASSERT_EQ(u"the menu", results.suggest_results[0].suggestion());
    ASSERT_EQ(u"", results.suggest_results[0].annotation());
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[0].entity_info(),
                               omnibox::EntityInfo::default_instance()));
    ASSERT_TRUE(results.suggest_results[0].entity_info().image_url().empty());
    // Empty "t" value from server results in suggestion being used instead.
    ASSERT_EQ(u"the menu", results.suggest_results[0].match_contents());

    ASSERT_EQ(u"the menu", results.suggest_results[1].suggestion());
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[1].entity_info(),
                               first_entity_info));

    ASSERT_EQ(u"the midnight club", results.suggest_results[2].suggestion());
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[2].entity_info(),
                               second_entity_info));
  }

  // If possible, fall back to individual JSON fields when attempting to parse
  // EntityInfo data from garbled proto field.
  {
    std::string json_data = R"([
      "the m",
      ["the menu", "the menu", "the midnight club"],
      ["", "", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:suggestdetail": [
          {},
          {
            "google:entityinfo": "<< invalid format >>",
            "a": "2022 film",
            "dc": "#424242",
            "i": "https://encrypted-tbn0.gstatic.com/images?q=the+menu",
            "q": "gs_ssp=eJzj4tVP1zc0LCwoKssryyg3YPTiKMlIVchNzSsFAGrSCGQ",
            "t": "The Menu",
            "zae": "/g/11qprvnvhw"
          },
          {
            "google:entityinfo": "<< invalid format >>",
            "a": "Thriller series",
            "dc": "#283e75",
            "i": "https://encrypted-tbn0.gstatic.com/images?q=the+midnight+club",
            "q": "gs_ssp=eJzj4tVP1zc0zMqrNCvJNkwyYPQSLMlIVcjNTMnLTM8oUUjOKU0CALmyCz8",
            "t": "The Midnight Club",
            "zae": "/g/11jny6tk1b"
          }
        ],
        "google:suggestrelevance": [701, 700, 553],
        "google:suggestsubtypes": [
          [512, 433, 131, 355],
          [131, 433, 512],
          [512, 433]
        ],
        "google:suggesttype": ["QUERY", "ENTITY", "ENTITY"],
        "google:verbatimrelevance": 851
      }])";

    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    ASSERT_EQ(3U, results.suggest_results.size());

    // For each suggestion, verify that the JSON fields were correctly parsed.
    ASSERT_EQ(u"the menu", results.suggest_results[0].suggestion());
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[0].entity_info(),
                               omnibox::EntityInfo::default_instance()));
    ASSERT_EQ(u"", results.suggest_results[0].annotation());
    // Empty "t" value from server results in suggestion being used instead.
    ASSERT_EQ(u"the menu", results.suggest_results[0].match_contents());

    ASSERT_EQ(u"the menu", results.suggest_results[1].suggestion());
    ASSERT_EQ(u"2022 film", results.suggest_results[1].annotation());
    ASSERT_EQ("#424242",
              results.suggest_results[1].entity_info().dominant_color());
    ASSERT_EQ(
        "https://encrypted-tbn0.gstatic.com/"
        "images?q=the+menu",
        results.suggest_results[1].entity_info().image_url());
    ASSERT_EQ(
        "gs_ssp=eJzj4tVP1zc0LCwoKssryyg3YPTiKMlIVchNzSsFAGrSCGQ",
        results.suggest_results[1].entity_info().suggest_search_parameters());
    ASSERT_EQ(u"The Menu", results.suggest_results[1].match_contents());
    ASSERT_EQ("/g/11qprvnvhw",
              results.suggest_results[1].entity_info().entity_id());

    ASSERT_EQ(u"the midnight club", results.suggest_results[2].suggestion());
    ASSERT_EQ(u"Thriller series", results.suggest_results[2].annotation());
    ASSERT_EQ("#283e75",
              results.suggest_results[2].entity_info().dominant_color());
    ASSERT_EQ(
        "https://encrypted-tbn0.gstatic.com/"
        "images?q=the+midnight+club",
        results.suggest_results[2].entity_info().image_url());
    ASSERT_EQ(
        "gs_ssp=eJzj4tVP1zc0zMqrNCvJNkwyYPQSLMlIVcjNTMnLTM8oUUjOKU0CALmyCz8",
        results.suggest_results[2].entity_info().suggest_search_parameters());
    ASSERT_EQ(u"The Midnight Club",
              results.suggest_results[2].match_contents());
    ASSERT_EQ("/g/11jny6tk1b",
              results.suggest_results[2].entity_info().entity_id());
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

TEST(SearchSuggestionParserTest, ParseCalculatorSuggestion) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"1 + 1", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  const std::string json_data = R"([
    "1 + 1",
    [
      "1 + 1",
      "= 2",
      "1 + 1"
    ],
    ["", "Calculator", ""],
    [],
    {
      "google:clientdata": {
        "bpc": false,
        "tlw": false
      },
      "google:suggestdetail": [
        {},
        {},
        {
          "a": "Song",
          "dc": "#424242",
          "i": "https://encrypted-tbn0.gstatic.com/images?q=song",
          "q": "gs_ssp=eJzj4tFP1zcsNjAzMykwKDZg9GI1VNBWMAQAOlEEsA",
          "t": "1+1",
          "zae": "/g/1s0664p0s"
        }
      ],
      "google:suggestrelevance": [1300, 1252, 1250],
      "google:suggestsubtypes": [
        [512, 355],
        [],
        [512]
      ],
      "google:suggesttype": [
        "QUERY",
        "CALCULATOR",
        "ENTITY"
      ],
      "google:verbatimrelevance": 1300
    }
  ])";

  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);

  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_EQ(3U, results.suggest_results.size());

  // Most fields for a verbatim suggestion should be empty.
  ASSERT_EQ(u"1 + 1", results.suggest_results[0].suggestion());
  ASSERT_TRUE(ProtosAreEqual(results.suggest_results[0].entity_info(),
                             omnibox::EntityInfo::default_instance()));
  ASSERT_EQ(u"", results.suggest_results[0].annotation());
  ASSERT_EQ(u"1 + 1", results.suggest_results[0].match_contents());

  // Calculator suggestions should have specific values for the |suggestion|,
  // |match_contents|, and |annotation| fields.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(u"2", results.suggest_results[1].suggestion());
  ASSERT_EQ(u"2", results.suggest_results[1].annotation());
  ASSERT_TRUE(ProtosAreEqual(results.suggest_results[1].entity_info(),
                             omnibox::EntityInfo::default_instance()));
  ASSERT_EQ(u"1 + 1", results.suggest_results[1].match_contents());
#else
  ASSERT_EQ(u"2", results.suggest_results[1].suggestion());
  ASSERT_EQ(u"", results.suggest_results[1].annotation());
  ASSERT_TRUE(ProtosAreEqual(results.suggest_results[1].entity_info(),
                             omnibox::EntityInfo::default_instance()));
  ASSERT_EQ(u"= 2", results.suggest_results[1].match_contents());
#endif

  // Entity data should be correctly sourced as usual.
  ASSERT_EQ(u"1 + 1", results.suggest_results[2].suggestion());
  ASSERT_EQ(u"Song", results.suggest_results[2].annotation());
  ASSERT_EQ("#424242",
            results.suggest_results[2].entity_info().dominant_color());
  ASSERT_EQ("https://encrypted-tbn0.gstatic.com/images?q=song",
            results.suggest_results[2].entity_info().image_url());
  ASSERT_EQ(
      "gs_ssp=eJzj4tFP1zcsNjAzMykwKDZg9GI1VNBWMAQAOlEEsA",
      results.suggest_results[2].entity_info().suggest_search_parameters());
  ASSERT_EQ(u"1+1", results.suggest_results[2].match_contents());
  ASSERT_EQ("/g/1s0664p0s",
            results.suggest_results[2].entity_info().entity_id());
}
