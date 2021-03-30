// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

class BrowserFrame;
class BrowserView;

namespace views {
class DesktopNativeWidgetAura;
}

// Platform independent frame implementation. Utilizes
// DesktopWindowTreeHostPlatform, which uses PlatformWindow with different
// backends like ozone and others.
class BrowserDesktopWindowTreeHostPlatform
    : public BrowserDesktopWindowTreeHost,
      public views::DesktopWindowTreeHostPlatform {
 public:
  BrowserDesktopWindowTreeHostPlatform(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  ~BrowserDesktopWindowTreeHostPlatform() override;

 private:
  // Overridden from BrowserDesktopWindowTreeHost:
  views::DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // Overridden from views::DesktopWindowTreeHostPlatform:
  bool ShouldUseLayerForShapedWindow() const override;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostPlatform);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
