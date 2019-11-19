// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContentsDelegate;
}

namespace views {
class WebView;
}

// Views implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they're managing
// closes (in the DeleteDelegate callback).
class SigninViewControllerDelegateViews
    : public views::DialogDelegateView,
      public SigninViewControllerDelegate,
      public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate {
 public:

  static std::unique_ptr<views::WebView> CreateSyncConfirmationWebView(
      Browser* browser);

  static std::unique_ptr<views::WebView> CreateSigninErrorWebView(
      Browser* browser);

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

 private:
  friend SigninViewControllerDelegate;

  // Creates and displays a constrained window containing |web_contents|. If
  // |wait_for_size| is true, the delegate will wait for ResizeNativeView() to
  // be called by the base class before displaying the constrained window.
  SigninViewControllerDelegateViews(
      SigninViewController* signin_view_controller,
      std::unique_ptr<views::WebView> content_view,
      Browser* browser,
      ui::ModalType dialog_modal_type,
      bool wait_for_size);
  ~SigninViewControllerDelegateViews() override;

  // Creates a WebView for a dialog with the specified URL.
  static std::unique_ptr<views::WebView> CreateDialogWebView(
      Browser* browser,
      const std::string& url,
      int dialog_height,
      base::Optional<int> dialog_width);

  // Notifies the SigninViewController that this instance is being deleted.
  void ResetSigninViewControllerDelegate();

  // Displays the modal dialog.
  void DisplayModal();

  Browser* browser() { return browser_; }

  SigninViewController* signin_view_controller_;  // Not owned.
  content::WebContents* const web_contents_;      // Not owned.
  Browser* const browser_;                        // Not owned.
  views::WebView* content_view_;
  views::Widget* modal_signin_widget_;  // Not owned.
  ui::ModalType dialog_modal_type_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
