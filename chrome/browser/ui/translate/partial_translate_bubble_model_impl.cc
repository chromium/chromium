// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/partial_translate_bubble_model_impl.h"

#include <utility>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"

// TODO(crbug/1314825): When the PartialTranslateManager is added it will
// replace and take the role of the TranslateUIDelegate. TranslateUIDelegate
// calls have been left in this implementation as future reference for
// implementing PartialTranslateManager.
PartialTranslateBubbleModelImpl::PartialTranslateBubbleModelImpl(
    ViewState view_state,
    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate)
    : ui_delegate_(std::move(ui_delegate)) {
  DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
  DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
  current_view_state_ = view_state;
}

PartialTranslateBubbleModelImpl::~PartialTranslateBubbleModelImpl() = default;

PartialTranslateBubbleModelImpl::ViewState
PartialTranslateBubbleModelImpl::GetViewState() const {
  return current_view_state_;
}

void PartialTranslateBubbleModelImpl::SetViewState(
    PartialTranslateBubbleModelImpl::ViewState view_state) {
  current_view_state_ = view_state;
}

void PartialTranslateBubbleModelImpl::ShowError(
    translate::TranslateErrors::Type error_type) {
  // TODO(crbug/1314825): implement when partial translate specific
  // metrics are added.
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

void PartialTranslateBubbleModelImpl::Translate() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
}

void PartialTranslateBubbleModelImpl::RevertTranslation() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
}

bool PartialTranslateBubbleModelImpl::IsCurrentSelectionTranslated() const {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.

  // Normally we'd check the ui delegate stored languages against the page
  // LanguageState. To replace LanguageState we need to save the "currently
  // translated language", and we need to query the source language from the
  // PartialTranslateManager instead of the UIDelegate.
  // There is still the open question of whether the source language can be
  // changed - if not then this becomes easier. Presumably the target language
  // can change, so we will need to save that state because we can't rely on
  // LanguageState.
  return false;
}

void PartialTranslateBubbleModelImpl::TranslateFullPage(
    content::WebContents* web_contents) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  translate_manager->ShowTranslateUI(true);
}
