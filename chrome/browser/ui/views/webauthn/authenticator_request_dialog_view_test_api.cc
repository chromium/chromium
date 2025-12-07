// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_controller_views.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

namespace test {

// static
void AuthenticatorRequestDialogViewTestApi::SetSheetTo(
    AuthenticatorRequestDialogViewControllerViews* controller,
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  AuthenticatorRequestDialogView* view = GetView(controller);
  view->ReplaceCurrentSheetWith(std::move(new_sheet));
  view->Show();
}

// static
AuthenticatorRequestSheetView* AuthenticatorRequestDialogViewTestApi::GetSheet(
    AuthenticatorRequestDialogViewControllerViews* controller) {
  return GetView(controller)->sheet_;
}

// static
AuthenticatorRequestDialogView* AuthenticatorRequestDialogViewTestApi::GetView(
    AuthenticatorRequestDialogViewControllerViews* controller) {
  CHECK(controller);
  AuthenticatorRequestDialogView* view =
      controller->view_for_test();  // IN-TEST
  CHECK(view);
  return view;
}

}  // namespace test
