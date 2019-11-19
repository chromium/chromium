// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"

#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/apps/app_window_easy_resize_window_targeter.h"
#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

#if defined(USE_X11)
#include "chrome/browser/shell_integration_linux.h"
#endif

using extensions::AppWindow;

ChromeNativeAppWindowViewsAura::ChromeNativeAppWindowViewsAura() {
}

ChromeNativeAppWindowViewsAura::~ChromeNativeAppWindowViewsAura() {
}

ui::WindowShowState
ChromeNativeAppWindowViewsAura::GetRestorableState(
    const ui::WindowShowState restore_state) const {
  // Whitelist states to return so that invalid and transient states
  // are not saved and used to restore windows when they are recreated.
  switch (restore_state) {
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      return restore_state;

    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_MINIMIZED:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_END:
      return ui::SHOW_STATE_NORMAL;
  }

  return ui::SHOW_STATE_NORMAL;
}

void ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
#if defined(USE_X11)
  std::string app_name =
      web_app::GenerateApplicationNameFromAppId(app_window()->extension_id());
  // Set up a custom WM_CLASS for app windows. This allows task switchers in
  // X11 environments to distinguish them from main browser windows.
  init_params->wm_class_name =
      shell_integration_linux::GetWMClassFromAppName(app_name);
  init_params->wm_class_class = shell_integration_linux::GetProgramClassClass();
  const char kX11WindowRoleApp[] = "app";
  init_params->wm_role_name = std::string(kX11WindowRoleApp);
#endif

  ChromeNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                 widget);
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsAura::CreateNonStandardAppFrame() {
  apps::AppWindowFrameView* frame =
      new apps::AppWindowFrameView(widget(), this, HasFrameColor(),
                                   ActiveFrameColor(), InactiveFrameColor());
  frame->Init();

  // Install an easy resize window targeter, which ensures that the root window
  // (not the app) receives mouse events on the edges.
  aura::Window* window = widget()->GetNativeWindow();
  // Add the AppWindowEasyResizeWindowTargeter on the window, not its root
  // window. The root window does not have a delegate, which is needed to
  // handle the event in Linux.
  window->SetEventTargeter(std::make_unique<AppWindowEasyResizeWindowTargeter>(
      gfx::Insets(frame->resize_inside_bounds_size()), this));

  return frame;
}

ui::WindowShowState ChromeNativeAppWindowViewsAura::GetRestoredState() const {
  // First normal states are checked.
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen()) {
    return ui::SHOW_STATE_FULLSCREEN;
  }

  // Use kPreMinimizedShowStateKey in case a window is minimized/hidden.
  ui::WindowShowState restore_state = widget()->GetNativeWindow()->GetProperty(
      aura::client::kPreMinimizedShowStateKey);
  return GetRestorableState(restore_state);
}

ui::ZOrderLevel ChromeNativeAppWindowViewsAura::GetZOrderLevel() const {
  return widget()->GetZOrderLevel();
}

void ChromeNativeAppWindowViewsAura::UpdateShape(
    std::unique_ptr<ShapeRects> rects) {
  bool had_shape = !!shape();

  ChromeNativeAppWindowViews::UpdateShape(std::move(rects));

  aura::Window* native_window = widget()->GetNativeWindow();
  if (shape() && !had_shape) {
    native_window->SetEventTargeter(
        std::make_unique<ShapedAppWindowTargeter>(this));
  } else if (!shape() && had_shape) {
    native_window->SetEventTargeter(nullptr);
  }
}
