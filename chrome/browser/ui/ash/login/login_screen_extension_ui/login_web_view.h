// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_LOGIN_WEB_VIEW_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_LOGIN_WEB_VIEW_H_

#include <memory>

#include "ash/system/tray/system_tray_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ash {
namespace login_screen_extension_ui {
class DialogDelegate;

// A WebDialogView subclass used by chrome.loginScreenUi API calls. It hides
// the close button if `DialogDelegate::CanCloseDialog()` is false.
class LoginWebView : public views::WebDialogView, public SystemTrayObserver {
  METADATA_HEADER(LoginWebView, views::WebDialogView)

 public:
  explicit LoginWebView(
      content::BrowserContext* context,
      login_screen_extension_ui::DialogDelegate* delegate,
      std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
          handler);
  LoginWebView(const LoginWebView&) = delete;
  LoginWebView& operator=(const LoginWebView&) = delete;
  ~LoginWebView() override;

  // views::WebDialogView
  bool TakeFocus(content::WebContents* source, bool reverse) override;

  // SystemTrayObserver
  void OnFocusLeavingSystemTray(bool reverse) override;

 private:
  // views::WebDialogView extends views::DialogDelegate, so fully qualified name
  // is needed.
  raw_ptr<login_screen_extension_ui::DialogDelegate, DanglingUntriaged>
      delegate_ = nullptr;
};

}  // namespace login_screen_extension_ui
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_LOGIN_WEB_VIEW_H_
