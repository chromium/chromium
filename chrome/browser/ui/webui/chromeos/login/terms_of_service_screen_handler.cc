// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

constexpr StaticOobeScreenId TermsOfServiceScreenView::kScreenId;

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler(
    JSCallsContainer* js_calls_container,
    CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
}

TermsOfServiceScreenHandler::~TermsOfServiceScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void TermsOfServiceScreenHandler::RegisterMessages() {
  AddCallback("termsOfServiceBack",
              &TermsOfServiceScreenHandler::HandleBack);
  AddCallback("termsOfServiceAccept",
              &TermsOfServiceScreenHandler::HandleAccept);
}

void TermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("termsOfServiceScreenHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_HEADING);
  builder->Add("termsOfServiceScreenSubheading",
               IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING);
  builder->Add("termsOfServiceContentHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_CONTENT_HEADING);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
  builder->Add("termsOfServiceError", IDS_TERMS_OF_SERVICE_SCREEN_ERROR);
  builder->Add("termsOfServiceTryAgain", IDS_TERMS_OF_SERVICE_SCREEN_TRY_AGAIN);
  builder->Add("termsOfServiceBackButton",
               IDS_TERMS_OF_SERVICE_SCREEN_BACK_BUTTON);
  builder->Add("termsOfServiceAcceptButton",
               IDS_TERMS_OF_SERVICE_SCREEN_ACCEPT_BUTTON);
}

void TermsOfServiceScreenHandler::SetDelegate(TermsOfServiceScreen* screen) {
  screen_ = screen;
}

void TermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  std::string locale =
      ProfileHelper::Get()
          ->GetProfileByUserUnsafe(
              user_manager::UserManager::Get()->GetActiveUser())
          ->GetPrefs()
          ->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&locale);

  if (locale.empty() || locale == g_browser_process->GetApplicationLocale()) {
    // If the user has not chosen a UI locale yet or the chosen locale matches
    // the current UI locale, show the screen immediately.
    DoShow();
    return;
  }

  // Switch to the user's UI locale before showing the screen.
  locale_util::SwitchLanguageCallback callback(
      base::Bind(&TermsOfServiceScreenHandler::OnLanguageChangedCallback,
                 base::Unretained(this)));
  locale_util::SwitchLanguage(locale,
                              true,   // enable_locale_keyboard_layouts
                              false,  // login_layouts_only
                              callback, ProfileManager::GetActiveUserProfile());
}

void TermsOfServiceScreenHandler::Hide() {
}

void TermsOfServiceScreenHandler::SetDomain(const std::string& domain) {
  domain_ = domain;
  UpdateDomainInUI();
}

void TermsOfServiceScreenHandler::OnLoadError() {
  load_error_ = true;
  terms_of_service_ = "";
  UpdateTermsOfServiceInUI();
}

void TermsOfServiceScreenHandler::OnLoadSuccess(
    const std::string& terms_of_service) {
  load_error_ = false;
  terms_of_service_ = terms_of_service;
  UpdateTermsOfServiceInUI();
}

void TermsOfServiceScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void TermsOfServiceScreenHandler::OnLanguageChangedCallback(
    const locale_util::LanguageSwitchResult& result) {
  // Update the screen contents to the new locale.
  base::DictionaryValue localized_strings;
  GetOobeUI()->GetLocalizedStrings(&localized_strings);
  core_oobe_view_->ReloadContent(localized_strings);

  DoShow();
}

void TermsOfServiceScreenHandler::DoShow() {
  // Determine the user's most preferred input method.
  std::vector<std::string> input_methods = base::SplitString(
      ProfileHelper::Get()
          ->GetProfileByUserUnsafe(
              user_manager::UserManager::Get()->GetActiveUser())
          ->GetPrefs()
          ->GetString(prefs::kLanguagePreloadEngines),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (!input_methods.empty()) {
    // If the user has a preferred input method, enable it and switch to it.
    chromeos::input_method::InputMethodManager* input_method_manager =
        chromeos::input_method::InputMethodManager::Get();
    input_method_manager->GetActiveIMEState()->EnableInputMethod(
        input_methods.front());
    input_method_manager->GetActiveIMEState()->ChangeInputMethod(
        input_methods.front(), false /* show_message */);
  }

  // Updates the domain name shown in the UI.
  UpdateDomainInUI();

  // Update the UI to show an error message or the Terms of Service.
  UpdateTermsOfServiceInUI();

  ShowScreen(kScreenId);
}

void TermsOfServiceScreenHandler::UpdateDomainInUI() {
  if (page_is_ready())
    CallJS("login.TermsOfServiceScreen.setDomain", domain_);
}

void TermsOfServiceScreenHandler::UpdateTermsOfServiceInUI() {
  if (!page_is_ready())
    return;

  // If either |load_error_| or |terms_of_service_| is set, the download of the
  // Terms of Service has completed and the UI should be updated. Otherwise, the
  // download is still in progress and the UI will be updated when the
  // OnLoadError() or the OnLoadSuccess() callback is called.
  if (load_error_)
    CallJS("login.TermsOfServiceScreen.setTermsOfServiceLoadError");
  else if (!terms_of_service_.empty())
    CallJS("login.TermsOfServiceScreen.setTermsOfService", terms_of_service_);
}

void TermsOfServiceScreenHandler::HandleBack() {
  if (screen_)
    screen_->OnDecline();
}

void TermsOfServiceScreenHandler::HandleAccept() {
  if (!screen_)
    return;

  // If the Terms of Service have not been successfully downloaded, the "accept
  // and continue" button should not be accessible. If the user managed to
  // activate it somehow anway, do not treat this as acceptance of the Terms
  // and Conditions and end the session instead, as if the user had declined.
  if (terms_of_service_.empty())
    screen_->OnDecline();
  else
    screen_->OnAccept();
}

}  // namespace chromeos
