// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr char kHttpsPrefix[] = "https:";

// DictionaryResult
constexpr char kDictionaryResultKey[] = "dictionaryResult";
constexpr char kQueryTermKey[] = "queryTerm";
constexpr char kEntriesKey[] = "entries";

// Entry
constexpr char kHeadwordKey[] = "headword";
constexpr char kLocaleKey[] = "locale";
constexpr char kPhoneticsKey[] = "phonetics";
constexpr char kSenseFamiliesKey[] = "senseFamilies";

// Phonetics
constexpr char kPhoneticsTextKey[] = "text";
constexpr char kPhoneticsAudioKey[] = "oxfordAudio";
constexpr char kPhoneticsTtsAudioEnabledKey[] = "ttsAudioEnabled";

// SenseFamilies
constexpr char kSensesKey[] = "senses";

// Sense
constexpr char kDefinitionKey[] = "definition";
constexpr char kDefinitionTextKey[] = "text";

std::unique_ptr<Sense> ParseSense(const base::Value::Dict& sense_result) {
  const base::Value::Dict* definition_entry =
      sense_result.FindDict(kDefinitionKey);
  if (!definition_entry) {
    DLOG(ERROR) << "Unable to find definition entry.";
    return nullptr;
  }

  const std::string* definition_text =
      definition_entry->FindString(kDefinitionTextKey);
  if (!definition_text) {
    DLOG(ERROR) << "Unable to find a text in a definition entry.";
    return nullptr;
  }

  std::unique_ptr<Sense> sense = std::make_unique<Sense>();
  sense->definition = *definition_text;
  return sense;
}

const std::string* GetQueryTerm(const base::Value::Dict& result) {
  return result.FindString(kQueryTermKey);
}

const std::string* GetHeadword(const base::Value::Dict& entry_result) {
  return entry_result.FindString(kHeadwordKey);
}

}  // namespace

std::unique_ptr<StructuredResult>
DefinitionResultParser::ParseInStructuredResult(
    const base::Value::Dict& result) {
  const Value::Dict* dictionary_result = result.FindDict(kDictionaryResultKey);
  if (!dictionary_result) {
    DLOG(ERROR) << "Unable to find the dictionary result entry.";
    return nullptr;
  }

  const Value::Dict* first_entry =
      GetFirstListElement(*dictionary_result, kEntriesKey);
  if (!first_entry) {
    DLOG(ERROR) << "Unable to find a first entry.";
    return nullptr;
  }

  const Value::Dict* first_sense_family = ExtractFirstSenseFamily(*first_entry);
  if (!first_sense_family) {
    DLOG(ERROR) << "Unable to find a first sense familiy.";
    return nullptr;
  }

  const Value::Dict* first_sense =
      GetFirstListElement(*first_sense_family, kSensesKey);
  if (!first_sense) {
    DLOG(ERROR) << "Unable to find a first sense.";
    return nullptr;
  }

  std::unique_ptr<Sense> sense = ParseSense(*first_sense);
  if (!sense) {
    DLOG(ERROR) << "Unable to parse a sense.";
    return nullptr;
  }
  std::unique_ptr<DefinitionResult> definition_result =
      std::make_unique<DefinitionResult>();
  definition_result->sense = *(sense.get());

  const std::string* word = GetQueryTerm(*dictionary_result);
  if (!word) {
    word = GetHeadword(*first_entry);
  }
  if (!word) {
    DLOG(ERROR) << "Unable to find a word in either query term or headword.";
    return nullptr;
  }
  definition_result->word = *word;

  std::unique_ptr<PhoneticsInfo> phonetics_info =
      ParsePhoneticsInfo(*first_entry);
  if (phonetics_info) {
    definition_result->phonetics_info = *(phonetics_info.get());
  }

  std::unique_ptr<StructuredResult> structured_result =
      std::make_unique<StructuredResult>();
  structured_result->definition_result = std::move(definition_result);
  return structured_result;
}

