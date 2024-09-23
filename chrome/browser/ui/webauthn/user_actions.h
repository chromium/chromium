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

void RecordMechanismClick(
    const AuthenticatorRequestDialogModel::Mechanism& mech);

void RecordCancelClick();

void RecordAcceptClick();

void RecordTrustDialogShown(bool is_create);

void RecordCreateGpmDialogShown();

void RecordRecoveryShown(bool is_create);
void RecordRecoveryCancelled();
void RecordRecoverySucceeded();

void RecordICloudShown(bool is_create);
void RecordICloudCancelled();
void RecordICloudSuccess();

void RecordGpmTouchIdDialogShown(bool is_create);
// TODO(crbug.com/358277466): Add user action for cancelling MacOS password
// dialog.
void RecordGpmPinSheetShown(bool is_credential_creation,
                            bool is_pin_creation,
                            bool is_arbitrary);
void RecordGpmForgotPinClick();
void RecordGpmPinOptionChangeClick();
void RecordGpmLockedShown();
void RecordGpmSuccess();
void RecordGpmFailureShown();
void RecordGpmWinUvShown(bool is_create);
// TODO(crbug.com/358277466): Add user action for cancelling Windows Hello
// dialog.

void RecordChromeProfileAuthenticatorShown(bool is_create);
void RecordChromeProfileCancelled();
void RecordChromeProfileSuccess();

void RecordWindowsHelloShown(bool is_create);
void RecordWindowsHelloCancelled();
void RecordWindowsHelloSuccess();

}  // namespace webauthn::user_actions

#endif  // CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
