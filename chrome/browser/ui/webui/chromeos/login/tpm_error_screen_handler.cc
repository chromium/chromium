// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/tpm_error_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {
const char kTPMErrorOwnedStep[] = "tpm-owned";
const char kTPMErrorDbusStep[] = "dbus-error";
}  // namespace

constexpr StaticOobeScreenId TpmErrorView::kScreenId;

TpmErrorScreenHandler::TpmErrorScreenHandler() : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.TPMErrorMessageScreen.userActed");
}

TpmErrorScreenHandler::~TpmErrorScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void TpmErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("errorTpmFailureTitle", IDS_LOGIN_ERROR_TPM_FAILURE_TITLE);
  builder->Add("errorTpmDbusErrorTitle", IDS_LOGIN_ERROR_TPM_DBUS_ERROR_TITLE);
  builder->Add("errorTpmFailureReboot", IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT);
  builder->Add("errorTpmFailureRebootButton",
               IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT_BUTTON);

  builder->Add("errorTPMOwnedTitle",
               IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_TITLE);
  builder->AddF("errorTPMOwnedSubtitle",
                IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("errorTPMOwnedContent",
                IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_CONTENT,
                IDS_INSTALLED_PRODUCT_OS_NAME);
}

void TpmErrorScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void TpmErrorScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  ShowInWebUI();
}

void TpmErrorScreenHandler::SetTPMOwnedErrorStep() {
  CallJS("login.TPMErrorMessageScreen.setStep",
         std::string(kTPMErrorOwnedStep));
}

void TpmErrorScreenHandler::SetTPMDbusErrorStep() {
  CallJS("login.TPMErrorMessageScreen.setStep", std::string(kTPMErrorDbusStep));
}

void TpmErrorScreenHandler::Bind(TpmErrorScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
}

void TpmErrorScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

}  // namespace chromeos
