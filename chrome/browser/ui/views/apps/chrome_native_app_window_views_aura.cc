// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"

#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/apps/app_window_easy_resize_window_targeter.h"
#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/profiles/profile.h"
#endif

using extensions::AppWindow;

ui::mojom::WindowShowState ChromeNativeAppWindowViewsAura::GetRestorableState(
    const ui::mojom::WindowShowState restore_state) const {
  // Allowlist states to return so that invalid and transient states
  // are not saved and used to restore windows when they are recreated.
  switch (restore_state) {
    case ui::mojom::WindowShowState::kNormal:
    case ui::mojom::WindowShowState::kMaximized:
    case ui::mojom::WindowShowState::kFullscreen:
      return restore_state;

    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kMinimized:
    case ui::mojom::WindowShowState::kInactive:
    case ui::mojom::WindowShowState::kEnd:
      return ui::mojom::WindowShowState::kNormal;
  }

  return ui::mojom::WindowShowState::kNormal;
}

void ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
#if BUILDFLAG(IS_LINUX)
  std::string app_name =
      web_app::GenerateApplicationNameFromAppId(app_window()->extension_id());
  // Set up a custom WM_CLASS for app windows. This allows task switchers in
  // X11 environments to distinguish them from main browser windows.
  init_params->wm_class_name =
      shell_integration_linux::GetWMClassFromAppName(app_name);
  init_params->wm_class_class = shell_integration_linux::GetProgramClassClass();
  const char kX11WindowRoleApp[] = "app";
  init_params->wm_role_name = std::string(kX11WindowRoleApp);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params->restore_session_id = app_window()->session_id().id();
  if (app_window()->GetExtension()) {
    init_params->restore_window_id_source = app_window()->GetExtension()->id();
  }
#endif

  ChromeNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                 widget);
}

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViewsAura::CreateNonStandardAppFrame() {
  auto frame = std::make_unique<apps::AppWindowFrameView>(
      widget(), this, HasFrameColor(), ActiveFrameColor(),
      InactiveFrameColor());
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

ui::mojom::WindowShowState ChromeNativeAppWindowViewsAura::GetRestoredState()
    const {
  // First normal states are checked.
  if (IsMaximized())
    return ui::mojom::WindowShowState::kMaximized;
  if (IsFullscreen()) {
    return ui::mojom::WindowShowState::kFullscreen;
  }

  // Use kRestoreShowStateKey to get the window restore show state in case a
  // window is minimized/hidden.
  ui::mojom::WindowShowState restore_state =
      widget()->GetNativeWindow()->GetProperty(
          aura::client::kRestoreShowStateKey);
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
