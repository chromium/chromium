// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_X11_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

class BrowserFrame;
class BrowserView;

namespace views {
class DesktopNativeWidgetAura;
}

class BrowserDesktopWindowTreeHostX11
    : public BrowserDesktopWindowTreeHost,
      public views::DesktopWindowTreeHostX11 {
 public:
  BrowserDesktopWindowTreeHostX11(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  ~BrowserDesktopWindowTreeHostX11() override;

 private:
  // Overridden from BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // Overridden from views::DesktopWindowTreeHostX11:
  void Init(const views::Widget::InitParams& params) override;
  void CloseNow() override;

  BrowserView* browser_view_;

  // Each browser frame maintains its own menu bar object because the lower
  // level dbus protocol associates a xid to a menu bar; we can't map multiple
  // xids to the same menu bar.
  std::unique_ptr<GlobalMenuBarX11> global_menu_bar_x11_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_X11_H_
