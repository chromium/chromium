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
#include "ui/base/ime/ash/input_method_manager.h"

namespace chromeos {

constexpr StaticOobeScreenId TermsOfServiceScreenView::kScreenId;

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.TermsOfServiceScreen.userActed");
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
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
  screen_ = screen;
}

void TermsOfServiceScreenHandler::Show(const std::string& manager) {
  manager_ = manager;
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  // Update the UI to show an error message or the Terms of Service.
  UpdateTermsOfServiceInUI();

  base::Value::Dict data;
  data.Set("manager", manager_);

  ShowInWebUI(std::move(data));
}

void TermsOfServiceScreenHandler::Hide() {}

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

void TermsOfServiceScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    Show(manager_);
    show_on_init_ = false;
  }
}

void TermsOfServiceScreenHandler::UpdateTermsOfServiceInUI() {
  if (!IsJavascriptAllowed())
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
