// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

namespace quick_answers {
namespace {
using base::Value;
}  // namespace

class DefinitionResultParserTest : public testing::Test {
 public:
  DefinitionResultParserTest()
      : parser_(std::make_unique<DefinitionResultParser>()) {}

  DefinitionResultParserTest(const DefinitionResultParserTest&) = delete;
  DefinitionResultParserTest& operator=(const DefinitionResultParserTest&) =
      delete;

 protected:
  Value::Dict BuildDictionaryResult(const std::string& query_term,
                                    const std::string& phonetic_str,
                                    const std::string& definition) {
    Value::Dict result;

    if (!query_term.empty())
      result.SetByDottedPath("dictionaryResult.queryTerm", query_term);

    // Build definition entry.
    Value::List entries;
    Value::Dict entry;

    // Build phonetics.
    if (!phonetic_str.empty()) {
      Value::List phonetics;
      Value::Dict phonetic;
      phonetic.Set("text", phonetic_str);
      phonetics.Append(std::move(phonetic));
      entry.Set("phonetics", std::move(phonetics));
    }

    // Build definition.
    if (!definition.empty()) {
      Value::List sense_families;
      Value::Dict sense_family;
      Value::List senses;
      Value::Dict sense;
      sense.SetByDottedPath("definition.text", definition);
      senses.Append(std::move(sense));
      sense_family.Set("senses", std::move(senses));
      sense_families.Append(std::move(sense_family));
      entry.Set("senseFamilies", std::move(sense_families));
    }

    entries.Append(std::move(entry));

    result.SetByDottedPath("dictionaryResult.entries", std::move(entries));

    return result;
  }

  void SetHeadWord(Value::Dict& result, const std::string& headword) {
    (*result.FindListByDottedPath("dictionaryResult.entries"))[0].GetDict().Set(
        "headword", headword);
  }

  std::unique_ptr<DefinitionResultParser> parser_;
};

TEST_F(DefinitionResultParserTest, Success) {
  Value::Dict result =
      BuildDictionaryResult("unfathomable", "ˌənˈfaT͟Həməb(ə)",
                            "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  const auto& expected_answer =
      "incapable of being fully explored or understood.";
  EXPECT_EQ(ResultType::kDefinitionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(expected_answer,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);
  auto* title = static_cast<QuickAnswerText*>(quick_answer.title[0].get());
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
  EXPECT_EQ(ui::kColorLabelForeground, title->color_id);
}

TEST_F(DefinitionResultParserTest, EmptyValue) {
  Value::Dict result;
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoQueryTerm) {
  Value::Dict result =
      BuildDictionaryResult("", "ˌənˈfaT͟Həməb(ə)",
                            "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoQueryTermShouldFallbackToHeadword) {
  Value::Dict result =
      BuildDictionaryResult("", "ˌənˈfaT͟Həməb(ə)",
                            "incapable of being fully explored or understood.");
  SetHeadWord(result, "unfathomable");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
}

TEST_F(DefinitionResultParserTest, ShouldPrioritizeQueryTerm) {
  Value::Dict result =
      BuildDictionaryResult("Unfathomable", "ˌənˈfaT͟Həməb(ə)",
                            "incapable of being fully explored or understood.");
  SetHeadWord(result, "Unfathomable");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "Unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
}

TEST_F(DefinitionResultParserTest, NoPhonetic) {
  Value::Dict result = BuildDictionaryResult(
      "unfathomable", "", "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "unfathomable";
  const auto& expected_answer =
      "incapable of being fully explored or understood.";
  EXPECT_EQ(ResultType::kDefinitionResult, quick_answer.result_type);
  EXPECT_EQ(expected_answer,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
}

TEST_F(DefinitionResultParserTest, NoDefinition) {
  Value::Dict result =
      BuildDictionaryResult("unfathomable", "ˌənˈfaT͟Həməb(ə)l", "");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

}  // namespace quick_answers