bool DefinitionResultParser::PopulateQuickAnswer(
    const StructuredResult& structured_result,
    QuickAnswer* quick_answer) {
  DefinitionResult* definition_result =
      structured_result.definition_result.get();
  if (!definition_result) {
    DLOG(ERROR) << "Unable to find definition_result.";
    return false;
  }

  quick_answer->result_type = ResultType::kDefinitionResult;
  quick_answer->phonetics_info =
      structured_result.definition_result->phonetics_info;

  // Title line
  if (definition_result->word.empty()) {
    DLOG(ERROR) << "Unable to find a word in definition_result.";
    return false;
  }
  const std::string& title =
      !quick_answer->phonetics_info.text.empty()
          ? BuildDefinitionTitleText(definition_result->word,
                                     quick_answer->phonetics_info.text)
          : definition_result->word;
  quick_answer->title.push_back(std::make_unique<QuickAnswerText>(title));

  // Second line, i.e. definition.
  if (definition_result->sense.definition.empty()) {
    DLOG(ERROR) << "Unable to find a definition in a sense.";
    return false;
  }

  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(
          definition_result->sense.definition));
  return true;
}

bool DefinitionResultParser::SupportsNewInterface() const {
  return true;
}

bool DefinitionResultParser::Parse(const base::Value::Dict& result,
                                   QuickAnswer* quick_answer) {
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult(result);
  if (!structured_result) {
    return false;
  }

  return PopulateQuickAnswer(*structured_result, quick_answer);
}

const Value::Dict* DefinitionResultParser::ExtractFirstSenseFamily(
    const base::Value::Dict& definition_entry) {
  const Value::Dict* first_sense_family =
      GetFirstListElement(definition_entry, kSenseFamiliesKey);
  if (!first_sense_family) {
    DLOG(ERROR) << "Can't find a sense family.";
    return nullptr;
  }

  return first_sense_family;
}

const Value::Dict* DefinitionResultParser::ExtractFirstPhonetics(
    const base::Value::Dict& definition_entry) {
  const Value::Dict* first_phonetics =
      GetFirstListElement(definition_entry, kPhoneticsKey);
  if (first_phonetics)
    return first_phonetics;

  // It is is possible to have phonetics per sense family in case of heteronyms
  // such as "arithmetic".
  const Value::Dict* sense_family = ExtractFirstSenseFamily(definition_entry);
  if (sense_family)
    return GetFirstListElement(*sense_family, kPhoneticsKey);

  DLOG(ERROR) << "Can't find a phonetics.";
  return nullptr;
}

std::unique_ptr<PhoneticsInfo> DefinitionResultParser::ParsePhoneticsInfo(
    const base::Value::Dict& entry_result) {
  const Value::Dict* first_phonetics = ExtractFirstPhonetics(entry_result);
  if (!first_phonetics) {
    DLOG(ERROR) << "Unable to find a first phonetics.";
    return nullptr;
  }

  std::unique_ptr<PhoneticsInfo> phonetics_info =
      std::make_unique<PhoneticsInfo>();

  const std::string* text = first_phonetics->FindString(kPhoneticsTextKey);
  if (text) {
    phonetics_info->text = *text;
  }

  // Check for the query text used for tts audio.
  const std::string* headword = GetHeadword(entry_result);
  if (headword) {
    phonetics_info->query_text = *headword;
  }

  // Check for the locale used for tts audio.
  const std::string* locale = entry_result.FindString(kLocaleKey);
  if (locale) {
    phonetics_info->locale = *locale;
  }

  // Check if the phonetics has an audio URL.
  const std::string* audio_url =
      first_phonetics->FindString(kPhoneticsAudioKey);
  if (audio_url) {
    phonetics_info->phonetics_audio = GURL(kHttpsPrefix + *audio_url);
  }

  // Check if tts audio is enabled for the query.
  absl::optional<bool> tts_audio_enabled =
      first_phonetics->FindBool(kPhoneticsTtsAudioEnabledKey);
  if (tts_audio_enabled) {
    phonetics_info->tts_audio_enabled = tts_audio_enabled.value();
  }
  return phonetics_info;
}

}  // namespace quick_answers
