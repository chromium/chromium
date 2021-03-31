// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

constexpr StaticOobeScreenId TermsOfServiceScreenView::kScreenId;

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.TermsOfServiceScreen.userActed");
}

TermsOfServiceScreenHandler::~TermsOfServiceScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void TermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("termsOfServiceScreenHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_HEADING);
  builder->Add("termsOfServiceScreenSubheading",
               IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
  builder->Add("termsOfServiceError", IDS_TERMS_OF_SERVICE_SCREEN_ERROR);
  builder->Add("termsOfServiceTryAgain", IDS_TERMS_OF_SERVICE_SCREEN_TRY_AGAIN);
  builder->Add("termsOfServiceBackButton",
               IDS_TERMS_OF_SERVICE_SCREEN_BACK_BUTTON);
  builder->Add("termsOfServiceAcceptButton",
               IDS_TERMS_OF_SERVICE_SCREEN_ACCEPT_BUTTON);
  builder->Add("termsOfServiceRetryButton",
               IDS_TERMS_OF_SERVICE_SCREEN_RETRY_BUTTON);
}

void TermsOfServiceScreenHandler::SetScreen(TermsOfServiceScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
}

void TermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  DoShow();
}

void TermsOfServiceScreenHandler::Hide() {
}

void TermsOfServiceScreenHandler::SetManager(const std::string& manager) {
  manager_ = manager;
  UpdateManagerInUI();
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

bool TermsOfServiceScreenHandler::AreTermsLoaded() {
  return !load_error_ && !terms_of_service_.empty();
}

void TermsOfServiceScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
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

  // Updates the manager name shown in the UI.
  UpdateManagerInUI();

  // Update the UI to show an error message or the Terms of Service.
  UpdateTermsOfServiceInUI();

  ShowScreen(kScreenId);
}

void TermsOfServiceScreenHandler::UpdateManagerInUI() {
  if (page_is_ready())
    CallJS("login.TermsOfServiceScreen.setManager", manager_);
}

void TermsOfServiceScreenHandler::UpdateTermsOfServiceInUI() {
  if (!page_is_ready())
    return;

  // If either `load_error_` or `terms_of_service_` is set, the download of the
  // Terms of Service has completed and the UI should be updated. Otherwise, the
  // download is still in progress and the UI will be updated when the
  // OnLoadError() or the OnLoadSuccess() callback is called.
  if (load_error_)
    CallJS("login.TermsOfServiceScreen.setTermsOfServiceLoadError");
  else if (!terms_of_service_.empty())
    CallJS("login.TermsOfServiceScreen.setTermsOfService", terms_of_service_);
}

}  // namespace chromeos
