// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <map>

#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"

namespace chromeos {
namespace quick_answers {
namespace {

using chromeos::machine_learning::mojom::LoadModelResult;
using machine_learning::mojom::TextAnnotationPtr;
using machine_learning::mojom::TextAnnotationRequestPtr;
using machine_learning::mojom::TextClassifier;

// TODO(llin): Finalize on the threshold based on user feedback.
constexpr int kUnitConversionIntentAndSelectionLengthDiffThreshold = 5;
constexpr int kTranslationTextLengthThreshold = 50;
constexpr int kDefinitionIntentAndSelectionLengthDiffThreshold = 2;

// TODO(b/169370175): Remove the temporary invalid set after we ramp up to v2
// model.
// Set of invalid characters for definition annonations.
constexpr char kInvalidCharactersSet[] = "()[]{}<>_&|!";

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

// TODO(b/169370175): There is an issue with text classifier that
// concatenated words are annotated as definitions. Before we switch to v2
// model, skip such kind of queries for definition annotation for now.
bool ShouldSkipDefinition(const std::string& text) {
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

}  // namespace

IntentGenerator::IntentGenerator(IntentGeneratorCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
}

IntentGenerator::~IntentGenerator() {
  if (complete_callback_)
    std::move(complete_callback_)
        .Run(IntentInfo(std::string(), IntentType::kUnknown));
}

void IntentGenerator::GenerateIntent(const QuickAnswersRequest& request) {
  if (!features::IsQuickAnswersTextAnnotatorEnabled()) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  // Load text classifier.
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->LoadTextClassifier(text_classifier_.BindNewPipeAndPassReceiver(),
                           base::BindOnce(&IntentGenerator::LoadModelCallback,
                                          weak_factory_.GetWeakPtr(), request));
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
        machine_learning::mojom::TextAnnotationRequest::New();

    // TODO(b/159664194): There is a issue with text classifier that some
    // capitalized words are not annotated properly. Convert the text to lower
    // case for now. Clean up after the issue is fixed.
    text_annotation_request->text = base::UTF16ToUTF8(
        base::i18n::ToLower(base::UTF8ToUTF16(request.selected_text)));
    text_annotation_request->default_locales =
        request.context.device_properties.language;

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
      // Skip the entity for definition annonation.
      if (it->second == IntentType::kDictionary &&
          ShouldSkipDefinition(request.selected_text)) {
        // Fallback to language detection for generating translation intent.
        MaybeGenerateTranslationIntent(request);
        return;
      }
      std::move(complete_callback_)
          .Run(IntentInfo(entity_str, RewriteIntent(request.selected_text,
                                                    entity_str, it->second)));
      return;
    }
  }
  // Fallback to language detection for generating translation intent.
  MaybeGenerateTranslationIntent(request);
}

void IntentGenerator::FindLanguagesCallback(
    const QuickAnswersRequest& request,
    std::vector<machine_learning::mojom::TextLanguagePtr> languages) {
  auto intent_type = IntentType::kUnknown;
  // TODO(b/150034512): Take confidence level into consideration.
  if (languages.empty() ||
      languages.front()->locale == request.context.device_properties.language) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }
  intent_type = IntentType::kTranslation;
  std::move(complete_callback_)
      .Run(IntentInfo(request.selected_text, intent_type,
                      languages.front()->locale,
                      request.context.device_properties.language));
}

void IntentGenerator::MaybeGenerateTranslationIntent(
    const QuickAnswersRequest& request) {
  DCHECK(complete_callback_);

  if (!features::IsQuickAnswersTranslationEnabled()) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  // Don't do language detection if no device language is provided or the length
  // of selected text is above the threshold. Returns unknown intent type.
  if (request.context.device_properties.language.empty() ||
      request.selected_text.length() > kTranslationTextLengthThreshold) {
    std::move(complete_callback_)
        .Run(IntentInfo(request.selected_text, IntentType::kUnknown));
    return;
  }

  if (text_classifier_) {
    text_classifier_->FindLanguages(
        !request.context.surrounding_text.empty()
            ? request.context.surrounding_text
            : request.selected_text,
        base::BindOnce(&IntentGenerator::FindLanguagesCallback,
                       weak_factory_.GetWeakPtr(), request));
  }
}

}  // namespace quick_answers
}  // namespace chromeos
