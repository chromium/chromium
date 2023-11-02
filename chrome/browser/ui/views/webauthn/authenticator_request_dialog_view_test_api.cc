// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "components/constrained_window/constrained_window_views.h"

namespace test {

// static
AuthenticatorRequestDialogView*
AuthenticatorRequestDialogViewTestApi::CreateDialogView(
    content::WebContents* web_contents,
    AuthenticatorRequestDialogModel* dialog_model) {
  return new AuthenticatorRequestDialogView(web_contents, dialog_model);
}

// static
void AuthenticatorRequestDialogViewTestApi::ShowWithSheet(
    AuthenticatorRequestDialogView* dialog,
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  dialog->ReplaceCurrentSheetWith(std::move(new_sheet));
  dialog->Show();
}

// static
AuthenticatorRequestSheetView* AuthenticatorRequestDialogViewTestApi::GetSheet(
    AuthenticatorRequestDialogView* dialog) {
  return dialog->sheet_;
}

}  // namespace test
