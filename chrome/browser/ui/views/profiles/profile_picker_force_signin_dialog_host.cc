// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"

#include "chrome/browser/profiles/profile.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

ProfilePickerForceSigninDialogHost::ProfilePickerForceSigninDialogHost() =
    default;

void ProfilePickerForceSigninDialogHost::ShowDialog(Profile* profile,
                                                    const GURL& url,
                                                    gfx::NativeView parent) {
  HideDialog();
  auto delegate = std::make_unique<ProfilePickerForceSigninDialogDelegate>(
      this, std::make_unique<views::WebView>(profile), url);
  delegate_ = delegate.get();
  views::DialogDelegate::CreateDialogWidget(std::move(delegate), nullptr,
                                            parent);
  delegate_->GetWidget()->Show();
}

void ProfilePickerForceSigninDialogHost::HideDialog() {
  if (delegate_) {
    delegate_->CloseDialog();
    DCHECK(!delegate_);
  }
}

void ProfilePickerForceSigninDialogHost::OnDialogDestroyed() {
  delegate_ = nullptr;
}

void ProfilePickerForceSigninDialogHost::DisplayErrorMessage() {
  if (delegate_)
    delegate_->DisplayErrorMessage();
}

views::DialogDelegateView*
ProfilePickerForceSigninDialogHost::GetDialogDelegateViewForTesting() const {
  return delegate_;
}
