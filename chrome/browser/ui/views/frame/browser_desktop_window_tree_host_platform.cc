// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_platform.h"

////////////////////////////////////////////////////////////////////////////////
//// BrowserDesktopWindowTreeHostPlatform, public:

BrowserDesktopWindowTreeHostPlatform::BrowserDesktopWindowTreeHostPlatform(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {}

BrowserDesktopWindowTreeHostPlatform::~BrowserDesktopWindowTreeHostPlatform() {}

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostPlatform::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostPlatform::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostPlatform::UsesNativeSystemMenu() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
//// DesktopWindowTreeHostPlatform, private:
bool BrowserDesktopWindowTreeHostPlatform::ShouldUseLayerForShapedWindow()
    const {
  return false;
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
  return new BrowserDesktopWindowTreeHostPlatform(native_widget_delegate,
                                                  desktop_native_widget_aura,
                                                  browser_view, browser_frame);
}
