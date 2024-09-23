// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <map>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/spell_checker.h"
#include "chromeos/components/quick_answers/utils/translation_v2_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {
namespace {

using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::TextAnnotationPtr;
using ::chromeos::machine_learning::mojom::TextAnnotationRequest;
using ::chromeos::machine_learning::mojom::TextAnnotationRequestPtr;
using ::chromeos::machine_learning::mojom::TextClassifier;

// TODO(llin): Finalize on the threshold based on user feedback.
constexpr int kUnitConversionIntentAndSelectionLengthDiffThreshold = 5;
constexpr int kTranslationTextLengthThreshold = 100;
constexpr int kRichAnswersTranslationTextLengthThreshold = 250;
constexpr int kDefinitionIntentAndSelectionLengthDiffThreshold = 2;

// TODO(b/169370175): Remove the temporary invalid set after we ramp up to v2
// model.
// Set of invalid characters for definition annonations.
constexpr char kInvalidCharactersSet[] = "()[]{}<>_&|!";

constexpr char kEnglishLanguage[] = "en";

const std::map<std::string, IntentType>& GetIntentTypeMap() {
  static base::NoDestructor<std::map<std::string, IntentType>> kIntentTypeMap(
      {{"unit", IntentType::kUnit}, {"dictionary", IntentType::kDictionary}});
  return *kIntentTypeMap;
}

bool ExtractEntity(const std::string& selected_text,
                   const std::vector<TextAnnotationPtr>& annotations,
                   std::string* entity_str,
                   std::string* type) {
  for (auto& annotation : annotations) {
    // The offset in annotation result is by chars instead of by bytes. Converts
    // to string16 to support extracting substring from string with UTF-16
    // characters.
    *entity_str = base::UTF16ToUTF8(
        base::UTF8ToUTF16(selected_text)
            .substr(annotation->start_offset,
                    annotation->end_offset - annotation->start_offset));

    // Use the first entity type.
    auto intent_type_map = GetIntentTypeMap();
    for (const auto& entity : annotation->entities) {
      if (intent_type_map.find(entity->name) != intent_type_map.end()) {
        *type = entity->name;
        return true;
      }
    }
  }

  return false;
}

IntentType RewriteIntent(const std::string& selected_text,
                         const std::string& entity_str,
                         const IntentType intent) {
  int intent_and_selection_length_diff =
      base::UTF8ToUTF16(selected_text).length() -
      base::UTF8ToUTF16(entity_str).length();
  if ((intent == IntentType::kUnit &&
       intent_and_selection_length_diff >
           kUnitConversionIntentAndSelectionLengthDiffThreshold) ||
      (intent == IntentType::kDictionary &&
       intent_and_selection_length_diff >
           kDefinitionIntentAndSelectionLengthDiffThreshold)) {
    // Override intent type to |kUnknown| if length diff between intent
    // text and selection text is above the threshold.
    return IntentType::kUnknown;
  }

  return intent;
}

bool IsPreferredLanguage(const std::string& detected_language) {
  auto preferred_languages_list =
      base::SplitString(QuickAnswersState::Get()->preferred_languages(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const std::string& locale : preferred_languages_list) {
    if (l10n_util::GetLanguage(locale) == detected_language)
      return true;
  }
  return false;
}

// TODO(b/169370175): There is an issue with text classifier that
// concatenated words are annotated as definitions. Before we switch to v2
// model, skip such kind of queries for definition annotation for now.
bool ShouldSkipDefinition(const std::string& text) {
  // Skip definition annotations if English is not device language or user
  // preferred language (Currently the text classifier only works with English
  // words).
  auto device_language =
      l10n_util::GetLanguage(QuickAnswersState::Get()->application_locale());
  if (device_language != kEnglishLanguage &&
      !IsPreferredLanguage(kEnglishLanguage))
    return true;

  DCHECK(text.length());
  // Skip the query for definition annotation if the selected text contains
  // capitalized characters in the middle and not all capitalized.
  const auto& text_utf16 = base::UTF8ToUTF16(text);
  bool has_capitalized_middle_characters =
      text_utf16.substr(1) != base::i18n::ToLower(text_utf16.substr(1));
  bool are_all_characters_capitalized =
      text_utf16 == base::i18n::ToUpper(text_utf16);
  if (has_capitalized_middle_characters && !are_all_characters_capitalized)
    return true;
  // Skip the query for definition annotation if the selected text contains
  // invalid characters.
  if (text.find_first_of(kInvalidCharactersSet) != std::string::npos)
    return true;

  return false;
}

// Check that both the source and target languages are supported by the
// translation v2 API.
bool AreTranslationLanguagesSupported(const std::string& source_language,
                                      const std::string& target_language) {
  return TranslationV2Utils::IsSupported(source_language) &&
         TranslationV2Utils::IsSupported(target_language);
}

bool HasDigits(const std::string& word) {
  for (char c : word) {
    if (absl::ascii_isdigit(static_cast<unsigned char>(c))) {
      return true;
    }
  }
  return false;
}

}  // namespace

IntentGenerator::IntentGenerator(base::WeakPtr<SpellChecker> spell_checker,
                                 IntentGeneratorCallback complete_callback)
    : spell_checker_(std::move(spell_checker)),
      complete_callback_(std::move(complete_callback)) {}

IntentGenerator::~IntentGenerator() {
  if (complete_callback_)
    std::move(complete_callback_)
        .Run(IntentInfo(std::string(), IntentType::kUnknown));
}

void IntentGenerator::GenerateIntent(const QuickAnswersRequest& request) {
  const std::u16string& u16_text = base::UTF8ToUTF16(request.selected_text);
  base::i18n::BreakIterator iter(u16_text,
                                 base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init() || !iter.Advance()) {
    NOTREACHED_IN_MIGRATION() << "Failed to load BreakIterator.";

    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  DCHECK(spell_checker_.get()) << "spell_checker_ should exist when the "
                                  "always trigger feature is enabled";
  // Check spelling if the selected text is a valid single word.
  if (iter.IsWord() && iter.prev() == 0 && iter.pos() == u16_text.length()) {
    // Search server do not provide useful information for proper nouns and
    // abbreviations (such as "Amy" and "ASAP"). Check spelling of the word in
    // lower case to filter out such cases.
    auto text = base::UTF16ToUTF8(
        base::i18n::ToLower(base::UTF8ToUTF16(request.selected_text)));
    spell_checker_->CheckSpelling(
        text, base::BindOnce(&IntentGenerator::CheckSpellingCallback,
                             weak_factory_.GetWeakPtr(), request));
    return;
  }

  // Fallback to text classifier.
  MaybeLoadTextClassifier(request);
}

void IntentGenerator::FlushForTesting() {
  text_classifier_.FlushForTesting();
}

void IntentGenerator::MaybeLoadTextClassifier(
    const QuickAnswersRequest& request) {
  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    // Load text classifier.
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadTextClassifier(
            text_classifier_.BindNewPipeAndPassReceiver(),
            base::BindOnce(&IntentGenerator::LoadModelCallback,
                           weak_factory_.GetWeakPtr(), request));
    return;
  }

  std::move(complete_callback_)
      .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
}

void IntentGenerator::CheckSpellingCallback(const QuickAnswersRequest& request,
                                            bool correctness,
                                            const std::string& language) {
  // Generate dictionary intent if the selected word passed spell check.
  // The dictionaries treat digits as valid words, while we will not be able to
  // grab any useful information from the Search server for words like that.
  // Thus we filter out the words containing digits. We still fallback to the
  // text classifier for unit conversion intent.
  if (correctness && !HasDigits(request.selected_text)) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kDictionary,
                        QuickAnswersState::Get()->application_locale(),
                        language));

    // Record intent source type and language for dictionary intent.
    RecordDictionaryIntentSource(DictionaryIntentSource::kHunspell);
    RecordDictionaryIntentLanguage(language);
    return;
  }

  // If the selected word did not pass spell check, fallback to the text
  // classifier. We may generate other intent type as well as definition intent
  // if the word is not covered in the dictionary but in the model.
  MaybeLoadTextClassifier(request);
}

