// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_SHEET_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_SHEET_VIEW_FACTORY_H_

#include <memory>

class AuthenticatorRequestSheetView;
struct AuthenticatorRequestDialogModel;

namespace autofill {
class WebauthnDialogModel;
}

// Creates the appropriate AuthenticatorRequestSheetView subclass instance,
// along with the appropriate AuthenticatorRequestSheetModel, for the current
// step of the |dialog_model|.
std::unique_ptr<AuthenticatorRequestSheetView> CreateSheetViewForCurrentStepOf(
    AuthenticatorRequestDialogModel* dialog_model);

// Creates the AuthenticatorRequestSheetView instance used by
// WebauthnDialogView.
std::unique_ptr<AuthenticatorRequestSheetView>
CreateSheetViewForAutofillWebAuthn(
    std::unique_ptr<autofill::WebauthnDialogModel> model);

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_SHEET_VIEW_FACTORY_H_
