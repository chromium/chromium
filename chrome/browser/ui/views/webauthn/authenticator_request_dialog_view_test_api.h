// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_

#include <memory>

class AuthenticatorRequestDialogModel;
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
      std::unique_ptr<AuthenticatorRequestDialogModel> dialog_model,
      content::WebContents* web_contents);

  // Replaces the current sheet on |dialog| with |new_sheet|.
  static void ShowWithSheet(
      AuthenticatorRequestDialogView* dialog,
      std::unique_ptr<AuthenticatorRequestSheetView> new_sheet);
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_TEST_API_H_
