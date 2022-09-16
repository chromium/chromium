// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/cryptohome_recovery_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"

namespace chromeos {

CryptohomeRecoveryScreenHandler::CryptohomeRecoveryScreenHandler()
    : BaseScreenHandler(kScreenId) {}

CryptohomeRecoveryScreenHandler::~CryptohomeRecoveryScreenHandler() = default;

void CryptohomeRecoveryScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void CryptohomeRecoveryScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace chromeos
