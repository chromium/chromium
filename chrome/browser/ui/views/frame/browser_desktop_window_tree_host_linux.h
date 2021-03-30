// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"  // nogncheck

#if defined(USE_DBUS_MENU)
#include "chrome/browser/ui/views/frame/dbus_appmenu.h"  // nogncheck
#endif

using DesktopWindowTreeHostLinuxImpl = views::DesktopWindowTreeHostLinux;

class BrowserFrame;
class BrowserView;
enum class TabDragKind;

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

  // Called when the tab drag status changes for this window.
  void TabDraggingKindChanged(TabDragKind tab_drag_kind);

 private:
  // BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // views::DesktopWindowTreeHostLinuxImpl:
  void Init(const views::Widget::InitParams& params) override;
  void CloseNow() override;

  // ui::X11ExtensionDelegate:
  bool IsOverrideRedirect(bool is_tiling_wm) const override;

  // ui::PlatformWindowDelegate
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;

  BrowserView* browser_view_ = nullptr;
  BrowserFrame* browser_frame_ = nullptr;

#if defined(USE_DBUS_MENU)
  // Each browser frame maintains its own menu bar object because the lower
  // level dbus protocol associates a xid to a menu bar; we can't map multiple
  // xids to the same menu bar.
  std::unique_ptr<DbusAppmenu> dbus_appmenu_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