void IntentGenerator::LoadModelCallback(const QuickAnswersRequest& request,
                                        LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    LOG(ERROR) << "Failed to load TextClassifier.";
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  if (text_classifier_) {
    TextAnnotationRequestPtr text_annotation_request =
        TextAnnotationRequest::New();

    text_annotation_request->text = request.selected_text;
    text_annotation_request->default_locales =
        QuickAnswersState::Get()->application_locale();
    text_annotation_request->trigger_dictionary_on_beginner_words = true;

    text_classifier_->Annotate(
        std::move(text_annotation_request),
        base::BindOnce(&IntentGenerator::AnnotationCallback,
                       weak_factory_.GetWeakPtr(), request));
  }
}

void IntentGenerator::AnnotationCallback(
    const QuickAnswersRequest& request,
    std::vector<TextAnnotationPtr> annotations) {
  std::string entity_str;
  std::string type;

  if (ExtractEntity(request.selected_text, annotations, &entity_str, &type)) {
    auto intent_type_map = GetIntentTypeMap();
    auto it = intent_type_map.find(type);
    if (it != intent_type_map.end()) {
      // Skip the entity if the corresponding intent type is ineligible.
      bool definition_ineligible =
          !QuickAnswersState::IsIntentEligible(Intent::kDefinition);
      bool unit_conversion_ineligible =
          !QuickAnswersState::IsIntentEligible(Intent::kUnitConversion);
      if ((it->second == IntentType::kDictionary && definition_ineligible) ||
          (it->second == IntentType::kUnit && unit_conversion_ineligible)) {
        // Fallback to language detection for generating translation intent.
        MaybeGenerateTranslationIntent(request);
        return;
      }
      // Skip the entity for definition annonation.
      if (it->second == IntentType::kDictionary &&
          ShouldSkipDefinition(request.selected_text)) {
        // Fallback to language detection for generating translation intent.
        MaybeGenerateTranslationIntent(request);
        return;
      }
      std::move(complete_callback_)
          .Run(IntentInfo(
              entity_str,
              RewriteIntent(request.selected_text, entity_str, it->second),
              QuickAnswersState::Get()->application_locale()));

      // Record intent source type and language for dictionary intent.
      if (it->second == IntentType::kDictionary) {
        RecordDictionaryIntentSource(DictionaryIntentSource::kTextClassifier);
        // Record the English language since currently the text classifier only
        // works with English words.
        RecordDictionaryIntentLanguage(kEnglishLanguage);
      }
      return;
    }
  }
  // Fallback to language detection for generating translation intent.
  MaybeGenerateTranslationIntent(request);
}

