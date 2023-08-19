// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_IMPL_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"

// Signin modal dialog that hosts a webUI in a native modal view.
// Delegates actual work to SigninViewControllerDelegate.
class SigninModalDialogImpl : public SigninModalDialog,
                              public SigninViewControllerDelegate::Observer {
 public:
  explicit SigninModalDialogImpl(SigninViewControllerDelegate* delegate,
                                 base::OnceClosure on_close_callback);

  ~SigninModalDialogImpl() override;

  // SigninModalDialog:
  void CloseModalDialog() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetModalDialogWebContentsForTesting() override;

  // SigninViewControllerDelegate::Observer:
  void OnModalDialogClosed() override;

 private:
  raw_ptr<SigninViewControllerDelegate> delegate_;
  base::ScopedObservation<SigninViewControllerDelegate,
                          SigninViewControllerDelegate::Observer>
      delegate_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_MODAL_DIALOG_IMPL_H_
