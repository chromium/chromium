// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"

#include "base/values.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {
namespace {
const char kTPMErrorOwnedStep[] = "tpm-owned";
const char kTPMErrorDbusStep[] = "dbus-error";
}  // namespace

TpmErrorScreenHandler::TpmErrorScreenHandler() : BaseScreenHandler(kScreenId) {}

TpmErrorScreenHandler::~TpmErrorScreenHandler() = default;

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

void TpmErrorScreenHandler::Show() {
  ShowInWebUI();
}

void TpmErrorScreenHandler::SetTPMOwnedErrorStep() {
  CallExternalAPI("setStep", std::string(kTPMErrorOwnedStep));
}

void TpmErrorScreenHandler::SetTPMDbusErrorStep() {
  CallExternalAPI("setStep", std::string(kTPMErrorDbusStep));
}

base::WeakPtr<TpmErrorView> TpmErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
