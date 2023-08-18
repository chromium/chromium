// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_modal_dialog_impl.h"

SigninModalDialogImpl::SigninModalDialogImpl(
    SigninViewControllerDelegate* delegate,
    base::OnceClosure on_close_callback)
    : SigninModalDialog(std::move(on_close_callback)), delegate_(delegate) {
  delegate_observation_.Observe(delegate_);
}

SigninModalDialogImpl::~SigninModalDialogImpl() = default;

void SigninModalDialogImpl::CloseModalDialog() {
  delegate_->CloseModalSignin();
}

void SigninModalDialogImpl::ResizeNativeView(int height) {
  delegate_->ResizeNativeView(height);
}

content::WebContents*
SigninModalDialogImpl::GetModalDialogWebContentsForTesting() {
  return delegate_->GetWebContents();
}

void SigninModalDialogImpl::OnModalDialogClosed() {
  NotifyModalDialogClosed();
}
