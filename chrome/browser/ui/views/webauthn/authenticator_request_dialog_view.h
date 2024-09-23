// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace test {
class AuthenticatorRequestDialogViewTestApi;
}

class AuthenticatorRequestSheetView;

// A tab-modal dialog shown while a Web Authentication API request is active.
//
// This UI first allows the user the select the transport protocol they wish to
// use to connect their security key (either USB, BLE, NFC, or internal), and
// then guides them through the flow of setting up their security key using the
// selecting transport protocol, and finally shows success/failure indications.
//
// The dialog view may be destroyed and recreated multiple times during the
// lifetime of a single WebAuthn request.
//
// Note that as a DialogDelegateView, AuthenticatorRequestDialogView is
// eventually deleted when DeleteDelegate() is called.
class AuthenticatorRequestDialogView
    : public views::DialogDelegateView,
      public AuthenticatorRequestDialogModel::Observer,
      public content::WebContentsObserver {
  METADATA_HEADER(AuthenticatorRequestDialogView, views::DialogDelegateView)

 public:
  AuthenticatorRequestDialogView(const AuthenticatorRequestDialogView&) =
      delete;
  AuthenticatorRequestDialogView& operator=(
      const AuthenticatorRequestDialogView&) = delete;
  ~AuthenticatorRequestDialogView() override;

 protected:
  // Replaces the |sheet_| currently being shown in the dialog with |new_sheet|,
  // destroying the old sheet.
  void ReplaceCurrentSheetWith(
      std::unique_ptr<AuthenticatorRequestSheetView> new_sheet);

  // Triggers updating the contents of the current sheet view, plus state of the
  // buttons on the dialog, the accessibility window title (using the data
  // provided by the new sheet), and the dialog size and position.
  void UpdateUIForCurrentSheet();

  // Returns whether the "Choose another option" button show be shown based on
  // whether the current sheet model defines a model for the other transports
  // popup menu, and whether it has at least one element.
  bool ShouldOtherMechanismsButtonBeVisible() const;

  AuthenticatorRequestSheetView* sheet() const {
    DCHECK(sheet_);
    return sheet_;
  }

  // views::View:
  void AddedToWidget() override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;
  void OnStepTransition() override;
  void OnSheetModelChanged() override;
  void OnButtonsStateChanged() override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class test::AuthenticatorRequestDialogViewTestApi;
  friend void ShowAuthenticatorRequestDialog(
      content::WebContents* web_contents,
      scoped_refptr<AuthenticatorRequestDialogModel> model);

  // Show by calling ShowAuthenticatorRequestDialog().
  AuthenticatorRequestDialogView(
      content::WebContents* web_contents,
      scoped_refptr<AuthenticatorRequestDialogModel> model);

  // Shows the dialog after creation or after being hidden.
  void Show();

  void OtherMechanismsButtonPressed();
  void ManageDevicesButtonPressed();
  void ForgotGPMPinPressed();
  void GPMPinOptionChosen(bool is_arbitrary);
  void UpdateFooter();

  scoped_refptr<AuthenticatorRequestDialogModel> model_;

  raw_ptr<AuthenticatorRequestSheetView, DanglingUntriaged> sheet_ = nullptr;
  std::unique_ptr<views::MenuRunner> other_mechanisms_menu_runner_;
  bool first_shown_ = false;

  // web_contents_hidden_ is true if the |WebContents| that this dialog should
  // attach to is currently hidden. In this case, the dialog won't be shown
  // when requested, but will wait until the WebContents is visible again.
  bool web_contents_hidden_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
