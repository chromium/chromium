// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace {

class BrowserDesktopWindowTreeHostFuchsia
    : public BrowserDesktopWindowTreeHost,
      public views::DesktopWindowTreeHostPlatform {
 public:
  BrowserDesktopWindowTreeHostFuchsia(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame)
      : views::DesktopWindowTreeHostPlatform(native_widget_delegate,
                                             desktop_native_widget_aura) {
    // TODO(crbug.com/1234748): Implement ViewProvider and connect it here,
    // so that the DesktopWindowTreeHostPlatform can create an Ozone window
    // with the necessary Scenic View token, ViewRef, etc.
  }
  ~BrowserDesktopWindowTreeHostFuchsia() override = default;

  BrowserDesktopWindowTreeHostFuchsia(
      const BrowserDesktopWindowTreeHostFuchsia&) = delete;
  BrowserDesktopWindowTreeHostFuchsia& operator=(
      const BrowserDesktopWindowTreeHostFuchsia&) = delete;

 private:
  // Overridden from BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override { return this; }
  int GetMinimizeButtonOffset() const override { return 0; }
  bool UsesNativeSystemMenu() const override { return false; }
};

}  // namespace

BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostFuchsia(native_widget_delegate,
                                                 desktop_native_widget_aura,
                                                 browser_view, browser_frame);
}
