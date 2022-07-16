// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_lacros.h"

#include "chromeos/ui/base/window_properties.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLacros, public:

BrowserDesktopWindowTreeHostLacros::BrowserDesktopWindowTreeHostLacros(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : BrowserDesktopWindowTreeHostLinux(native_widget_delegate,
                                        desktop_native_widget_aura,
                                        browser_view,
                                        browser_frame),
      desktop_native_widget_aura_(desktop_native_widget_aura) {
  // Lacros receives occlusion information from exo via aura-shell.
  SetNativeWindowOcclusionEnabled(true);
}

BrowserDesktopWindowTreeHostLacros::~BrowserDesktopWindowTreeHostLacros() =
    default;

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLacros,
//     DesktopWindowTreeHostPlatform implementation:

SkPath BrowserDesktopWindowTreeHostLacros::GetWindowMaskForClipping() const {
  // Lacros doesn't need to request clipping since it is already
  // done in views, so returns empty window mask.
  return SkPath();
}

void BrowserDesktopWindowTreeHostLacros::OnSurfaceFrameLockingChanged(
    bool lock) {
  aura::Window* window = desktop_native_widget_aura_->GetNativeWindow();
  DCHECK(window);
  window->SetProperty(chromeos::kFrameRestoreLookKey, lock);
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
  return new BrowserDesktopWindowTreeHostLacros(native_widget_delegate,
                                                desktop_native_widget_aura,
                                                browser_view, browser_frame);
}
