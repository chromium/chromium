// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"

// static
extensions::NativeAppWindow* ChromeAppWindowClient::CreateNativeAppWindowImpl(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  ChromeNativeAppWindowViewsWin* window = new ChromeNativeAppWindowViewsWin;
  window->Init(app_window, params);
  return window;
}
