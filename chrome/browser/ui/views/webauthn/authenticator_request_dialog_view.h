// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/metadata/metadata_header_macros.h"
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
// Note that as a DialogDelegateView, AuthenticatorRequestDialogView is
// deleted when DeleteDelegate() is called.
class AuthenticatorRequestDialogView
    : public views::DialogDelegateView,
      public AuthenticatorRequestDialogModel::Observer,
      public content::WebContentsObserver {
 public:
  METADATA_HEADER(AuthenticatorRequestDialogView);
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

  // Shows or hides the "Choose another option" button based on whether the
  // current sheet model defines a model for the other transports popup menu,
  // and whether it has at least one element.
  void ToggleOtherTransportsButtonVisibility();
  bool ShouldOtherTransportsButtonBeVisible() const;

  AuthenticatorRequestSheetView* sheet() const {
    DCHECK(sheet_);
    return sheet_;
  }

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;
  void OnStepTransition() override;
  void OnSheetModelChanged() override;

  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class test::AuthenticatorRequestDialogViewTestApi;
  friend void ShowAuthenticatorRequestDialog(
      content::WebContents* web_contents,
      std::unique_ptr<AuthenticatorRequestDialogModel> model);

  // Show by calling ShowAuthenticatorRequestDialog().
  AuthenticatorRequestDialogView(
      content::WebContents* web_contents,
      std::unique_ptr<AuthenticatorRequestDialogModel> model);

  // Shows the dialog after creation or after being hidden.
  void Show();

  void OtherTransportsButtonPressed();

  void OnDialogClosing();

  std::unique_ptr<AuthenticatorRequestDialogModel> model_;

  AuthenticatorRequestSheetView* sheet_ = nullptr;
  views::View* other_transports_button_ = nullptr;
  std::unique_ptr<views::MenuRunner> other_transports_menu_runner_;
  bool first_shown_ = false;

  // web_contents_hidden_ is true if the |WebContents| that this dialog should
  // attach to is currently hidden. In this case, the dialog won't be shown
  // when requested, but will wait until the WebContents is visible again.
  bool web_contents_hidden_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
