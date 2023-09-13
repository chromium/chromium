// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

CryptohomeRecoveryScreenHandler::CryptohomeRecoveryScreenHandler()
    : BaseScreenHandler(kScreenId) {}

CryptohomeRecoveryScreenHandler::~CryptohomeRecoveryScreenHandler() = default;

void CryptohomeRecoveryScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("cryptohomeRecoverySuccessTitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_SUCCESS_TITLE);
  builder->Add("cryptohomeRecoveryErrorTitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_ERROR_TITLE);
  builder->Add("cryptohomeRecoveryReauthNotificationTitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_REAUTH_NOTIFICATION_TITLE);
  builder->Add("cryptohomeRecoveryReauthNotificationSubtitle",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_REAUTH_NOTIFICATION_SUBTITLE);
  builder->Add("cryptohomeRecoveryManualRecoveryButton",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_MANUAL_RECOVERY_BUTTON);
  builder->Add("cryptohomeRecoveryRetryButton",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_RETRY_BUTTON);
  builder->Add("cryptohomeRecoveryDoneButton",
               IDS_LOGIN_CRYPTOHOME_RECOVERY_DONE_BUTTON);
}

void CryptohomeRecoveryScreenHandler::Show() {
  ShowInWebUI();
}

void CryptohomeRecoveryScreenHandler::OnRecoverySucceeded() {
  CallExternalAPI("onRecoverySucceeded");
}

void CryptohomeRecoveryScreenHandler::OnRecoveryFailed() {
  CallExternalAPI("onRecoveryFailed");
}

void CryptohomeRecoveryScreenHandler::ShowReauthNotification() {
  CallExternalAPI("showReauthNotification");
}

}  // namespace ash
