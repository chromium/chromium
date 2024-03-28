// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_

#include <memory>

struct AuthenticatorRequestDialogModel;
class AuthenticatorRequestDialogView;
class AuthenticatorRequestSheetView;

namespace content {
class WebContents;
}

namespace test {

class AuthenticatorRequestDialogViewTestApi {
 public:
  // Returns a non-owning pointer to an AuthenticatorRequestDialogView for
  // testing.
  static AuthenticatorRequestDialogView* CreateDialogView(
      content::WebContents* web_contents,
      AuthenticatorRequestDialogModel* dialog_model);

  // Replaces the current sheet on |dialog| with |new_sheet|.
  static void ShowWithSheet(
      AuthenticatorRequestDialogView* dialog,
      std::unique_ptr<AuthenticatorRequestSheetView> new_sheet);

  // Get a non-owning pointer to the current sheet.
  static AuthenticatorRequestSheetView* GetSheet(
      AuthenticatorRequestDialogView* dialog);
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_
