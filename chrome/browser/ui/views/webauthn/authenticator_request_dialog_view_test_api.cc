// Copyright 2018 The Chromium Authors. All rights reserved.
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
    std::unique_ptr<AuthenticatorRequestDialogModel> dialog_model,
    content::WebContents* web_contents) {
  return new AuthenticatorRequestDialogView(web_contents,
                                            std::move(dialog_model));
}

// static
void AuthenticatorRequestDialogViewTestApi::ShowWithSheet(
    AuthenticatorRequestDialogView* dialog,
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  dialog->ReplaceCurrentSheetWith(std::move(new_sheet));
  dialog->Show();
}
}  // namespace test
