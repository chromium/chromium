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
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  absl::optional<base::Value::List> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeMalformedJsonIsInvalid) {
  std::string json_data = "} malformed json {";
  absl::optional<base::Value::List> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeJsonData) {
  std::string json_data = R"([{"one": 1}])";
  absl::optional<base::Value> manifest_value =
      base::JSONReader::Read(json_data);
  ASSERT_TRUE(manifest_value);
  absl::optional<base::Value::List> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
  ASSERT_EQ(*manifest_value, *result);
}

TEST(SearchSuggestionParserTest, DeserializeWithXssiGuard) {
  // For XSSI protection, non-json may precede the actual data.
  // Parsing fails at:       v         v
  std::string json_data = R"([non-json [prefix [{"one": 1}])";
  // Parsing succeeds at:                      ^

  absl::optional<base::Value::List> result =
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
  absl::optional<base::Value::List> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
}

////////////////////////////////////////////////////////////////////////////////
// ExtractJsonData:

// TODO(crbug.com/831283): Add some ExtractJsonData tests.

////////////////////////////////////////////////////////////////////////////////
// ParseSuggestResults:

TEST(SearchSuggestionParserTest, ParseEmptyValueIsInvalid) {
  base::Value::List root_val;
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
  ASSERT_TRUE(root_val.value().is_list());
  AutocompleteInput input;
  TestSchemeClassifier scheme_classifier;
  int default_result_relevance = 0;
  bool is_keyword_result = false;
  SearchSuggestionParser::Results results;
  ASSERT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier, default_result_relevance,
      is_keyword_result, &results));
}

TEST(SearchSuggestionParserTest, ParseSuggestResults) {
  omnibox::EntityInfo entity_info;
  entity_info.set_annotation("American author");
  entity_info.set_dominant_color("#424242");
  entity_info.set_image_url("http://example.com/a.png");
  entity_info.set_suggest_search_parameters("gs_ssp=abc");
  entity_info.set_name("Christopher Doe");
  entity_info.set_entity_id("/m/065xxm");

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
            "google:entityinfo": ")" +
                          SerializeAndEncodeEntityInfo(entity_info) +
                          R"("
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"chris", metrics::OmniboxEventProto::NTP,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"pre", metrics::OmniboxEventProto::BLANK,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"pre", metrics::OmniboxEventProto::BLANK,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
    ASSERT_TRUE(root_val.value().is_list());

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        root_val->GetList(), input, scheme_classifier,
        /*default_result_relevance=*/400,
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
    ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
              results.suggest_results[3].suggestion_group_id());
  }
  {
    omnibox::GroupsInfo groups_info;
    auto* group_configs_map = groups_info.mutable_group_configs();
    // Group 1
    auto& group_config_1 = (*group_configs_map)[omnibox::GROUP_TRENDS];
    group_config_1.set_header_text("Trending Searches");
    group_config_1.set_visibility(omnibox::GroupConfig_Visibility_HIDDEN);
    // Group 2
    auto& group_config_2 = (*group_configs_map)
        [omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS];
    group_config_2.set_header_text("Related Entities");
    // Group 3
    auto& group_config_3 =
        (*group_configs_map)[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    group_config_3.set_header_text("Recent Searches");
    // Group 4
    auto& group_config_4 =
        (*group_configs_map)[static_cast<omnibox::GroupId>(101)];
    group_config_4.set_header_text("Unrecognized Suggestions");

    std::string json_data = R"([
      "",
      ["los angeles", "san diego", "las vegas", "san francisco", "sacramento"],
      ["", "history", "", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
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
            "zl":102
          },
          {
            "zl":101
          }
        ],
        "google:suggestrelevance": [607, 606, 605, 604, 603],
        "google:suggesttype": ["QUERY", "QUERY", "QUERY", "QUERY", "QUERY"]
      }])";
    absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
    ASSERT_TRUE(root_val);
    ASSERT_TRUE(root_val.value().is_list());

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        root_val->GetList(), input, scheme_classifier,
        /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));

    // Ensure group configs are correctly parsed from the serialized proto.
    // group configs with invalid or unrecognized group IDs are dropped.
    ASSERT_EQ(3U, results.suggestion_groups_map.size());
    // Group 1
    const auto& group_1 =
        results.suggestion_groups_map.at(omnibox::GROUP_TRENDS);
    ASSERT_EQ("Trending Searches", group_1.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_HIDDEN, group_1.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_1, group_1.section());
    // Group 2
    const auto& group_2 = results.suggestion_groups_map.at(
        omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS);
    ASSERT_EQ("Related Entities", group_2.header_text());
    ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
              group_2.visibility());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_2, group_2.section());
    // Group 3
    const auto& group_3 = results.suggestion_groups_map.at(
        omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST);
    ASSERT_EQ("Recent Searches", group_3.header_text());
    ASSERT_EQ(omnibox::SECTION_REMOTE_ZPS_3, group_3.section());

    // Ensure suggestion group IDs are correctly set in the suggestions.
    ASSERT_EQ(5U, results.suggest_results.size());

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
    // This suggestion belongs to an unrecognized group.
    ASSERT_EQ(omnibox::GROUP_INVALID,
              results.suggest_results[3].suggestion_group_id());

    ASSERT_EQ(u"sacramento", results.suggest_results[4].suggestion());
    // This suggestion belongs to an unrecognized group.
    ASSERT_EQ(omnibox::GROUP_INVALID,
              results.suggest_results[4].suggestion_group_id());
  }
}

