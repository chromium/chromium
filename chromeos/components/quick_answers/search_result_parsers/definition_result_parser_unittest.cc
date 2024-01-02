// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

namespace quick_answers {
namespace {
constexpr char kPhoneticsAudioUrlWithoutProtocol[] = "//example.com/audio";
constexpr char kPhoneticsAudioUrlWithProtocol[] = "https://example.com/audio";

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
  Value::Dict BuildDictionaryResult(
      const std::string& query_term,
      const std::string& phonetic_str,
      const std::string& definition,
      const std::string& word_class,
      const std::string& sample_sentence = std::string(),
      const std::vector<std::string>& synonyms_list = {},
      const std::vector<std::string>& subsenses_list = {}) {
    Value::Dict result;

    if (!query_term.empty()) {
      result.SetByDottedPath("dictionaryResult.queryTerm", query_term);
    }

    // Build definition entry.
    Value::List entries;
    Value::Dict entry;

    entry.Set("locale", "en");

    // Build phonetics.
    if (!phonetic_str.empty()) {
      Value::List phonetics;
      Value::Dict phonetic;
      phonetic.Set("text", phonetic_str);
      phonetic.Set("oxfordAudio", kPhoneticsAudioUrlWithoutProtocol);
      phonetics.Append(std::move(phonetic));
      entry.Set("phonetics", std::move(phonetics));
    }

    // Build senses.
    if (!definition.empty()) {
      Value::List sense_families;
      Value::Dict sense_family;
      Value::List senses;
      Value::Dict sense;

      sense.SetByDottedPath("definition.text", definition);
      if (!sample_sentence.empty()) {
        Value::List example_groups;
        Value::Dict example_group;
        Value::List examples;

        examples.Append(sample_sentence);
        example_group.Set("examples", std::move(examples));
        example_groups.Append(std::move(example_group));
        sense.Set("exampleGroups", std::move(example_groups));
      }
      if (!synonyms_list.empty()) {
        Value::List thesaurus_entries;
        Value::Dict thesaurus_entry;
        Value::List synonyms;
        Value::Dict synonym;
        Value::List nyms;

        for (std::string synonym_term : synonyms_list) {
          Value::Dict nym;
          nym.Set("nym", synonym_term);
          nyms.Append(std::move(nym));
        }
        synonym.Set("nyms", std::move(nyms));
        synonyms.Append(std::move(synonym));
        thesaurus_entry.Set("synonyms", std::move(synonyms));
        thesaurus_entries.Append(std::move(thesaurus_entry));
        sense.Set("thesaurusEntries", std::move(thesaurus_entries));
      }
      if (!subsenses_list.empty()) {
        Value::List subsenses;

        for (std::string subsense_term : subsenses_list) {
          Value::Dict subsense;
          subsense.SetByDottedPath("definition.text", subsense_term);
          subsenses.Append(std::move(subsense));
        }
        sense.Set("subsenses", std::move(subsenses));
      }
      senses.Append(std::move(sense));
      sense_family.Set("senses", std::move(senses));

      if (!word_class.empty()) {
        Value::List parts_of_speech;
        Value::Dict part_of_speech;
        part_of_speech.Set("value", word_class);
        parts_of_speech.Append(std::move(part_of_speech));
        sense_family.Set("partsOfSpeech", std::move(parts_of_speech));
      }
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
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"unfathomable", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"adjective");
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

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      parser_->ParseInStructuredResult(result);
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->definition_result);

  DefinitionResult* definition_result =
      structured_result->definition_result.get();
  EXPECT_EQ(definition_result->word, "unfathomable");
  EXPECT_EQ(definition_result->word_class, "adjective");

  // `PhoneticsInfo::query_text` is headword. It's a query text for TTS. We
  // should not expect this test case response from the server as either
  // `query_text` or `phonetics_audio` should be filled.
  EXPECT_TRUE(definition_result->phonetics_info.query_text.empty());
  EXPECT_EQ(definition_result->phonetics_info.text, "ˌənˈfaT͟Həməb(ə)");
  EXPECT_EQ(definition_result->phonetics_info.locale, "en");
  EXPECT_EQ(definition_result->phonetics_info.phonetics_audio,
            GURL(kPhoneticsAudioUrlWithProtocol));
  EXPECT_FALSE(definition_result->phonetics_info.tts_audio_enabled);

