// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_infobar_delegate.h"

#include <algorithm>
#include <utility>

#include "base/i18n/string_compare.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/common/language_experiments.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace translate {

BASE_FEATURE(kTranslateCompactUI,
             "TranslateCompactUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

const size_t TranslateInfoBarDelegate::kNoIndex =
    TranslateUILanguagesManager::kNoIndex;

TranslateInfoBarDelegate::~TranslateInfoBarDelegate() {
  for (auto& observer : observers_) {
    observer.OnTranslateInfoBarDelegateDestroyed(this);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
TranslateInfoBarDelegate::GetIdentifier() const {
  return TRANSLATE_INFOBAR_DELEGATE_NON_AURA;
}

// static
void TranslateInfoBarDelegate::Create(
    bool replace_existing_infobar,
    const base::WeakPtr<TranslateManager>& translate_manager,
    infobars::InfoBarManager* infobar_manager,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    TranslateErrors error_type,
    bool triggered_from_menu) {
  DCHECK(translate_manager);
  DCHECK(infobar_manager);

  // Check preconditions.
  if (step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
    DCHECK(TranslateDownloadManager::IsSupportedLanguage(target_language));
    if (!TranslateDownloadManager::IsSupportedLanguage(source_language)) {
      // If the source language is unsupported than it must be the unknown
      // language. On iOS, the source language can only be unknown for the
      // "translating" infobar, which is the case when the user started a
      // translation from the context menu.
#if BUILDFLAG(IS_IOS)
      DCHECK(step == translate::TRANSLATE_STEP_TRANSLATING ||
             step == translate::TRANSLATE_STEP_AFTER_TRANSLATE);
#endif
      DCHECK_EQ(translate::kUnknownLanguageCode, source_language);
    }
  }

  // Find any existing translate infobar delegate.
  infobars::InfoBar* old_infobar = NULL;
  TranslateInfoBarDelegate* old_delegate = NULL;
  for (infobars::InfoBar* infobar : infobar_manager->infobars()) {
    old_infobar = infobar;
    old_delegate = old_infobar->delegate()->AsTranslateInfoBarDelegate();
    if (old_delegate) {
      if (!replace_existing_infobar) {
        return;
      }
      break;
    }
  }

  if (old_delegate) {
    if (!triggered_from_menu) {
      // Try to reuse existing translate infobar delegate.
      old_delegate->step_ = step;
      for (auto& observer : old_delegate->observers_) {
        observer.OnTargetLanguageChanged(target_language);
        observer.OnTranslateStepChanged(step, error_type);
      }
      return;
    }
    // The old infobar may still be visible, but a new translate flow started.
    // Remove the previous infobar and add a new one.
    infobar_manager->RemoveInfoBar(old_infobar);
  }

  // Add the new delegate.
  TranslateClient* translate_client = translate_manager->translate_client();
  std::unique_ptr<infobars::InfoBar> infobar(translate_client->CreateInfoBar(
      base::WrapUnique(new TranslateInfoBarDelegate(
          translate_manager, step, source_language, target_language, error_type,
          triggered_from_menu))));
  infobar_manager->AddInfoBar(std::move(infobar));
}

size_t TranslateInfoBarDelegate::num_languages() const {
  return ui_languages_manager_->GetNumberOfLanguages();
}

std::string TranslateInfoBarDelegate::language_code_at(size_t index) const {
  return ui_languages_manager_->GetLanguageCodeAt(index);
}

std::u16string TranslateInfoBarDelegate::language_name_at(size_t index) const {
  return ui_languages_manager_->GetLanguageNameAt(index);
}

std::u16string TranslateInfoBarDelegate::source_language_name() const {
  return language_name_at(ui_languages_manager_->GetSourceLanguageIndex());
}

std::u16string TranslateInfoBarDelegate::initial_source_language_name() const {
  return language_name_at(
      ui_languages_manager_->GetInitialSourceLanguageIndex());
}

std::u16string TranslateInfoBarDelegate::target_language_name() const {
  return language_name_at(ui_languages_manager_->GetTargetLanguageIndex());
}

std::u16string TranslateInfoBarDelegate::unknown_language_name() const {
  return ui_languages_manager_->GetUnknownLanguageDisplayName();
}

void TranslateInfoBarDelegate::UpdateSourceLanguage(
    const std::string& language_code) {
  ui_delegate_.UpdateAndRecordSourceLanguage(language_code);
}

void TranslateInfoBarDelegate::UpdateTargetLanguage(
    const std::string& language_code) {
  ui_delegate_.UpdateAndRecordTargetLanguage(language_code);
}

void TranslateInfoBarDelegate::Translate() {
  ui_delegate_.Translate();
}

void TranslateInfoBarDelegate::RevertTranslation() {
  ui_delegate_.RevertTranslation();
  infobar()->RemoveSelf();
}

void TranslateInfoBarDelegate::RevertWithoutClosingInfobar() {
  ui_delegate_.RevertTranslation();
  step_ = TRANSLATE_STEP_BEFORE_TRANSLATE;
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  ui_delegate_.TranslationDeclined(true);
}

bool TranslateInfoBarDelegate::IsTranslatableLanguageByPrefs() const {
  TranslateClient* client = translate_manager_->translate_client();
  std::unique_ptr<TranslatePrefs> translate_prefs(client->GetTranslatePrefs());
  return translate_prefs->CanTranslateLanguage(source_language_code());
}

void TranslateInfoBarDelegate::ToggleTranslatableLanguageByPrefs() {
  ui_delegate_.SetLanguageBlocked(!ui_delegate_.IsLanguageBlocked());
}

bool TranslateInfoBarDelegate::IsSiteOnNeverPromptList() const {
  return ui_delegate_.IsSiteOnNeverPromptList();
}

void TranslateInfoBarDelegate::ToggleNeverPromptSite() {
  ui_delegate_.SetNeverPromptSite(!ui_delegate_.IsSiteOnNeverPromptList());
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() const {
  return ui_delegate_.ShouldAlwaysTranslate();
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  ui_delegate_.SetAlwaysTranslate(!ui_delegate_.ShouldAlwaysTranslate());
}

#if BUILDFLAG(IS_IOS)
void TranslateInfoBarDelegate::ShowNeverTranslateInfobar() {
  // Return if the infobar is not owned.
  if (!infobar()->owner())
    return;

  Create(true, translate_manager_, infobar()->owner(),
         translate::TRANSLATE_STEP_NEVER_TRANSLATE, source_language_code(),
         target_language_code(), TranslateErrors::NONE, false);
}
#endif

bool TranslateInfoBarDelegate::ShouldAutoAlwaysTranslate() {
  return ui_delegate_.ShouldAutoAlwaysTranslate();
}

bool TranslateInfoBarDelegate::ShouldAutoNeverTranslate() {
  return ui_delegate_.ShouldAutoNeverTranslate();
}

// static

TranslateDriver* TranslateInfoBarDelegate::GetTranslateDriver() {
  if (!translate_manager_)
    return NULL;

  return translate_manager_->translate_client()->GetTranslateDriver();
}

void TranslateInfoBarDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TranslateInfoBarDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

TranslateInfoBarDelegate::TranslateInfoBarDelegate(
    const base::WeakPtr<TranslateManager>& translate_manager,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    TranslateErrors error_type,
    bool triggered_from_menu)
    : infobars::InfoBarDelegate(),
      step_(step),
      ui_delegate_(translate_manager, source_language, target_language),
      translate_manager_(translate_manager),
      ui_languages_manager_(ui_delegate_.translate_ui_languages_manager()),
      error_type_(error_type),
      prefs_(translate_manager->translate_client()->GetTranslatePrefs()),
      triggered_from_menu_(triggered_from_menu) {
  DCHECK_NE((step_ == translate::TRANSLATE_STEP_TRANSLATE_ERROR),
            (error_type_ == TranslateErrors::NONE));
  DCHECK(translate_manager_);
}

int TranslateInfoBarDelegate::GetIconId() const {
  return 0;
}

void TranslateInfoBarDelegate::InfoBarDismissed() {
  OnInfoBarClosedByUser();
  ReportUIInteraction(UIInteraction::kCloseUIExplicitly);

  bool declined = false;
  bool has_observer = false;
  for (auto& observer : observers_) {
    has_observer = true;
    if (observer.IsDeclinedByUser())
      declined = true;
  }

  if (!has_observer)
    declined = step_ == translate::TRANSLATE_STEP_BEFORE_TRANSLATE;

  if (declined) {
    // The user closed the infobar without clicking the translate button.
    TranslationDeclined();
  }
}

#if BUILDFLAG(IS_IOS)
TranslateInfoBarDelegate*
TranslateInfoBarDelegate::AsTranslateInfoBarDelegate() {
  return this;
}
#endif

void TranslateInfoBarDelegate::OnInfoBarClosedByUser() {
  ui_delegate_.OnUIClosedByUser();
}

void TranslateInfoBarDelegate::ReportUIInteraction(
    UIInteraction ui_interaction) {
  ui_delegate_.ReportUIInteraction(ui_interaction);
}

}  // namespace translate
