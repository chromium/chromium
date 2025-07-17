// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

AndroidBrowserWindow::AndroidBrowserWindow() = default;

AndroidBrowserWindow::~AndroidBrowserWindow() = default;

ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost() {
  NOTREACHED();
}

const ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost()
    const {
  NOTREACHED();
}

ui::BaseWindow* AndroidBrowserWindow::GetWindow() {
  NOTREACHED();
}

Profile* AndroidBrowserWindow::GetProfile() {
  NOTREACHED();
}

const SessionID& AndroidBrowserWindow::GetSessionID() const {
  NOTREACHED();
}

content::WebContents* AndroidBrowserWindow::OpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  NOTREACHED();
}
