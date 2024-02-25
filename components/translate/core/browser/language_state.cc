// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/language_state.h"

#include "base/check.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_metrics_logger.h"

namespace translate {

LanguageState::LanguageState(TranslateDriver* driver)
    : is_page_translated_(false),
      translate_driver_(driver),
      page_level_translation_criteria_met_(false),
      translation_pending_(false),
      translation_error_(false),
      translation_declined_(false),
      is_same_document_navigation_(false),
      translate_enabled_(false),
      translation_type_(TranslationType::kUninitialized) {
  DCHECK(translate_driver_);
}

LanguageState::~LanguageState() = default;

void LanguageState::DidNavigate(bool is_same_document_navigation,
                                bool is_main_frame,
                                bool reload,
                                const std::string& href_translate,
                                bool navigation_from_google) {
  is_same_document_navigation_ = is_same_document_navigation;
  if (is_same_document_navigation_ || !is_main_frame)
    return;  // Don't reset our states, the page has not changed.

  if (reload) {
    // We might not get a LanguageDetermined notifications on reloads. Make sure
    // to keep the source language and to set current_lang_ so
    // IsPageTranslated() returns false.
    current_lang_ = source_lang_;
  } else {
    prev_source_lang_ = source_lang_;
    prev_current_lang_ = current_lang_;
    source_lang_.clear();
    current_lang_.clear();
  }

  SetIsPageTranslated(false);

  translation_pending_ = false;
  translation_error_ = false;
  translation_declined_ = false;
  translation_type_ = TranslationType::kUninitialized;
  href_translate_ = href_translate;
  navigation_from_google_ = navigation_from_google;

  SetTranslateEnabled(false);
}

void LanguageState::LanguageDetermined(
    const std::string& page_language,
    bool page_level_translation_criteria_met) {
  if (is_same_document_navigation_ && !source_lang_.empty()) {
    // Same-document navigation, we don't expect our states to change.
    // Note that we'll set the languages if source_lang_ is empty.  This might
    // happen if the we did not get called on the top-page.
    return;
  }
  page_level_translation_criteria_met_ = page_level_translation_criteria_met;
  source_lang_ = page_language;
  current_lang_ = page_language;
  SetIsPageTranslated(false);
}

bool LanguageState::InTranslateNavigation() const {
  // The user is in the same translate session if
  //   - no translation is pending
  //   - this page is in the same language as the previous page
  //   - the previous page had been translated
  //   - the new page was navigated through a link.
  return !translation_pending_ && prev_source_lang_ == source_lang_ &&
         prev_source_lang_ != prev_current_lang_ &&
         translate_driver_->IsLinkNavigation();
}

void LanguageState::SetSourceLanguage(const std::string& language) {
  source_lang_ = language;
  SetIsPageTranslated(current_lang_ != source_lang_);
}

void LanguageState::SetCurrentLanguage(const std::string& language) {
  current_lang_ = language;
  SetIsPageTranslated(current_lang_ != source_lang_);
}

std::string LanguageState::AutoTranslateTo() const {
  if (InTranslateNavigation() && !is_page_translated_)
    return prev_current_lang_;

  return std::string();
}

void LanguageState::SetTranslateEnabled(bool value) {
  if (translate_enabled_ == value)
    return;

  translate_enabled_ = value;
  translate_driver_->OnTranslateEnabledChanged();
}

bool LanguageState::HasLanguageChanged() const {
  return source_lang_ != prev_source_lang_;
}

void LanguageState::SetIsPageTranslated(bool value) {
  if (is_page_translated_ == value)
    return;

  is_page_translated_ = value;
  translate_driver_->OnIsPageTranslatedChanged();

  // With the translation done, the translate feature must be enabled.
  if (is_page_translated_)
    SetTranslateEnabled(true);
}

}  // namespace translate
