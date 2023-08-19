// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_H_

#include "base/functional/callback.h"

namespace content {
class WebContents;
}

// Base class for a signin modal dialog.
// SigninModalDialogImpl contains the default implementation that delegates
// all work to SigninViewControllerDelegate.
// Individual dialogs can extend this class to add some platform-agnostic logic.
class SigninModalDialog {
 public:
  explicit SigninModalDialog(base::OnceClosure on_close_callback);

  SigninModalDialog(const SigninModalDialog&) = delete;
  SigninModalDialog& operator=(const SigninModalDialog&) = delete;

  virtual ~SigninModalDialog();

  // Closes the sign-in dialog. Note that this method may trigger destruction
  // of this object, so the caller should no longer use this object after
  // calling this method.
  virtual void CloseModalDialog() = 0;

  // Requests a resize of the native view hosting the web contents. `height` is
  // the total height of the content, in pixels.
  virtual void ResizeNativeView(int height) = 0;

  // Returns the web contents of the modal dialog for testing.
  virtual content::WebContents* GetModalDialogWebContentsForTesting() = 0;

 protected:
  // Calls `on_close_callback_` to notify that the dialog has been closed. Must
  // be called exactly once per dialog's lifetime. The dialog may be destroyed
  // after this call.
  void NotifyModalDialogClosed();

 private:
  base::OnceClosure on_close_callback_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_H_
