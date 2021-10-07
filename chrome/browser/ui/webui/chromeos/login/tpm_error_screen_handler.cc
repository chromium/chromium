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

namespace chromeos {
namespace {
const char kTPMErrorDefaultStep[] = "default";
const char kTPMErrorOwnedStep[] = "tpm-owned";
}  // namespace

constexpr StaticOobeScreenId TpmErrorView::kScreenId;

TpmErrorScreenHandler::TpmErrorScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.TPMErrorMessageScreen.userActed");
}

TpmErrorScreenHandler::~TpmErrorScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void TpmErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("errorTpmFailureTitle", IDS_LOGIN_ERROR_TPM_FAILURE_TITLE);
  builder->Add("errorTpmFailureReboot", IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT);
  builder->Add("errorTpmFailureRebootButton",
               IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT_BUTTON);

  builder->Add("errorTPMOwnedTitle",
               IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_TITLE);
  builder->Add("errorTPMOwnedSubtitle",
               IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_SUBTITLE);
  builder->Add("errorTPMOwnedContent",
               IDS_LOGIN_ERROR_ENROLLMENT_TPM_FAILURE_CONTENT);

  // OS names
  builder->Add("osInstallChromiumOS", IDS_CHROMIUM_OS_NAME);
  builder->Add("osInstallCloudReadyOS", IDS_CLOUD_READY_OS_NAME);
}

void TpmErrorScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void TpmErrorScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void TpmErrorScreenHandler::SetTPMOwnedErrorStep(bool show_tpm_owned_step) {
  CallJS("login.TPMErrorMessageScreen.setStep",
         show_tpm_owned_step ? std::string(kTPMErrorOwnedStep)
                             : std::string(kTPMErrorDefaultStep));
}

void TpmErrorScreenHandler::SetIsBrandedBuild(bool is_branded) {
  CallJS("login.TPMErrorMessageScreen.setIsBrandedBuild", is_branded);
}

void TpmErrorScreenHandler::Bind(TpmErrorScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void TpmErrorScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

}  // namespace chromeos
