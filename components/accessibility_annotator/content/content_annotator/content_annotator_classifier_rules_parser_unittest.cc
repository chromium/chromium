// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(ContentAnnotatorClassifierRulesParserTest, ParseRulesFromJson_ValidJson) {
  std::string rules_json =
      R"Json(
      {"category1":[".*rule1.*",".*rule2.*"],
       "category2":[".*rule3.*",".*rule4.*"],
       "category3":["rule5"]
       })Json";
  base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(rules_json);
  EXPECT_THAT(rules["category1"], ElementsAre(".*rule1.*", ".*rule2.*"));
  EXPECT_THAT(rules["category2"], ElementsAre(".*rule3.*", ".*rule4.*"));
  EXPECT_THAT(rules["category3"], ElementsAre("rule5"));
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_CategoryWithEmptyRuleList) {
  std::string rules_json = R"Json({"category1":[]})Json";
  base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(rules_json);
  EXPECT_EQ(0u, rules.size());
}

TEST(ContentAnnotatorClassifierRulesParserTest, ParseRulesFromJson_EmptyJson) {
  base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(R"Json({})Json");
  EXPECT_THAT(rules, IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_InvalidJson) {
  EXPECT_THAT(ParseRulesFromJson("invalid json"), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_CategoryValueNotList) {
  std::string rules_json = R"Json({"category1":"not a list"})Json";
  EXPECT_THAT(ParseRulesFromJson(rules_json), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_RuleValueEmptyString) {
  std::string rules_json = R"Json({"category1":["rule1", ""]})Json";
  EXPECT_THAT(ParseRulesFromJson(rules_json), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_RuleValueNotString) {
  std::string rules_json = R"Json({"category1":["rule1", 123]})Json";
  EXPECT_THAT(ParseRulesFromJson(rules_json), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_TooManyCategories) {
  std::stringstream rules_stream;
  rules_stream << "{";
  for (size_t i = 0; i < kMaxRuleCategories + 1; ++i) {
    rules_stream << "\"category" << base::NumberToString(i) << "\":[\"rule\"],";
  }
  std::string rules_json = rules_stream.str();
  rules_json.pop_back();
  rules_json += "}";
  EXPECT_THAT(ParseRulesFromJson(rules_json), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRulesFromJson_TooManyRules) {
  std::stringstream rules_stream;
  rules_stream << "{\"category1\":[";
  for (size_t i = 0; i < kMaxRulesPerCategory + 1; ++i) {
    rules_stream << "\"rule" << base::NumberToString(i) << "\",";
  }
  std::string rules_json = rules_stream.str();
  rules_json.pop_back();
  rules_json += "]}";
  EXPECT_THAT(ParseRulesFromJson(rules_json), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRelevanceValuesFromJson_EmptyJson) {
  EXPECT_THAT(ParseRelevanceValuesFromJson(R"Json({})Json"), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRelevanceValuesFromJson_InvalidJson) {
  EXPECT_THAT(ParseRelevanceValuesFromJson("invalid json"), IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRelevanceValuesFromJson_ValidJson) {
  std::string json = R"Json({
    "category1": 1,
    "category2": 2,
    "category3": 3
  })Json";
  base::flat_map<std::string, ContentClassifierRelevance> relevance_values =
      ParseRelevanceValuesFromJson(json);
  ASSERT_EQ(3u, relevance_values.size());
  EXPECT_EQ(ContentClassifierRelevance::kLowContentRelevance,
            relevance_values["category1"]);
  EXPECT_EQ(ContentClassifierRelevance::kMediumContentRelevance,
            relevance_values["category2"]);
  EXPECT_EQ(ContentClassifierRelevance::kHighContentRelevance,
            relevance_values["category3"]);
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRelevanceValuesFromJson_InvalidValues) {
  std::string json = R"Json({
    "category1": 3,
    "category2": "not an int"
  })Json";
  base::flat_map<std::string, ContentClassifierRelevance> relevance_values =
      ParseRelevanceValuesFromJson(json);
  EXPECT_THAT(relevance_values, IsEmpty());
}

TEST(ContentAnnotatorClassifierRulesParserTest,
     ParseRelevanceValuesFromJson_OutOfRangeValue) {
  std::string json = R"Json({
    "category1": 50,
  })Json";
  base::flat_map<std::string, ContentClassifierRelevance> relevance_values =
      ParseRelevanceValuesFromJson(json);
  EXPECT_THAT(relevance_values, IsEmpty());
}

}  // namespace accessibility_annotator
