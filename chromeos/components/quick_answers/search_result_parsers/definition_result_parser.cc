// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr char kHttpsPrefix[] = "https:";

constexpr char kQueryTermPath[] = "dictionaryResult.queryTerm";
constexpr char kDictionaryEntriesPath[] = "dictionaryResult.entries";
constexpr char kSenseFamiliesKey[] = "senseFamilies";
constexpr char kSensesKey[] = "senses";
constexpr char kDefinitionPathUnderSense[] = "definition.text";
constexpr char kPhoneticsKey[] = "phonetics";
constexpr char kPhoneticsTextKey[] = "text";
constexpr char kPhoneticsAudioKey[] = "oxfordAudio";

}  // namespace

bool DefinitionResultParser::Parse(const Value* result,
                                   QuickAnswer* quick_answer) {
  const Value* first_entry =
      GetFirstListElement(*result, kDictionaryEntriesPath);
  if (!first_entry) {
    LOG(ERROR) << "Can't find a definition entry.";
    return false;
  }

  // Get definition and phonetics.
  const std::string* definition = ExtractDefinition(first_entry);
  if (!definition) {
    LOG(ERROR) << "Fail in extracting definition";
    return false;
  }
  const std::string* phonetics = ExtractPhoneticsText(first_entry);

  const std::string* query_term = result->FindStringPath(kQueryTermPath);
  if (!query_term) {
    LOG(ERROR) << "Fail in extracting query term";
    return false;
  }

  const std::string& secondary_answer =
      phonetics
          ? BuildDefinitionTitleText(query_term->c_str(), phonetics->c_str())
          : query_term->c_str();
  quick_answer->result_type = ResultType::kDefinitionResult;
  quick_answer->title.push_back(
      std::make_unique<QuickAnswerText>(secondary_answer));
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(*definition));
  quick_answer->phonetics_audio = ExtractPhoneticsAudio(first_entry);
  return true;
}

const Value* DefinitionResultParser::ExtractFirstSenseFamily(
    const base::Value* definition_entry) {
  const Value* first_sense_family =
      GetFirstListElement(*definition_entry, kSenseFamiliesKey);
  if (!first_sense_family) {
    LOG(ERROR) << "Can't find a sense family.";
    return nullptr;
  }

  return first_sense_family;
}

const Value* DefinitionResultParser::ExtractFirstPhonetics(
    const base::Value* definition_entry) {
  const Value* first_phonetics =
      GetFirstListElement(*definition_entry, kPhoneticsKey);
  if (first_phonetics)
    return first_phonetics;

  // It is is possible to have phonetics per sense family in case of heteronyms
  // such as "arithmetic".
  const Value* sense_family = ExtractFirstSenseFamily(definition_entry);
  if (sense_family)
    return GetFirstListElement(*sense_family, kPhoneticsKey);

  LOG(ERROR) << "Can't find a phonetics.";
  return nullptr;
}

const std::string* DefinitionResultParser::ExtractDefinition(
    const base::Value* definition_entry) {
  const Value* first_sense_family = ExtractFirstSenseFamily(definition_entry);
  if (!first_sense_family)
    return nullptr;

  const Value* first_sense =
      GetFirstListElement(*first_sense_family, kSensesKey);
  if (!first_sense) {
    LOG(ERROR) << "Can't find a sense.";
    return nullptr;
  }

  return first_sense->FindStringPath(kDefinitionPathUnderSense);
}

const std::string* DefinitionResultParser::ExtractPhoneticsText(
    const base::Value* definition_entry) {
  const Value* first_phonetics = ExtractFirstPhonetics(definition_entry);
  if (!first_phonetics)
    return nullptr;

  return first_phonetics->FindStringPath(kPhoneticsTextKey);
}

GURL DefinitionResultParser::ExtractPhoneticsAudio(
    const base::Value* definition_entry) {
  const Value* first_phonetics = ExtractFirstPhonetics(definition_entry);
  // Sometimes the phonetics has no audio URL.
  if (!first_phonetics ||
      !first_phonetics->FindStringPath(kPhoneticsAudioKey)) {
    return GURL();
  }

  return GURL(kHttpsPrefix +
              *first_phonetics->FindStringPath(kPhoneticsAudioKey));
}

}  // namespace quick_answers
