// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"

#include <memory>

#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::babelorca {

namespace {

bool IsNonIdeographicSourceOrIdeographicTarget(
    const std::string& source_language,
    const std::string& target_language) {
  return !::captions::IsIdeographicLocale(source_language) ||
         ::captions::IsIdeographicLocale(target_language);
}

bool AreLanguagesTheSame(const std::string& source_language,
                         const std::string& target_language) {
  std::string source_language_component =
      l10n_util::GetLanguage(source_language);
  std::string target_language_component =
      l10n_util::GetLanguage(target_language);

  return source_language_component == target_language_component;
}

}  // namespace

BabelOrcaCaptionTranslator::BabelOrcaCaptionTranslator(
    std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher)
    : translation_dispatcher_(std::move(translation_dispatcher)) {}
BabelOrcaCaptionTranslator::~BabelOrcaCaptionTranslator() = default;

void BabelOrcaCaptionTranslator::Translate(
    const media::SpeechRecognitionResult& recognition_result,
    OnTranslationCallback callback,
    const std::string& source_language,
    const std::string& target_language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // no callback = noop
  if (!callback) {
    VLOG(1) << "Must pass a valid callback to translate.";
    return;
  }

  if (!l10n_util::IsValidLocaleSyntax(source_language) ||
      !l10n_util::IsValidLocaleSyntax(target_language)) {
    VLOG(1) << "Invalid source or target language syntax.  Will not translate";
    std::move(callback).Run(recognition_result);
    return;
  }

  // If the languages are the same then immediately
  // pass the recognition_result to the callback.
  if (AreLanguagesTheSame(source_language, target_language)) {
    std::move(callback).Run(recognition_result);
    return;
  }

  // Check that if the languages have changed in between calls that we then
  // record the language switch metrics.
  if ((current_source_language_.has_value() &&
       !AreLanguagesTheSame(source_language,
                            current_source_language_.value())) ||
      (current_target_language_.has_value() &&
       !AreLanguagesTheSame(target_language,
                            current_target_language_.value()))) {
    boca::RecordBabelOrcaTranslationLanguageSwitched();
  }

  // If the target language changed record it here.
  if (current_target_language_.has_value() &&
      !AreLanguagesTheSame(target_language, current_target_language_.value())) {
    boca::RecordBabelOrcaTranslationLanguage(target_language);
  }
  current_source_language_ = source_language;
  current_target_language_ = target_language;

  // Enqueue the dispatch, we might have to wait until a pending translation is
  // completed before dispatching the current one.
  dispatch_queue_.push(
      base::BindOnce(&BabelOrcaCaptionTranslator::DispatchTranslationRequest,
                     weak_ptr_factory_.GetWeakPtr(), recognition_result,
                     source_language, target_language, std::move(callback)));

  // If there isn't a segment pending then we will dispatch immediately.
  if (!pending_is_final_) {
    PopQueue();
  }
}

void BabelOrcaCaptionTranslator::SetDefaultLanguagesForTesting(
    const std::string& default_source,
    const std::string& default_target) {
  current_source_language_ = default_source;
  current_target_language_ = default_target;
}

void BabelOrcaCaptionTranslator::UnsetCurrentLanguagesForTesting() {
  current_source_language_ = std::nullopt;
  current_target_language_ = std::nullopt;
}

void BabelOrcaCaptionTranslator::DispatchTranslationRequest(
    const media::SpeechRecognitionResult& sr_result,
    const std::string& source_language,
    const std::string& target_language,
    OnTranslationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Because segments will always come in order if our current
  // segment is not final and there is more than one dispatch
  // in the queue we know that a subsequent dispatch contains a
  // superstring of the current dispatch so we can skip the
  // current dispatch to prevent redundant work.
  if (!sr_result.is_final && !dispatch_queue_.empty()) {
    PopQueue();
    return;
  }

  // Check the cache for work that is already done.
  auto cache_result = translation_cache_.FindCachedTranslationOrRemaining(
      sr_result.transcription, source_language, target_language);
  std::string cached_translation = cache_result.second;
  std::string string_to_translate = cache_result.first;

  // If we have the entirety of the translation in the cache there is no
  // reason to dispatch, just return the cached value. In terms of the
  // dispatch queue this is semantically equivalent to dispatching
  // a translation and receiving the response, so flush the queue
  // as well.
  if (string_to_translate.empty()) {
    std::move(callback).Run(
        media::SpeechRecognitionResult(cached_translation, sr_result.is_final));
    PopQueue();
    return;
  }

  pending_is_final_ = sr_result.is_final;

  // We actually have something to translate, so actually dispatch
  // it.
  translation_dispatcher_->GetTranslation(
      string_to_translate, source_language, target_language,
      base::BindOnce(
          &BabelOrcaCaptionTranslator::OnTranslationDispatcherCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          cached_translation, sr_result.transcription, source_language,
          target_language, sr_result.is_final));
}

void BabelOrcaCaptionTranslator::OnTranslationDispatcherCallback(
    OnTranslationCallback callback,
    const std::string& cached_translation,
    const std::string& original_transcription,
    const std::string& source_language,
    const std::string& target_language,
    bool is_final,
    const captions::TranslateEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // In case of error just return the non-translated text and dip
  // out.
  //
  // TODO(384018894) Record error metrics?
  // TODO(384026579) Communicate something went wrong to the
  // end user.
  if (!event.has_value()) {
    VLOG(1) << event.error();
    std::move(callback).Run(
        media::SpeechRecognitionResult(original_transcription, is_final));
    return;
  }

  std::string formatted_result = event.value();

  bool is_non_ideographic_source_or_ideographic_target =
      IsNonIdeographicSourceOrIdeographicTarget(source_language,
                                                target_language);
  // The comments below are originally from the SystemLiveCaptionService.
  // They appear here to explain the logic that this function mirrors
  // from the service's OnTranslationCallback.
  //
  // Don't cache the translation if the source language is an ideographic
  // language but the target language is not. This avoids translate
  // sentence by sentence because the Cloud Translation API does not properly
  // translate idographic punctuation marks.
  if (is_non_ideographic_source_or_ideographic_target && is_final) {
    translation_cache_.Clear();
  } else if (is_non_ideographic_source_or_ideographic_target) {
    translation_cache_.InsertIntoCache(original_transcription, event.value(),
                                       source_language, target_language);
  } else if (is_final) {
    // Append a space after final results when translating from an ideographic
    // to non-ideographic locale. The Speech On-Device API (SODA) automatically
    // prepends a space to recognition events after a final event, but only for
    // non-ideographic locales.
    //
    // note: while crbug.com/40261536 is resolved the original comment adds
    // a todo here referencing that bug and questioning whether this logic
    // should be added to the live translate controller.
    formatted_result += " ";
  }

  std::move(callback).Run(media::SpeechRecognitionResult(
      base::StrCat({cached_translation, formatted_result}), is_final));

  pending_is_final_ = false;

  PopQueue();
}

void BabelOrcaCaptionTranslator::PopQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dispatch_queue_.empty()) {
    return;
  }

  auto cb = std::move(dispatch_queue_.front());
  dispatch_queue_.pop();
  std::move(cb).Run();
}

}  // namespace ash::babelorca
