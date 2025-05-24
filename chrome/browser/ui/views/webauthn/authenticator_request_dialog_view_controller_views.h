// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog_view_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

class AuthenticatorRequestDialogView;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

class AuthenticatorRequestDialogViewControllerViews
    : public AuthenticatorRequestDialogViewController,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorRequestDialogViewControllerViews(
      content::WebContents* web_contents,
      AuthenticatorRequestDialogModel* model);
  ~AuthenticatorRequestDialogViewControllerViews() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override;
  void OnSheetModelChanged() override;
  void OnButtonsStateChanged() override;

  AuthenticatorRequestDialogView* view_for_test() { return view_; }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;  // Owns us.
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AuthenticatorRequestDialogView> view_;  // Owned by `widget_`.
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_CONTROLLER_VIEWS_H_
