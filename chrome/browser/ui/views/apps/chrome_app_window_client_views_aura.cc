// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include <memory>

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"

// static
extensions::NativeAppWindow* ChromeAppWindowClient::CreateNativeAppWindowImpl(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  auto window = std::make_unique<ChromeNativeAppWindowViewsAura>();
  window->Init(app_window, params);
  return window.release();
}
