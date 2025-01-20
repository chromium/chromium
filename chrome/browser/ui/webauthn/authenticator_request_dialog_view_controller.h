// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_H_

#include <memory>

struct AuthenticatorRequestDialogModel;

namespace content {
class WebContents;
}

class AuthenticatorRequestDialogViewController {
 public:
  // Creates and shows the dialog for a given WebContents.
  // `model` is expected to own the created controller.
  static std::unique_ptr<AuthenticatorRequestDialogViewController> Create(
      content::WebContents* web_contents,
      AuthenticatorRequestDialogModel* model);

  virtual ~AuthenticatorRequestDialogViewController() = default;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_H_
