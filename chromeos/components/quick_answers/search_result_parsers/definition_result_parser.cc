// Copyright 2019 The Chromium Authors
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

constexpr char kDictionaryEntriesPath[] = "dictionaryResult.entries";
constexpr char kDefinitionPathUnderSense[] = "definition.text";
constexpr char kHeadwordKey[] = "headword";
constexpr char kLocaleKey[] = "locale";
constexpr char kPhoneticsKey[] = "phonetics";
constexpr char kPhoneticsTextKey[] = "text";
constexpr char kPhoneticsAudioKey[] = "oxfordAudio";
constexpr char kPhoneticsTtsAudioEnabledKey[] = "ttsAudioEnabled";
constexpr char kSenseFamiliesKey[] = "senseFamilies";
constexpr char kSensesKey[] = "senses";
constexpr char kQueryTermPath[] = "dictionaryResult.queryTerm";

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
    LOG(ERROR) << "Fail in extracting definition.";
    return false;
  }
  const std::string* phonetics = ExtractPhoneticsText(first_entry);

  // If query term path not found, fallback to use headword.
  const std::string* query = result->FindStringPath(kQueryTermPath);
  if (!query)
    query = first_entry->FindStringPath(kHeadwordKey);
  if (!query) {
    LOG(ERROR) << "Fail in extracting query.";
    return false;
  }

  const std::string& secondary_answer =
      phonetics ? BuildDefinitionTitleText(query->c_str(), phonetics->c_str())
                : query->c_str();
  quick_answer->result_type = ResultType::kDefinitionResult;
  quick_answer->title.push_back(
      std::make_unique<QuickAnswerText>(secondary_answer));
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(*definition));
  ExtractPhoneticsInfo(&quick_answer->phonetics_info, first_entry);
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

void DefinitionResultParser::ExtractPhoneticsInfo(
    PhoneticsInfo* phonetics_info,
    const base::Value* definition_entry) {
  // Check for the query text used for tts audio.
  if (definition_entry->FindStringPath(kHeadwordKey))
    phonetics_info->query_text =
        *definition_entry->FindStringPath(kHeadwordKey);

  // Check for the locale used for tts audio.
  if (definition_entry->FindStringPath(kLocaleKey))
    phonetics_info->locale = *definition_entry->FindStringPath(kLocaleKey);

  const Value* first_phonetics = ExtractFirstPhonetics(definition_entry);

  if (!first_phonetics)
    return;

  // Check if the phonetics has an audio URL.
  if (first_phonetics->FindStringPath(kPhoneticsAudioKey))
    phonetics_info->phonetics_audio = GURL(
        kHttpsPrefix + *first_phonetics->FindStringPath(kPhoneticsAudioKey));

  // Check if tts audio is enabled for the query.
  if (first_phonetics->FindBoolPath(kPhoneticsTtsAudioEnabledKey))
    phonetics_info->tts_audio_enabled = true;
}

}  // namespace quick_answers
