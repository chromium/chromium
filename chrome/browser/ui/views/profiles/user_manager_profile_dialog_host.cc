// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_host.h"

#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

UserManagerProfileDialogHost::UserManagerProfileDialogHost() = default;

void UserManagerProfileDialogHost::ShowDialog(
    content::BrowserContext* browser_context,
    const GURL& url,
    const base::FilePath& profile_path,
    gfx::NativeView parent) {
  HideDialog();
  force_signin_profile_path_ = profile_path;
  auto delegate = std::make_unique<UserManagerProfileDialogDelegate>(
      this, std::make_unique<views::WebView>(browser_context), url);
  delegate_ = delegate.get();
  views::DialogDelegate::CreateDialogWidget(std::move(delegate), nullptr,
                                            parent);
  delegate_->GetWidget()->Show();
}

void UserManagerProfileDialogHost::HideDialog() {
  if (delegate_) {
    delegate_->CloseDialog();
    DCHECK(!delegate_);
  }
  force_signin_profile_path_.clear();
}

base::FilePath UserManagerProfileDialogHost::GetForceSigninProfilePath() {
  return force_signin_profile_path_;
}

void UserManagerProfileDialogHost::OnDialogDestroyed() {
  delegate_ = nullptr;
  force_signin_profile_path_.clear();
}

void UserManagerProfileDialogHost::DisplayErrorMessage() {
  if (delegate_)
    delegate_->DisplayErrorMessage();
}
