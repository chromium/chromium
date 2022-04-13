// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"

#include <utility>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_ui_delegate.h"

// TODO(crbug/1314825): When the PartialTranslateManager is added it will
// replace and take the role of the TranslateUIDelegate. TranslateUIDelegate
// calls have been left in this implementation as future reference for
// implementing PartialTranslateManager.
PartialTranslateBubbleModel::PartialTranslateBubbleModel(
    ViewState view_state,
    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate)
    : ui_delegate_(std::move(ui_delegate)) {
  DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
  DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
  current_view_state_ = view_state;
}

PartialTranslateBubbleModel::~PartialTranslateBubbleModel() = default;

PartialTranslateBubbleModel::ViewState
PartialTranslateBubbleModel::GetViewState() const {
  return current_view_state_;
}

void PartialTranslateBubbleModel::SetViewState(
    PartialTranslateBubbleModel::ViewState view_state) {
  current_view_state_ = view_state;
}

void PartialTranslateBubbleModel::ShowError(
    translate::TranslateErrors::Type error_type) {
  // TODO(crbug/1314825): implement when partial translate specific
  // metrics are added.
}

int PartialTranslateBubbleModel::GetNumberOfSourceLanguages() const {
  return ui_delegate_->GetNumberOfLanguages();
}

int PartialTranslateBubbleModel::GetNumberOfTargetLanguages() const {
  // Subtract 1 to account for unknown language option being omitted.
  return ui_delegate_->GetNumberOfLanguages() - 1;
}

std::u16string PartialTranslateBubbleModel::GetSourceLanguageNameAt(
    int index) const {
  return ui_delegate_->GetLanguageNameAt(index);
}

std::u16string PartialTranslateBubbleModel::GetTargetLanguageNameAt(
    int index) const {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  return ui_delegate_->GetLanguageNameAt(index + 1);
}

int PartialTranslateBubbleModel::GetSourceLanguageIndex() const {
  return ui_delegate_->GetSourceLanguageIndex();
}

void PartialTranslateBubbleModel::UpdateSourceLanguageIndex(int index) {
  ui_delegate_->UpdateSourceLanguageIndex(index);
}

int PartialTranslateBubbleModel::GetTargetLanguageIndex() const {
  // Subtract 1 to account for unknown language option being omitted from the
  // bubble target language list.
  return ui_delegate_->GetTargetLanguageIndex() - 1;
}

void PartialTranslateBubbleModel::UpdateTargetLanguageIndex(int index) {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  ui_delegate_->UpdateTargetLanguageIndex(index + 1);
}

void PartialTranslateBubbleModel::Translate() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
}

void PartialTranslateBubbleModel::RevertTranslation() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
}

bool PartialTranslateBubbleModel::IsCurrentSelectionTranslated() const {
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
