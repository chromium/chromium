// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

// static
const char* DesktopBrowserWindowCapabilities::kDataKey =
    "DesktopBrowserWindowCapabilities";

DesktopBrowserWindowCapabilities::DesktopBrowserWindowCapabilities(
    BrowserWindow* browser_window,
    UnownedUserDataHost& host)
    : browser_window_(browser_window), scoped_data_holder_(host, this) {}

DesktopBrowserWindowCapabilities::~DesktopBrowserWindowCapabilities() = default;

DesktopBrowserWindowCapabilities* DesktopBrowserWindowCapabilities::From(
    BrowserWindowInterface* browser_window_interface) {
  return ScopedUnownedUserData<DesktopBrowserWindowCapabilities>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

const DesktopBrowserWindowCapabilities* DesktopBrowserWindowCapabilities::From(
    const BrowserWindowInterface* browser_window_interface) {
  return ScopedUnownedUserData<DesktopBrowserWindowCapabilities>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

bool DesktopBrowserWindowCapabilities::IsVisibleOnScreen() const {
  return browser_window_->IsVisibleOnScreen();
}