TEST(SearchSuggestionParserTest, ParseSuggestionEntityInfo) {
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
    ASSERT_TRUE(root_val.value().is_list());

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        root_val->GetList(), input, scheme_classifier,
        /*default_result_relevance=*/400,
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

  // Parse EntityInfo data from garbled proto field.
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
            "google:entityinfo": "<< invalid format >>"
          },
          {
            "google:entityinfo": "<< invalid format >>"
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
    ASSERT_TRUE(root_val.value().is_list());

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        root_val->GetList(), input, scheme_classifier,
        /*default_result_relevance=*/400,
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
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[1].entity_info(),
                               omnibox::EntityInfo::default_instance()));

    ASSERT_EQ(u"the midnight club", results.suggest_results[2].suggestion());
    ASSERT_TRUE(ProtosAreEqual(results.suggest_results[2].entity_info(),
                               omnibox::EntityInfo::default_instance()));
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
  ASSERT_TRUE(root_val.value().is_list());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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
    ASSERT_TRUE(root_val.value().is_list());
    TestSchemeClassifier scheme_classifier;
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                            scheme_classifier);
    SearchSuggestionParser::Results results;
    ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
        root_val->GetList(), input, scheme_classifier,
        /*default_result_relevance=*/400,
        /*is_keyword_result=*/false, &results));
  }
}

TEST(SearchSuggestionParserTest, ParseCalculatorSuggestion) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"1 + 1", metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  omnibox::EntityInfo entity_info;
  entity_info.set_annotation("Song");
  entity_info.set_dominant_color("#424242");
  entity_info.set_image_url("https://encrypted-tbn0.gstatic.com/images?q=song");
  entity_info.set_suggest_search_parameters(
      "gs_ssp=eJzj4tFP1zcsNjAzMykwKDZg9GI1VNBWMAQAOlEEsA");
  entity_info.set_name("1+1");
  entity_info.set_entity_id("/g/1s0664p0s");

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
          "google:entityinfo": ")" +
                                SerializeAndEncodeEntityInfo(entity_info) +
                                R"("
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
  ASSERT_TRUE(root_val.value().is_list());

  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
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

TEST(SearchSuggestionParserTest, ParseTailSuggestion) {
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"hobbit hole for sale in ",
                          metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);

  const std::string json_data = R"([
    "hobbit hole for sale in ",
    [
      "hobbit hole for sale in california"
    ],
    [
      ""
    ],
    [],
    {
      "google:clientdata": {
        "bpc": false,
        "tlw": false
      },
      "google:suggestdetail": [
        {
          "mp": "… ",
          "t": "in california"
        }
      ],
      "google:suggestrelevance": [
        601
      ],
      "google:suggestsubtypes": [
        [
          160
        ]
      ],
      "google:suggesttype": [
        "TAIL"
      ],
      "google:verbatimrelevance": 851
    }
  ])";

  absl::optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  ASSERT_TRUE(root_val.value().is_list());

  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  ASSERT_EQ(1U, results.suggest_results.size());
  ASSERT_EQ(u"hobbit hole for sale in california",
            results.suggest_results[0].suggestion());
  ASSERT_EQ(u"in california", results.suggest_results[0].match_contents());
  ASSERT_EQ(u"… ", results.suggest_results[0].match_contents_prefix());
}
