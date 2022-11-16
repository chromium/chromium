// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/window/native_frame_view_mac.h"

namespace extensions {
class NativeAppWindow;
}

class Widget;

// Provides metrics consistent with a native frame on Mac. The actual frame is
// drawn by NSWindow.
class NativeAppWindowFrameViewMac : public views::NativeFrameViewMac {
 public:
  NativeAppWindowFrameViewMac(views::Widget* frame,
                              extensions::NativeAppWindow* window);

  NativeAppWindowFrameViewMac(const NativeAppWindowFrameViewMac&) = delete;
  NativeAppWindowFrameViewMac& operator=(const NativeAppWindowFrameViewMac&) =
      delete;

  ~NativeAppWindowFrameViewMac() override;

  // NonClientFrameView:
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  // Weak. Owned by extensions::AppWindow (which manages our Widget via its
  // WebContents).
  raw_ptr<extensions::NativeAppWindow, DanglingUntriaged> native_app_window_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