void IntentGenerator::MaybeGenerateTranslationIntent(
    const QuickAnswersRequest& request) {
  DCHECK(complete_callback_);

  if (!QuickAnswersState::IsIntentEligible(Intent::kTranslation) ||
      chromeos::features::IsQuickAnswersV2TranslationDisabled()) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  size_t translation_text_length_threshold =
      chromeos::features::IsQuickAnswersRichCardEnabled()
          ? kRichAnswersTranslationTextLengthThreshold
          : kTranslationTextLengthThreshold;
  // Don't generate translation intent if no device language is provided or the
  // length of selected text is above the threshold. Returns unknown intent
  // type.
  if (QuickAnswersState::Get()->application_locale().empty() ||
      request.selected_text.length() > translation_text_length_threshold) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  language_detector_ =
      std::make_unique<LanguageDetector>(text_classifier_.get());
  language_detector_->DetectLanguage(
      request.context.surrounding_text, request.selected_text,
      base::BindOnce(&IntentGenerator::LanguageDetectorCallback,
                     weak_factory_.GetWeakPtr(), request));
}

void IntentGenerator::LanguageDetectorCallback(
    const QuickAnswersRequest& request,
    std::optional<std::string> detected_locale) {
  language_detector_.reset();

  auto device_language =
      l10n_util::GetLanguage(QuickAnswersState::Get()->application_locale());
  auto detected_language = detected_locale.has_value()
                               ? l10n_util::GetLanguage(detected_locale.value())
                               : std::string();

  // Generate translation intent if the detected language is different to the
  // system language and is not one of the preferred languages.
  // Skip translation if the source or target languages are not supported.
  if (!detected_language.empty() && detected_language != device_language &&
      !IsPreferredLanguage(detected_language) &&
      AreTranslationLanguagesSupported(detected_language, device_language)) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kTranslation,
                        device_language, detected_language));
    return;
  }

  std::move(complete_callback_)
      .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
}

}  // namespace quick_answers
