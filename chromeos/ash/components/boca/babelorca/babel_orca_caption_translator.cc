// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"

#include <memory>

#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

BabelOrcaCaptionTranslator::BabelOrcaCaptionTranslator(
    std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher)
    : translation_dispatcher_(std::move(translation_dispatcher)) {}
BabelOrcaCaptionTranslator::~BabelOrcaCaptionTranslator() = default;

void BabelOrcaCaptionTranslator::InitTranslationAndSetCallback(
    OnTranslationCallback callback,
    const std::string& source_language,
    const std::string& target_language) {
  if (source_language != target_language &&
      target_language != target_language_) {
    boca::RecordBabelOrcaTranslationLanguage(target_language);
    boca::RecordBabelOrcaTranslationLanguageSwitched();
  }
  source_language_ = source_language;
  target_language_ = target_language;

  callback_ = callback;

  // TODO validate language string.
}

void BabelOrcaCaptionTranslator::Translate(
    const std::optional<media::SpeechRecognitionResult>& recognition_result) {
  // no callback = noop
  if (!callback_) {
    return;
  }

  // If recognition result is nullopt or the languages are the same
  // then immediately pass the recognition_result to the callback.
  if (!recognition_result || AreLanguagesTheSame()) {
    callback_.Run(recognition_result);
    return;
  }

  auto cache_result = translation_cache_.FindCachedTranslationOrRemaining(
      recognition_result->transcription, source_language_, target_language_);
  std::string cached_translation = cache_result.second;
  std::string string_to_translate = cache_result.first;

  // This logic mirrors the logic in SystemLiveCaptionService's
  // OnSpeechResult translation logic.
  if (!string_to_translate.empty()) {
    // TODO(376329088) record translation metric.
    translation_dispatcher_->GetTranslation(
        string_to_translate, source_language_, target_language_,
        base::BindOnce(
            &BabelOrcaCaptionTranslator::OnTranslationDispatcherCallback,
            weak_ptr_factory_.GetWeakPtr(), cached_translation,
            string_to_translate, source_language_, target_language_,
            recognition_result->is_final));
  } else {
    // If the entirety of the string to translate is found in the cache
    // return the cached translation rather than performing a redundant
    // translation.
    callback_.Run(media::SpeechRecognitionResult(cached_translation,
                                                 recognition_result->is_final));
  }
}

std::string BabelOrcaCaptionTranslator::GetLanguageComponentFromLocale(
    const std::string& locale) {
  return locale.substr(0, 2);
}

void BabelOrcaCaptionTranslator::OnTranslationDispatcherCallback(
    const std::string& cached_translation,
    const std::string& original_transcription,
    const std::string& source_language,
    const std::string& target_language,
    bool is_final,
    const std::string& result) {
  // Do nothing if callback has been reset between the
  // translation dispatch and the call to this function.
  // If the source or target language changes we should
  // also drop this translation.
  if (!callback_ ||
      GetLanguageComponentFromLocale(source_language_) !=
          GetLanguageComponentFromLocale(source_language) ||
      GetLanguageComponentFromLocale(target_language_) !=
          GetLanguageComponentFromLocale(target_language)) {
    return;
  }

  std::string formatted_result = result;

  bool is_non_ideographic_source_or_ideographic_target =
      IsNonIdeographicSourceOrIdeographicTarget();
  if (is_non_ideographic_source_or_ideographic_target && is_final) {
    translation_cache_.Clear();
  } else if (is_non_ideographic_source_or_ideographic_target) {
    translation_cache_.InsertIntoCache(original_transcription, result,
                                       source_language_, target_language_);
  } else if (is_final) {
    formatted_result += " ";
  }

  auto text = base::StrCat({cached_translation, formatted_result});
  callback_.Run(media::SpeechRecognitionResult(
      base::StrCat({cached_translation, formatted_result}), is_final));
}

bool BabelOrcaCaptionTranslator::IsNonIdeographicSourceOrIdeographicTarget() {
  return !::captions::IsIdeographicLocale(source_language_) ||
         ::captions::IsIdeographicLocale(target_language_);
}

bool BabelOrcaCaptionTranslator::AreLanguagesTheSame() {
  std::string source_language_component =
      GetLanguageComponentFromLocale(source_language_);
  std::string target_language_component =
      GetLanguageComponentFromLocale(target_language_);

  return source_language_component == target_language_component;
}

}  // namespace ash::babelorca
