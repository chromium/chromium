// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

CryptohomeRecoverySetupScreenHandler::CryptohomeRecoverySetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

CryptohomeRecoverySetupScreenHandler::~CryptohomeRecoverySetupScreenHandler() =
    default;

void CryptohomeRecoverySetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("cryptohomeRecoverySetupErrorTitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_SETUP_ERROR_TITLE);
  builder->Add("cryptohomeRecoverySetupErrorSubtitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_SETUP_ERROR_SUBTITLE);
  builder->Add("cryptohomeRecoverySetupSkipButton",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_SETUP_SKIP_BUTTON);
  builder->Add("cryptohomeRecoverySetupRetryButton",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_SETUP_RETRY_BUTTON);
}

void CryptohomeRecoverySetupScreenHandler::Show() {
  ShowInWebUI();
}

void CryptohomeRecoverySetupScreenHandler::OnSetupFailed() {
  CallExternalAPI("onSetupFailed");
}

void CryptohomeRecoverySetupScreenHandler::SetLoadingState() {
  CallExternalAPI("setLoadingState");
}

base::WeakPtr<CryptohomeRecoverySetupScreenView>
CryptohomeRecoverySetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
