// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"

#if defined(USE_X11)
#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"  // nogncheck
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"  // nogncheck
#else
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"  // nogncheck
#endif

// TODO(https://crbug.com/990756): Make sure correct
// DesktopWindowTreeHost is used while the DWTHX11 is being refactored and
// merged into the DWTHLinux and the DWTHPlatform. Non-Ozone X11 must use
// the DWTHX11 now, but Ozone must use DWTHLinux. Remove this guard once
// DWTHX11 is finally merged into DWTHPlatform and DWTHLinux.
#if defined(USE_X11)
using DesktopWindowTreeHostLinuxImpl = views::DesktopWindowTreeHostX11;
#else
using DesktopWindowTreeHostLinuxImpl = views::DesktopWindowTreeHostLinux;
#endif

class BrowserFrame;
class BrowserView;

namespace views {
class DesktopNativeWidgetAura;
}

class BrowserDesktopWindowTreeHostLinux
    : public BrowserDesktopWindowTreeHost,
      public DesktopWindowTreeHostLinuxImpl {
 public:
  BrowserDesktopWindowTreeHostLinux(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  ~BrowserDesktopWindowTreeHostLinux() override;

 private:
  // Overridden from BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // Overridden from views::DesktopWindowTreeHostLinuxImpl:
  void Init(const views::Widget::InitParams& params) override;
  void CloseNow() override;

#if defined(USE_X11)
  BrowserView* browser_view_ = nullptr;

  // Each browser frame maintains its own menu bar object because the lower
  // level dbus protocol associates a xid to a menu bar; we can't map multiple
  // xids to the same menu bar.
  std::unique_ptr<GlobalMenuBarX11> global_menu_bar_x11_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