  EXPECT_EQ(definition_result->sense.definition, expected_answer);
}

TEST_F(DefinitionResultParserTest, SuccessWithRichCardInfo) {
  const std::vector<std::string> synonyms_list = {"fine", "nice", "minute",
                                                  "precise"};
  const std::vector<std::string> subsenses_list = {
      "(of a mixture or effect) delicately complex and understated.",
      "making use of clever and indirect methods to achieve something.",
      "capable of making fine distinctions.",
      "arranged in an ingenious and elaborate way."};
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"subtle", /*phonetic_str=*/"ˈsəd(ə)l",
      /*definition=*/
      "(especially of a change or distinction) so delicate or precise as to be "
      "difficult to analyze or describe.",
      /*word_class=*/"adjective",
      /*sample_sentence=*/"his language expresses rich and subtle meanings",
      synonyms_list, subsenses_list);
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  EXPECT_EQ(ResultType::kDefinitionResult, quick_answer.result_type);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      parser_->ParseInStructuredResult(result);
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->definition_result);

  DefinitionResult* definition_result =
      structured_result->definition_result.get();
  EXPECT_EQ(definition_result->word, "subtle");
  EXPECT_EQ(definition_result->word_class, "adjective");

  // `PhoneticsInfo::query_text` is headword. It's a query text for TTS. We
  // should not expect this test case response from the server as either
  // `query_text` or `phonetics_audio` should be filled.
  EXPECT_TRUE(definition_result->phonetics_info.query_text.empty());
  EXPECT_EQ(definition_result->phonetics_info.text, "ˈsəd(ə)l");
  EXPECT_EQ(definition_result->phonetics_info.locale, "en");
  EXPECT_EQ(definition_result->phonetics_info.phonetics_audio,
            GURL(kPhoneticsAudioUrlWithProtocol));
  EXPECT_FALSE(definition_result->phonetics_info.tts_audio_enabled);

  EXPECT_EQ(
      definition_result->sense.definition,
      "(especially of a change or distinction) so delicate or precise as to be "
      "difficult to analyze or describe.");

  // Rich card specific `StructuredResult` fields.
  EXPECT_TRUE(definition_result->sense.sample_sentence.has_value());
  EXPECT_EQ(definition_result->sense.sample_sentence,
            "his language expresses rich and subtle meanings");

  EXPECT_TRUE(definition_result->sense.synonyms_list.has_value());
  const std::vector<std::string> expected_synonyms_list = {"fine", "nice",
                                                           "minute"};
  EXPECT_EQ(definition_result->sense.synonyms_list, expected_synonyms_list);

  EXPECT_TRUE(definition_result->subsenses_list.has_value());
  const std::vector<std::string> expected_subsenses_list = {
      "(of a mixture or effect) delicately complex and understated.",
      "making use of clever and indirect methods to achieve something.",
      "capable of making fine distinctions."};
  std::vector<std::string> actual_subsenses_list = {};
  for (quick_answers::Sense subsense :
       definition_result->subsenses_list.value()) {
    actual_subsenses_list.push_back(subsense.definition);
  }
  EXPECT_EQ(actual_subsenses_list, expected_subsenses_list);
}

TEST_F(DefinitionResultParserTest, EmptyValue) {
  Value::Dict result;
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoQueryTerm) {
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"adjective");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoQueryTermShouldFallbackToHeadword) {
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"adjective");
  SetHeadWord(result, "unfathomable");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
}

TEST_F(DefinitionResultParserTest, ShouldPrioritizeQueryTerm) {
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"Unfathomable", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"adjective");
  SetHeadWord(result, "Unfathomable");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  const auto& expected_title = "Unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  EXPECT_EQ(expected_title, GetQuickAnswerTextForTesting(quick_answer.title));
}

TEST_F(DefinitionResultParserTest, NoPhonetic) {
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"unfathomable", /*phonetic_str=*/"",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"adjective");
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
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"unfathomable", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)l",
      /*definition=*/"", /*word_class=*/"adjective");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoWordClass) {
  Value::Dict result = BuildDictionaryResult(
      /*query_term=*/"unfathomable", /*phonetic_str=*/"ˌənˈfaT͟Həməb(ə)l",
      /*definition=*/"incapable of being fully explored or understood.",
      /*word_class=*/"");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

}  // namespace quick_answers
