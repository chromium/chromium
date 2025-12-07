// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/translation_view_wrapper_impl.h"

#include <string>

#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"

namespace ash::babelorca {

TranslationViewWrapperImpl::TranslationViewWrapperImpl(
    CaptionBubbleSettingsImpl* caption_bubble_settings)
    : caption_bubble_settings_(caption_bubble_settings) {}

TranslationViewWrapperImpl::~TranslationViewWrapperImpl() = default;

views::MdTextButton*
TranslationViewWrapperImpl::GetTranslateToggleButtonForTesting() {
  return button(translate_toggle_button_index_);
}

void TranslationViewWrapperImpl::
    SimulateTranslateToggleButtonClickForTesting() {
  OnTranslateToggleButtonPressed();
}

captions::CaptionBubbleSettings*
TranslationViewWrapperImpl::caption_bubble_settings() {
  return caption_bubble_settings_;
}

void TranslationViewWrapperImpl::MaybeAddChildViews(
    views::View* translate_container) {
  translate_toggle_button_index_ = AddLanguageTextButton(
      translate_container,
      base::BindRepeating(
          &TranslationViewWrapperImpl::OnTranslateToggleButtonPressed,
          weak_ptr_factory_.GetWeakPtr()));
}

void TranslationViewWrapperImpl::UpdateLanguageLabelInternal() {
  captions::TranslationViewWrapperBase::UpdateLanguageLabelInternal();
  button(translate_toggle_button_index_)
      ->SetText(caption_bubble_settings_->GetLiveTranslateEnabled()
                    ? l10n_util::GetStringUTF16(
                          IDS_BOCA_CAPTIONS_STOP_TRANSLATING_BUTTON_TEXT)
                    : l10n_util::GetStringUTF16(
                          IDS_BOCA_CAPTIONS_TRANSLATION_AVAILABLE_BUTTON_TEXT));
}

void TranslationViewWrapperImpl::SetTranslationsViewVisible(
    bool live_translate_enabled) {
  captions::TranslationViewWrapperBase::SetTranslationsViewVisible(
      live_translate_enabled);
  button(translate_toggle_button_index_)
      ->SetVisible(caption_bubble_settings_->GetTranslateAllowed());
  if (!live_translate_enabled) {
    source_language_button()->SetVisible(false);
  }
}

void TranslationViewWrapperImpl::OnTranslateToggleButtonPressed() {
  caption_bubble_settings_->SetLiveTranslateEnabled(
      !caption_bubble_settings_->GetLiveTranslateEnabled());
}

}  // namespace ash::babelorca
