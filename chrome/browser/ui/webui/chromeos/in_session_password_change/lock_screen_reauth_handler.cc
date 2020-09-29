// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"

namespace chromeos {

LockScreenReauthHandler::LockScreenReauthHandler() = default;
LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleInitialize(const base::ListValue* value) {}

void LockScreenReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&LockScreenReauthHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos