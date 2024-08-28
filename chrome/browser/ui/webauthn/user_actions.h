// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_

#include <vector>

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

namespace webauthn::user_actions {

// Emits what authenticators are visible to the user in the WebAuthn selection
// dialog when there are multiple options are available. Targets only GPM,
// iCloud Keychain and Windows Hello authenticators.
void RecordMultipleOptionsShown(
    const std::vector<AuthenticatorRequestDialogModel::Mechanism>& mechanisms,
    bool is_create);

// Emits what authenticator is displayed as the priority mechanism in the
// priority WebAuthn credential selector dialog.
void RecordPriorityOptionShown(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism);

void RecordCancelClick();

void RecordICloudShown(bool is_create);

void RecordICloudCancelled();

void RecordICloudSuccess();

}  // namespace webauthn::user_actions

#endif  // CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
