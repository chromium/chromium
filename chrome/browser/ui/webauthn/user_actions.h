// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_

#include <vector>

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_constants.h"

namespace webauthn::user_actions {

// Emits what authenticators are visible to the user in the WebAuthn selection
// dialog when there are multiple options are available. Targets only GPM,
// iCloud Keychain and Windows Hello authenticators.
void RecordMultipleOptionsShown(
    const std::vector<AuthenticatorRequestDialogModel::Mechanism>& mechanisms,
    device::FidoRequestType request_type);

// Emits what authenticator is displayed as the priority mechanism in the
// priority WebAuthn credential selector dialog.
void RecordPriorityOptionShown(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism);

void RecordHybridAndSecurityKeyDialogShown(
    device::FidoRequestType request_type);
void RecordSecurityKeyDialogShown(device::FidoRequestType request_type);

void RecordMechanismClick(
    const AuthenticatorRequestDialogModel::Mechanism& mech);

void RecordCancelClick();

void RecordAcceptClick();

void RecordTrustDialogShown(device::FidoRequestType request_type);

void RecordCreateGpmDialogShown();

void RecordRecoveryShown(device::FidoRequestType request_type);
void RecordRecoveryCancelled();
void RecordRecoverySucceeded();

void RecordICloudShown(device::FidoRequestType request_type);
void RecordICloudCancelled();
void RecordICloudSuccess();

void RecordGpmTouchIdDialogShown(device::FidoRequestType request_type);
// TODO(crbug.com/358277466): Add user action for cancelling MacOS password
// dialog.
void RecordGpmPinSheetShown(device::FidoRequestType request_type,
                            bool is_pin_creation,
                            bool is_arbitrary);
void RecordGpmForgotPinClick();
void RecordGpmPinOptionChangeClick();
void RecordGpmLockedShown();
void RecordGpmSuccess();
void RecordGpmFailureShown();
void RecordGpmWinUvShown(device::FidoRequestType request_type);
// TODO(crbug.com/358277466): Add user action for cancelling Windows Hello
// dialog.

void RecordChromeProfileAuthenticatorShown(
    device::FidoRequestType request_type);
void RecordChromeProfileCancelled();
void RecordChromeProfileSuccess();

void RecordWindowsHelloShown(device::FidoRequestType request_type);
void RecordWindowsHelloCancelled();
void RecordWindowsHelloSuccess();

void RecordContextMenuEntryClick();

}  // namespace webauthn::user_actions

#endif  // CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
