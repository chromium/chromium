// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(DesktopBrowserWindowCapabilities);

DesktopBrowserWindowCapabilities::DesktopBrowserWindowCapabilities(
    BrowserWindowModalDialogDelegate* modal_dialog_delegate,
    UnloadController* unload_controller,
    BrowserWindow* browser_window,
    ui::UnownedUserDataHost& host)
    : modal_dialog_delegate_(CHECK_DEREF(modal_dialog_delegate)),
      unload_controller_(CHECK_DEREF(unload_controller)),
      browser_window_(CHECK_DEREF(browser_window)),
      scoped_data_holder_(host, *this) {}

DesktopBrowserWindowCapabilities::~DesktopBrowserWindowCapabilities() = default;

DesktopBrowserWindowCapabilities* DesktopBrowserWindowCapabilities::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<DesktopBrowserWindowCapabilities>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

const DesktopBrowserWindowCapabilities* DesktopBrowserWindowCapabilities::From(
    const BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<DesktopBrowserWindowCapabilities>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

bool DesktopBrowserWindowCapabilities::IsVisibleOnScreen() const {
  return browser_window_->IsVisibleOnScreen();
}

bool DesktopBrowserWindowCapabilities::IsAttemptingToCloseBrowser() const {
  return unload_controller_->is_attempting_to_close_browser();
}

gfx::Size DesktopBrowserWindowCapabilities::GetContentsSize() const {
  return browser_window_->GetContentsSize();
}

void DesktopBrowserWindowCapabilities::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  return modal_dialog_delegate_->SetWebContentsBlocked(web_contents, blocked);
}

bool DesktopBrowserWindowCapabilities::AllowKeyboardLockForInnerContents(
    content::WebContents* web_contents) const {
  if (WebUIBrowserWindow::FromNativeWindow(
          browser_window_->GetNativeWindow())) {
    // Allow keyboard lock for tab WebContents in WebUIBrowserWindow.
    return tabs::TabInterface::MaybeGetFromContents(web_contents) != nullptr;
  }

  return false;
}
