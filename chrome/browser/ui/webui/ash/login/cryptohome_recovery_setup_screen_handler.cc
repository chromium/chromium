// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"

namespace ash {

CryptohomeRecoverySetupScreenHandler::CryptohomeRecoverySetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

CryptohomeRecoverySetupScreenHandler::~CryptohomeRecoverySetupScreenHandler() =
    default;

void CryptohomeRecoverySetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void CryptohomeRecoverySetupScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace ash
