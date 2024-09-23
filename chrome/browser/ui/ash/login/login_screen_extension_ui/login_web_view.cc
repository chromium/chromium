// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_extension_ui/login_web_view.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/dialog_delegate.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {
namespace login_screen_extension_ui {

LoginWebView::LoginWebView(
    content::BrowserContext* context,
    login_screen_extension_ui::DialogDelegate* delegate,
    std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
        handler)
    : views::WebDialogView(context, delegate, std::move(handler)),
      delegate_(delegate) {
  views::WidgetDelegate::SetShowTitle(!delegate_ ||
                                      delegate_->ShouldCenterDialogTitleText());
  if (LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->AddSystemTrayObserver(this);
  }
}

LoginWebView::~LoginWebView() {
  if (LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->RemoveSystemTrayObserver(this);
  }
}

bool LoginWebView::TakeFocus(content::WebContents* source, bool reverse) {
  LoginScreen::Get()->FocusLoginShelf(reverse);
  return true;
}

void LoginWebView::OnFocusLeavingSystemTray(bool reverse) {
  web_contents()->FocusThroughTabTraversal(reverse);
  web_contents()->Focus();
}

BEGIN_METADATA(LoginWebView)
END_METADATA

}  // namespace login_screen_extension_ui
}  // namespace ash
