// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"

#include <memory>

#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
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
  if (!recognition_result || source_language_ == target_language_) {
    callback_.Run(recognition_result);
    return;
  }

  auto cache_result = translation_cache_.FindCachedTranslationOrRemaining(
      recognition_result->transcription, source_language_, target_language_);
  std::string cached_translation = cache_result.second;
  std::string string_to_translate = cache_result.first;

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
    callback_.Run(recognition_result);
  }
}

void BabelOrcaCaptionTranslator::OnTranslationDispatcherCallback(
    const std::string& cached_translation,
    const std::string& original_transcription,
    const std::string& source_language,
    const std::string& target_language,
    bool is_final,
    const std::string& result) {
  // We should never get here without the callback.
  CHECK(callback_);

  // We should always have the same source and target languages
  // on both sides of the transaction.
  CHECK_EQ(source_language_, source_language);
  CHECK_EQ(target_language_, target_language);

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

}  // namespace ash::babelorca
