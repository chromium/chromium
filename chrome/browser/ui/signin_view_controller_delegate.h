// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_

#include "build/build_config.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class SigninViewController;

namespace signin_metrics {
enum class AccessPoint;
}

// Abstract base class to the platform-specific managers of the Signin and Sync
// confirmation tab-modal dialogs. This and its platform-specific
// implementations are responsible for actually creating and owning the dialogs,
// as well as managing the navigation inside them.
// Subclasses are responsible for deleting themselves when the window they're
// managing closes.
class SigninViewControllerDelegate
    : public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate {
 public:
  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the sign in flow. The returned object should delete itself when
  // the window it's managing is closed.
  static SigninViewControllerDelegate* CreateModalSigninDelegate(
      SigninViewController* signin_view_controller,
      profiles::BubbleViewMode mode,
      Browser* browser,
      signin_metrics::AccessPoint access_point);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the sync confirmation dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSyncConfirmationDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser,
      bool is_consent_bump = false);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the modal sign in error dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSigninErrorDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser);

  // Attaches a dialog manager to this sign-in view controller dialog.
  // Should be called by subclasses when a different dialog may need to be
  // presented on top of the sign-in dialog.
  void AttachDialogManager();

  // Closes the sign-in dialog. Note that this method may destroy this object,
  // so the caller should no longer use this object after calling this method.
  void CloseModalSignin();

  // Either navigates back in the signin flow if the history state allows it or
  // closes the flow otherwise. Note that if view is closed, this method may
  // destroy this object, so the caller should no longer use this object after
  // calling this method.
  void PerformNavigation();

  // This will be called by the base class to request a resize of the native
  // view hosting the content to |height|. |height| is the total height of the
  // content, in pixels.
  virtual void ResizeNativeView(int height) = 0;

  // content::WebContentsDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // WebContents is used for executing javascript in the context of a modal sync
  // confirmation dialog.
  content::WebContents* web_contents() { return web_contents_; }

 protected:
  SigninViewControllerDelegate(SigninViewController* signin_view_controller,
                               content::WebContents* web_contents,
                               Browser* browser);
  ~SigninViewControllerDelegate() override;

  Browser* browser() { return browser_; }

  // Notifies the SigninViewController that this instance is being deleted.
  void ResetSigninViewControllerDelegate();

  // content::WebContentsDelegate
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;

  // Subclasses must override this method to correctly handle accelerators.
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // This will be called by this base class when the tab-modal window must be
  // closed. This should close the platform-specific window that is currently
  // showing the sign in flow or the sync confirmation dialog. Note that this
  // method may destroy this object, so the caller should no longer use this
  // object after calling this method.
  virtual void PerformClose() = 0;

 private:
  bool CanGoBack(content::WebContents* web_ui_web_contents) const;

  SigninViewController* signin_view_controller_;  // Not owned.
  content::WebContents* const web_contents_;      // Not owned.
  Browser* const browser_;                        // Not owned.

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegate);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
