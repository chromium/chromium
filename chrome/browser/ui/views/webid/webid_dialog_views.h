// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_DIALOG_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webid/webid_dialog.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}  // namespace content

// Basic bubble dialog that is used in the WebID flow.
//
// It creates a dialog and changes the content of that dialog as user moves
// through the WebID flow steps.
class WebIdDialogViews : public WebIdDialog,
                         public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(WebIdDialogViews);
  // Constructs a new dialog. The actual dialog widget will be modal to the
  // |rp_web_contents| and is shown using the
  // |constrained_window::ShowWebModalDialogViews| machinery.
  WebIdDialogViews(content::WebContents* rp_web_contents, CloseCallback);
  // Constructs a new dialog. The actual dialog widget gets to be modal to the
  // |parent| window. This bypasses constrained_window machinery making
  // it easier to test.
  WebIdDialogViews(content::WebContents* rp_web_contents,
                   gfx::NativeView parent,
                   CloseCallback);
  WebIdDialogViews(const WebIdDialogViews&) = delete;
  WebIdDialogViews operator=(const WebIdDialogViews&) = delete;
  ~WebIdDialogViews() override;

  void ShowInitialPermission(const std::u16string& idp_hostname,
                             const std::u16string& rp_hostname,
                             PermissionDialogMode mode,
                             PermissionCallback) override;
  void ShowTokenExchangePermission(const std::u16string& idp_hostname,
                                   const std::u16string& rp_hostname,
                                   PermissionCallback) override;
  void ShowSigninPage(content::WebContents* idp_web_contents,
                      const GURL& idp_signin_url) override;
  void CloseSigninPage() override;

 private:
  // Shows the dialog and creates it if necessary.
  void ShowDialog();
  // Changes the content view of the dialog.
  void SetContent(std::unique_ptr<views::View> content);

  void OnClose();
  bool Accept() override;
  bool Cancel() override;

  enum class State {
    kUninitialized,
    kInitialPermission,
    kSignIn,
    kTokenExchangePermission
  };
  // A simple state machine to keep track of where in flow we are.
  State state_{State::kUninitialized};

  PermissionCallback permission_callback_;

  // |close_callback_| is called when the dialog is closed by user action and
  // |permission_callback_| is nullptr. This corresponds to a cancellation
  // while no permission is being requested, in particular because the sign-in
  // page has been or is being loaded.
  CloseCallback close_callback_;

  // Modifies behavior based on whether the caller expects state to carry
  // through multiple calls or whether the dialog should close at the end
  // of a single invocation.
  PermissionDialogMode permission_dialog_mode_ =
      PermissionDialogMode::kStateful;

  // Dialog widget that shows the content. It is created and shown on the first
  // step. It remains shown until user reaches the end of the flow or explicitly
  // closes it.
  raw_ptr<views::Widget> dialog_{nullptr};
  // The content that is currently shown.
  raw_ptr<views::View> content_{nullptr};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_DIALOG_VIEWS_H_
