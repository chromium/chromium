// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <optional>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
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
constexpr char kPartsOfSpeechKey[] = "partsOfSpeech";

// Sense
constexpr char kDefinitionKey[] = "definition";
constexpr char kDefinitionTextKey[] = "text";
constexpr char kExampleGroupsKey[] = "exampleGroups";
constexpr char kExamplesKey[] = "examples";
constexpr char kAdditionalExamplesKey[] = "additionalExamples";
constexpr char kThesaurusEntriesKey[] = "thesaurusEntries";
constexpr char kSynonymsKey[] = "synonyms";
constexpr char kNymsKey[] = "nyms";
constexpr char kSynonymTextKey[] = "nym";
constexpr int kMaxSynonymsNumber = 3;

// SubSenses
constexpr char kSubSensesKey[] = "subsenses";
constexpr int kMaxSubSensesNumber = 3;

// PartsOfSpeech
constexpr char kPartsOfSpeechTextKey[] = "value";

std::string GetQueryTerm(const base::Value::Dict& result) {
  const std::string* query_term = result.FindString(kQueryTermKey);
  if (!query_term) {
    return std::string();
  }
  return *query_term;
}

std::string GetHeadword(const base::Value::Dict& entry_result) {
  const std::string* headword = entry_result.FindString(kHeadwordKey);
  if (!headword) {
    return std::string();
  }
  return *headword;
}

std::string GetSampleSentence(const base::Value::Dict& sense_result) {
  std::string sample_sentence;

  // Check both the `exampleGroups` and `additionalExamples` keys for a
  // sample sentence text.
  const base::Value::Dict* example_groups =
      ResultParser::GetFirstDictElementFromList(sense_result,
                                                kExampleGroupsKey);
  if (example_groups) {
    const Value::List* examples = example_groups->FindList(kExamplesKey);
    if (examples && !examples->empty()) {
      sample_sentence = examples->front().GetString();
    }
  }
  if (sample_sentence.empty()) {
    const Value::List* additional_examples =
        sense_result.FindList(kAdditionalExamplesKey);
    if (additional_examples && !additional_examples->empty()) {
      sample_sentence = additional_examples->front().GetString();
    }
  }

  return sample_sentence;
}

std::vector<std::string> GetSynonymsList(
    const base::Value::Dict& sense_result) {
  std::vector<std::string> synonyms_list;

  const base::Value::Dict* thesaurus_entries =
      ResultParser::GetFirstDictElementFromList(sense_result,
                                                kThesaurusEntriesKey);
  if (!thesaurus_entries) {
    return synonyms_list;
  }

  const base::Value::Dict* synonym_entries =
      ResultParser::GetFirstDictElementFromList(*thesaurus_entries,
                                                kSynonymsKey);
  if (!synonym_entries) {
    return synonyms_list;
  }

  const Value::List* nyms_entries = synonym_entries->FindList(kNymsKey);
  if (!nyms_entries) {
    return synonyms_list;
  }

  for (const base::Value& value : *nyms_entries) {
    // Stop after fetching the max number of synonyms.
    if (synonyms_list.size() == kMaxSynonymsNumber) {
      break;
    }

    const base::Value::Dict* nyms_entry = &(value.GetDict());
    if (nyms_entry->empty()) {
      continue;
    }

    const std::string* synonym_text = nyms_entry->FindString(kSynonymTextKey);
    if (synonym_text) {
      synonyms_list.push_back(*synonym_text);
    }
  }

  return synonyms_list;
}

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
  sense->definition = ResultParser::RemoveKnownHtmlTags(*definition_text);

  const std::string sample_sentence = GetSampleSentence(sense_result);
  if (!sample_sentence.empty()) {
    sense->sample_sentence = ResultParser::RemoveKnownHtmlTags(sample_sentence);
  }

  const std::vector<std::string> synonyms_list = GetSynonymsList(sense_result);
  if (!synonyms_list.empty()) {
    sense->synonyms_list = std::move(synonyms_list);
  }

  return sense;
}

