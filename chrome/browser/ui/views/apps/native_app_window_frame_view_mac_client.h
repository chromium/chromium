// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_CLIENT_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/views/window/native_frame_view_mac.h"

namespace extensions {
class NativeAppWindow;
}

namespace views {
class Widget;
}

// Client that provides app-specific behavior for NativeFrameViewMac, such as
// custom draggable regions for frameless app windows.
class NativeAppWindowFrameViewMacClient
    : public views::NativeFrameViewMacClient {
 public:
  NativeAppWindowFrameViewMacClient(views::Widget* widget,
                                    extensions::NativeAppWindow* window);
  ~NativeAppWindowFrameViewMacClient() override;

  NativeAppWindowFrameViewMacClient(const NativeAppWindowFrameViewMacClient&) =
      delete;
  NativeAppWindowFrameViewMacClient& operator=(
      const NativeAppWindowFrameViewMacClient&) = delete;

  // views::NativeFrameViewMacClient:
  std::optional<int> NonClientHitTest(const gfx::Point& point) override;

 private:
  const raw_ptr<views::Widget> widget_;
  // Weak. Owned by extensions::AppWindow (which manages our Widget via its
  // WebContents).
  const raw_ptr<extensions::NativeAppWindow, DanglingUntriaged>
      native_app_window_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_CLIENT_H_
