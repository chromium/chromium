// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/partial_translate_bubble_model_impl.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/time/time.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "components/translate/content/browser/partial_translate_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"

namespace {

const char kTranslatePartialTranslationSourceLanguage[] =
    "Translate.PartialTranslation.SourceLanguage";
const char kTranslatePartialTranslationTargetLanguage[] =
    "Translate.PartialTranslation.TargetLanguage";
const char kTranslatePartialTranslationResponseTime[] =
    "Translate.PartialTranslation.ResponseTime";
const char kTranslatePartialTranslationTranslationStatus[] =
    "Translate.PartialTranslation.TranslationStatus";
const char kTranslatePartialTranslationTranslatedCharacterCount[] =
    "Translate.PartialTranslation.Translated.CharacterCount";

}  // namespace

PartialTranslateBubbleModelImpl::PartialTranslateBubbleModelImpl(
    ViewState view_state,
    translate::TranslateErrors error_type,
    const std::u16string& source_text,
    const std::u16string& target_text,
    std::unique_ptr<PartialTranslateManager> partial_translate_manager,
    std::unique_ptr<translate::TranslateUILanguagesManager>
        ui_languages_manager)
    : current_view_state_(view_state),
      error_type_(error_type),
      source_text_(source_text),
      target_text_(target_text),
      partial_translate_manager_(std::move(partial_translate_manager)),
      ui_languages_manager_(std::move(ui_languages_manager)) {
  DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
  DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
}

PartialTranslateBubbleModelImpl::~PartialTranslateBubbleModelImpl() = default;

void PartialTranslateBubbleModelImpl::AddObserver(
    PartialTranslateBubbleModel::Observer* obs) {
  observers_.AddObserver(obs);
}
void PartialTranslateBubbleModelImpl::RemoveObserver(
    PartialTranslateBubbleModel::Observer* obs) {
  observers_.RemoveObserver(obs);
}

PartialTranslateBubbleModelImpl::ViewState
PartialTranslateBubbleModelImpl::GetViewState() const {
  return current_view_state_;
}

void PartialTranslateBubbleModelImpl::SetViewState(
    PartialTranslateBubbleModelImpl::ViewState view_state) {
  current_view_state_ = view_state;
}

void PartialTranslateBubbleModelImpl::SetSourceLanguage(
    const std::string& language_code) {
  ui_languages_manager_->UpdateSourceLanguage(language_code);
}

void PartialTranslateBubbleModelImpl::SetTargetLanguage(
    const std::string& language_code) {
  ui_languages_manager_->UpdateTargetLanguage(language_code);
}

void PartialTranslateBubbleModelImpl::SetSourceText(
    const std::u16string& text) {
  source_text_ = text;
}

std::u16string PartialTranslateBubbleModelImpl::GetSourceText() const {
  return source_text_;
}

void PartialTranslateBubbleModelImpl::SetTargetText(
    const std::u16string& text) {
  // Note: Some languages have syntactic differences in use of ellipses.
  // Luxembourgish uses a leading space and is the only one of these languages
  // supported by Translate in Chrome. Given this, specific localization is not
  // handled, but could be in the future if more languages are included.
  if (source_text_truncated_)
    target_text_ = text + u"…";
  else
    target_text_ = text;
}

std::u16string PartialTranslateBubbleModelImpl::GetTargetText() const {
  return target_text_;
}

void PartialTranslateBubbleModelImpl::SetError(
    translate::TranslateErrors error_type) {
  error_type_ = error_type;
}

translate::TranslateErrors PartialTranslateBubbleModelImpl::GetError() const {
  return error_type_;
}

int PartialTranslateBubbleModelImpl::GetNumberOfSourceLanguages() const {
  return ui_languages_manager_->GetNumberOfLanguages();
}

int PartialTranslateBubbleModelImpl::GetNumberOfTargetLanguages() const {
  // Subtract 1 to account for unknown language option being omitted.
  return ui_languages_manager_->GetNumberOfLanguages() - 1;
}

std::u16string PartialTranslateBubbleModelImpl::GetSourceLanguageNameAt(
    int index) const {
  return ui_languages_manager_->GetLanguageNameAt(index);
}

std::u16string PartialTranslateBubbleModelImpl::GetTargetLanguageNameAt(
    int index) const {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUILanguagesManager language list.
  return ui_languages_manager_->GetLanguageNameAt(index + 1);
}

int PartialTranslateBubbleModelImpl::GetSourceLanguageIndex() const {
  return ui_languages_manager_->GetSourceLanguageIndex();
}

void PartialTranslateBubbleModelImpl::UpdateSourceLanguageIndex(int index) {
  ui_languages_manager_->UpdateSourceLanguageIndex(index);
}

