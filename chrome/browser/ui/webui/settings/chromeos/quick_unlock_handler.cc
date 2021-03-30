// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/quick_unlock_handler.h"

#include "base/bind.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

QuickUnlockHandler::QuickUnlockHandler() = default;

QuickUnlockHandler::~QuickUnlockHandler() = default;

void QuickUnlockHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "RequestPinLoginState",
      base::BindRepeating(&QuickUnlockHandler::HandleRequestPinLoginState,
                          base::Unretained(this)));
}

void QuickUnlockHandler::HandleRequestPinLoginState(
    const base::ListValue* args) {
  AllowJavascript();
  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&QuickUnlockHandler::OnPinLoginAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickUnlockHandler::OnPinLoginAvailable(bool is_available) {
  FireWebUIListener("pin-login-available-changed", base::Value(is_available));
}

}  // namespace settings
}  // namespace chromeos
