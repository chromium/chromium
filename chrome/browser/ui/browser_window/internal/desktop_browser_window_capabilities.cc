// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities_delegate.h"

DEFINE_USER_DATA(DesktopBrowserWindowCapabilities);

DesktopBrowserWindowCapabilities::DesktopBrowserWindowCapabilities(
    DesktopBrowserWindowCapabilitiesDelegate* delegate,
    BrowserWindow* browser_window,
    ui::UnownedUserDataHost& host)
    : delegate_(delegate),
      browser_window_(browser_window),
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
  return delegate_->IsAttemptingToCloseBrowser();
}

void DesktopBrowserWindowCapabilities::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  return delegate_->SetWebContentsBlocked(web_contents, blocked);
}