int PartialTranslateBubbleModelImpl::GetTargetLanguageIndex() const {
  // Subtract 1 to account for unknown language option being omitted from the
  // bubble target language list.
  return ui_languages_manager_->GetTargetLanguageIndex() - 1;
}

void PartialTranslateBubbleModelImpl::UpdateTargetLanguageIndex(int index) {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUILanguagesManager language list.
  ui_languages_manager_->UpdateTargetLanguageIndex(index + 1);
}

std::string PartialTranslateBubbleModelImpl::GetSourceLanguageCode() const {
  return ui_languages_manager_->GetSourceLanguageCode();
}

std::string PartialTranslateBubbleModelImpl::GetTargetLanguageCode() const {
  return ui_languages_manager_->GetTargetLanguageCode();
}

void PartialTranslateBubbleModelImpl::Translate(
    content::WebContents* web_contents) {
  PartialTranslateRequest request;
  // If the selected text was truncated, strip the trailing ellipses before
  // sending for translation.
  std::u16string source_text = GetSourceText();
  if (source_text_truncated_)
    request.selection_text = source_text.substr(0, source_text.size() - 1);
  else
    request.selection_text = source_text;

  request.selection_encoding = web_contents->GetEncoding();
  std::string source_language_code = GetSourceLanguageCode();
  if (source_language_code != translate::kUnknownLanguageCode) {
    // |source_language_code| will be kUnknownLanguageCode if it was initially
    // returned by page language detection, or if the user explicitly selects
    // "Detected Language" in the language list. In such cases,
    // |request.source_language| is left as an "empty" value.
    request.source_language = source_language_code;
  }
  request.target_language = GetTargetLanguageCode();

  // If this is the initial Partial Translate request, then the source language
  // should be used as a hint for backend language detection.
  request.apply_lang_hint = !initial_request_completed_;

  translate_request_started_time_ = base::TimeTicks::Now();

  RecordHistogramsOnPartialTranslateStart();

  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  translate_manager->translate_client()
      ->GetTranslatePrefs()
      ->SetRecentTargetLanguage(GetTargetLanguageCode());

  // Cancels any ongoing requests.
  partial_translate_manager_->StartPartialTranslate(
      web_contents, request,
      base::BindOnce(
          &PartialTranslateBubbleModelImpl::OnPartialTranslateResponse,
          // |partial_translate_manager_| is owned by Model and will be
          // destructed (cancelling the Callback) if Model is.
          base::Unretained(this), request));
}

void PartialTranslateBubbleModelImpl::TranslateFullPage(
    content::WebContents* web_contents) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  translate_manager->ShowTranslateUI(GetTargetLanguageCode(), true);
}

void PartialTranslateBubbleModelImpl::SetSourceTextTruncated(
    bool is_truncated) {
  source_text_truncated_ = is_truncated;
}

void PartialTranslateBubbleModelImpl::OnPartialTranslateResponse(
    const PartialTranslateRequest& request,
    const PartialTranslateResponse& response) {
  translate_response_received_time_ = base::TimeTicks::Now();

  bool status_error = (response.status != PartialTranslateStatus::kSuccess);
  if (status_error) {
    error_type_ = translate::TranslateErrors::TRANSLATION_ERROR;
    SetViewState(PartialTranslateBubbleModel::VIEW_STATE_ERROR);
  } else {
    SetSourceLanguage(response.source_language);
    SetTargetLanguage(response.target_language);
    SetTargetText(response.translated_text);
    SetViewState(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
    error_type_ = translate::TranslateErrors::NONE;
    initial_request_completed_ = true;
  }

  RecordHistogramsOnPartialTranslateComplete(status_error);

  for (PartialTranslateBubbleModel::Observer& obs : observers_) {
    obs.OnPartialTranslateComplete();
  }
}

void PartialTranslateBubbleModelImpl::
    RecordHistogramsOnPartialTranslateStart() {
  base::UmaHistogramSparse(kTranslatePartialTranslationSourceLanguage,
                           base::HashMetricName(GetSourceLanguageCode()));
  base::UmaHistogramSparse(kTranslatePartialTranslationTargetLanguage,
                           base::HashMetricName(GetTargetLanguageCode()));
}

void PartialTranslateBubbleModelImpl::
    RecordHistogramsOnPartialTranslateComplete(bool status_error) {
  base::UmaHistogramMediumTimes(
      kTranslatePartialTranslationResponseTime,
      translate_response_received_time_ - translate_request_started_time_);
  // All PartialTranslateTranslationStatus enum values >0 represent error
  // states. Currently there is only one error value, but this can be split into
  // specific error types in the future.
  base::UmaHistogramBoolean(kTranslatePartialTranslationTranslationStatus,
                            status_error);

  if (!status_error) {
    base::UmaHistogramCounts100000(
        kTranslatePartialTranslationTranslatedCharacterCount,
        target_text_.length());
  }
}
