// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/partial_translate_bubble_model_impl.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"

namespace {

// Returns a vector of language codes that represent the user's fluent
// languages.
std::vector<std::string> GetFluentLanguages(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(profile->GetPrefs());
  return translate_prefs->GetNeverTranslateLanguages();
}

}  // namespace

PartialTranslateBubbleModelImpl::PartialTranslateBubbleModelImpl(
    ViewState view_state,
    translate::TranslateErrors error_type,
    const std::u16string& source_text,
    const std::u16string& target_text,
    std::unique_ptr<PartialTranslateManager> partial_translate_manager,
    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate)
    : current_view_state_(view_state),
      error_type_(error_type),
      source_text_(source_text),
      target_text_(target_text),
      partial_translate_manager_(std::move(partial_translate_manager)),
      ui_delegate_(std::move(ui_delegate)) {
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
  ui_delegate_->UpdateSourceLanguage(language_code);
}

void PartialTranslateBubbleModelImpl::SetTargetLanguage(
    const std::string& language_code) {
  ui_delegate_->UpdateTargetLanguage(language_code);
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
  return ui_delegate_->GetNumberOfLanguages();
}

int PartialTranslateBubbleModelImpl::GetNumberOfTargetLanguages() const {
  // Subtract 1 to account for unknown language option being omitted.
  return ui_delegate_->GetNumberOfLanguages() - 1;
}

std::u16string PartialTranslateBubbleModelImpl::GetSourceLanguageNameAt(
    int index) const {
  return ui_delegate_->GetLanguageNameAt(index);
}

std::u16string PartialTranslateBubbleModelImpl::GetTargetLanguageNameAt(
    int index) const {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  return ui_delegate_->GetLanguageNameAt(index + 1);
}

int PartialTranslateBubbleModelImpl::GetSourceLanguageIndex() const {
  return ui_delegate_->GetSourceLanguageIndex();
}

void PartialTranslateBubbleModelImpl::UpdateSourceLanguageIndex(int index) {
  ui_delegate_->UpdateSourceLanguageIndex(index);
}

int PartialTranslateBubbleModelImpl::GetTargetLanguageIndex() const {
  // Subtract 1 to account for unknown language option being omitted from the
  // bubble target language list.
  return ui_delegate_->GetTargetLanguageIndex() - 1;
}

void PartialTranslateBubbleModelImpl::UpdateTargetLanguageIndex(int index) {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  ui_delegate_->UpdateTargetLanguageIndex(index + 1);
}

std::string PartialTranslateBubbleModelImpl::GetSourceLanguageCode() const {
  return ui_delegate_->GetSourceLanguageCode();
}

std::string PartialTranslateBubbleModelImpl::GetTargetLanguageCode() const {
  return ui_delegate_->GetTargetLanguageCode();
}

void PartialTranslateBubbleModelImpl::Translate(
    content::WebContents* web_contents) {
  PartialTranslateRequest request;
  request.selection_text = GetSourceText();
  request.selection_encoding = web_contents->GetEncoding();
  request.source_language = ui_delegate_->GetSourceLanguageCode();
  request.target_language = ui_delegate_->GetTargetLanguageCode();
  request.fluent_languages = GetFluentLanguages(web_contents);

  // Cancels any ongoing requests.
  partial_translate_manager_->StartPartialTranslate(
      web_contents, request,
      base::BindOnce(
          &PartialTranslateBubbleModelImpl::OnPartialTranslateResponse,
          // partial_translate_manager_ is owned by Model and will be
          // destructed (cancelling the Callback) if Model is.
          base::Unretained(this), request));
}

void PartialTranslateBubbleModelImpl::TranslateFullPage(
    content::WebContents* web_contents) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  translate_manager->ShowTranslateUI(true);
}

void PartialTranslateBubbleModelImpl::OnPartialTranslateResponse(
    const PartialTranslateRequest& request,
    const PartialTranslateResponse& response) {
  SetSourceLanguage(response.source_language);
  SetTargetLanguage(response.target_language);
  SetSourceText(request.selection_text);
  SetTargetText(response.translated_text);
  SetViewState(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  error_type_ = translate::TranslateErrors::NONE;

  for (PartialTranslateBubbleModel::Observer& obs : observers_) {
    obs.OnPartialTranslateComplete();
  }
}
