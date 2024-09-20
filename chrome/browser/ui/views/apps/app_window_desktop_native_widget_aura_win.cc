// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_desktop_native_widget_aura_win.h"

#include "chrome/browser/ui/views/apps/app_window_desktop_window_tree_host_win.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

AppWindowDesktopNativeWidgetAuraWin::AppWindowDesktopNativeWidgetAuraWin(
    ChromeNativeAppWindowViewsWin* app_window)
    : views::DesktopNativeWidgetAura(app_window->widget()),
      app_window_(app_window) {
  GetNativeWindow()->SetName("AppWindowAura");
}

AppWindowDesktopNativeWidgetAuraWin::~AppWindowDesktopNativeWidgetAuraWin() {
}

void AppWindowDesktopNativeWidgetAuraWin::InitNativeWidget(
    views::Widget::InitParams params) {
  tree_host_ = new AppWindowDesktopWindowTreeHostWin(app_window_, this);
  params.desktop_window_tree_host = tree_host_;
  DesktopNativeWidgetAura::InitNativeWidget(std::move(params));
}

void AppWindowDesktopNativeWidgetAuraWin::Maximize() {
  // Maximizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Maximize() does not also show the window.
  if (tree_host_ && !tree_host_->IsVisible()) {
    DesktopNativeWidgetAura::Show(ui::mojom::WindowShowState::kNormal,
                                  gfx::Rect());
  }
  DesktopNativeWidgetAura::Maximize();
}

void AppWindowDesktopNativeWidgetAuraWin::Minimize() {
  // Minimizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Minimize() does not also show the window.
  if (tree_host_ && !tree_host_->IsVisible()) {
    DesktopNativeWidgetAura::Show(ui::mojom::WindowShowState::kNormal,
                                  gfx::Rect());
  }
  DesktopNativeWidgetAura::Minimize();
}

void AppWindowDesktopNativeWidgetAuraWin::OnHostClosed() {
  tree_host_ = nullptr;
  views::DesktopNativeWidgetAura::OnHostClosed();
}
