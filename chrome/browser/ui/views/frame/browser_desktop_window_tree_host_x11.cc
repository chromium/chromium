// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_x11.h"

#include <utility>

#include "base/macros.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11, public:

BrowserDesktopWindowTreeHostX11::BrowserDesktopWindowTreeHostX11(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostX11(native_widget_delegate,
                               desktop_native_widget_aura),
      browser_view_(browser_view) {
  browser_frame->set_frame_type(
      browser_frame->UseCustomFrame() ? views::Widget::FRAME_TYPE_FORCE_CUSTOM
                                      : views::Widget::FRAME_TYPE_FORCE_NATIVE);
}

BrowserDesktopWindowTreeHostX11::~BrowserDesktopWindowTreeHostX11() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
    BrowserDesktopWindowTreeHostX11::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostX11::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostX11::UsesNativeSystemMenu() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11,
//     views::DesktopWindowTreeHostX11 implementation:

void BrowserDesktopWindowTreeHostX11::Init(
    const views::Widget::InitParams& params) {
  views::DesktopWindowTreeHostX11::Init(params);

  // We have now created our backing X11 window. We now need to (possibly)
  // alert Unity that there's a menu bar attached to it.
  global_menu_bar_x11_.reset(new GlobalMenuBarX11(browser_view_, this));
}

void BrowserDesktopWindowTreeHostX11::CloseNow() {
  global_menu_bar_x11_.reset();
  DesktopWindowTreeHostX11::CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
    BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostX11(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             browser_view,
                                             browser_frame);
}
