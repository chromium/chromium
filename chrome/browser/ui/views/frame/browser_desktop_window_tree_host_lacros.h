// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LACROS_H_

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

namespace views {
class DesktopNativeWidgetAura;
}

class BrowserDesktopWindowTreeHostLacros
    : public BrowserDesktopWindowTreeHostLinux {
 public:
  BrowserDesktopWindowTreeHostLacros(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  BrowserDesktopWindowTreeHostLacros(
      const BrowserDesktopWindowTreeHostLacros&) = delete;
  BrowserDesktopWindowTreeHostLacros& operator=(
      const BrowserDesktopWindowTreeHostLacros&) = delete;
  ~BrowserDesktopWindowTreeHostLacros() override;

 private:
  // views::DesktopWindowTreeHostPlatform:
  bool ShouldUseLayerForShapedWindow() const override;
  void OnSurfaceFrameLockingChanged(bool lock) override;

  views::DesktopNativeWidgetAura* desktop_native_widget_aura_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