std::vector<Sense> ParseSubSenses(const base::Value::Dict& sense_result) {
  std::vector<Sense> subsenses_list;

  const Value::List* subsense_entries = sense_result.FindList(kSubSensesKey);
  if (!subsense_entries) {
    return subsenses_list;
  }

  for (const base::Value& value : *subsense_entries) {
    // Stop after fetching the max number of subsenses.
    if (subsenses_list.size() == kMaxSubSensesNumber) {
      break;
    }

    const base::Value::Dict* subsense_entry = &(value.GetDict());
    if (subsense_entry->empty()) {
      continue;
    }

    std::unique_ptr<Sense> subsense = ParseSense(*subsense_entry);
    if (!subsense) {
      continue;
    }

    subsenses_list.push_back(*subsense);
  }

  return subsenses_list;
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

  const Value::Dict* first_entry = ResultParser::GetFirstDictElementFromList(
      *dictionary_result, kEntriesKey);
  if (!first_entry) {
    DLOG(ERROR) << "Unable to find a first entry.";
    return nullptr;
  }

  const Value::Dict* first_sense_family = ExtractFirstSenseFamily(*first_entry);
  if (!first_sense_family) {
    DLOG(ERROR) << "Unable to find a first sense familiy.";
    return nullptr;
  }

  const Value::Dict* first_sense = ResultParser::GetFirstDictElementFromList(
      *first_sense_family, kSensesKey);
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

  const std::vector<Sense> subsenses_list = ParseSubSenses(*first_sense);
  if (!subsenses_list.empty()) {
    definition_result->subsenses_list = std::move(subsenses_list);
  }

  const Value::Dict* part_of_speech = ResultParser::GetFirstDictElementFromList(
      *first_sense_family, kPartsOfSpeechKey);
  if (!part_of_speech) {
    // For Spanish dictionary results, the |partsOfSpeech| field is found in
    // the individual sense information rather than in the sense family.
    // Try to find the |partsOfSpeech| for the |first_sense| since that is the
    // definition information we use for Quick Answers.
    part_of_speech = ResultParser::GetFirstDictElementFromList(
        *first_sense, kPartsOfSpeechKey);
    if (!part_of_speech) {
      DLOG(ERROR) << "Unable to find a part of speech.";
      return nullptr;
    }
  }

  const std::string* word_class =
      part_of_speech->FindString(kPartsOfSpeechTextKey);
  if (!word_class) {
    DLOG(ERROR) << "Unable to find a text in a part of speech entry.";
    return nullptr;
  }
  definition_result->word_class = *word_class;

  std::string word = GetQueryTerm(*dictionary_result);
  if (word.empty()) {
    word = GetHeadword(*first_entry);
    if (word.empty()) {
      DLOG(ERROR) << "Unable to find a word in either query term or headword.";
      return nullptr;
    }
  }
  definition_result->word = word;

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
      ResultParser::GetFirstDictElementFromList(definition_entry,
                                                kSenseFamiliesKey);
  if (!first_sense_family) {
    DLOG(ERROR) << "Can't find a sense family.";
    return nullptr;
  }

  return first_sense_family;
}

const Value::Dict* DefinitionResultParser::ExtractFirstPhonetics(
    const base::Value::Dict& definition_entry) {
  const Value::Dict* first_phonetics =
      ResultParser::GetFirstDictElementFromList(definition_entry,
                                                kPhoneticsKey);
  if (first_phonetics)
    return first_phonetics;

  // It is is possible to have phonetics per sense family in case of heteronyms
  // such as "arithmetic".
  const Value::Dict* sense_family = ExtractFirstSenseFamily(definition_entry);
  if (sense_family)
    return ResultParser::GetFirstDictElementFromList(*sense_family,
                                                     kPhoneticsKey);

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
  const std::string headword = GetHeadword(entry_result);
  if (!headword.empty()) {
    phonetics_info->query_text = headword;
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
  std::optional<bool> tts_audio_enabled =
      first_phonetics->FindBool(kPhoneticsTtsAudioEnabledKey);
  if (tts_audio_enabled) {
    phonetics_info->tts_audio_enabled = tts_audio_enabled.value();
  }
  return phonetics_info;
}

}  // namespace quick_answers
