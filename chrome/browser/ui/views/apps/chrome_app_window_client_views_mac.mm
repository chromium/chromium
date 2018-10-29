// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_mac.h"

// static
extensions::NativeAppWindow* ChromeAppWindowClient::CreateNativeAppWindowImpl(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  ChromeNativeAppWindowViewsMac* window = new ChromeNativeAppWindowViewsMac;
  window->Init(app_window, params);
  return window;
}
