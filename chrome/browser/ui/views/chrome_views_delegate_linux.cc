// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/environment.h"
#include "base/feature_list.h"
#include "base/nix/xdg_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/version_info/channel.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/linux/linux_ui.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Returns true if the window manager forcefully draws a title bar over
// maximized windows, in which case Chrome must not draw its own caption
// buttons.
//
// Ubuntu Unity is the only desktop that does this, and it only ever shipped as
// an X11 session. Unity is detected from XDG_CURRENT_DESKTOP, which Chrome
// inherits from whichever process started it rather than from the running
// session, so the variable can name a desktop that isn't actually in use when
// Chrome is launched from a terminal, from another application, or from an
// automation harness. Requiring a non-Wayland session keeps such a value from
// stripping the minimize, maximize and close buttons out of a maximized window
// on Wayland, where nothing replaces them: a Wayland compositor cannot add
// decorations to a window that draws its own.
bool WindowManagerDrawsTitleBarOverMaximizedWindows() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (base::nix::GetSessionType(*env) == base::nix::SessionType::kWayland) {
    return false;
  }

  base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  return desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY;
}

#if BUILDFLAG(IS_LINUX)
int GetWindowIconResourceId() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (chrome::GetChannel()) {
    case version_info::Channel::DEV:
      return IDR_PRODUCT_LOGO_128_DEV;
    case version_info::Channel::BETA:
      return IDR_PRODUCT_LOGO_128_BETA;
    default:
      break;
  }
#endif
  return IDR_PRODUCT_LOGO_128;
}
#endif  // BUILDFLAG(IS_LINUX)

NativeWidgetType GetNativeWidgetTypeForInitParams(
    const views::Widget::InitParams& params) {
  // If this is a security surface, always use a toplevel window,
  // otherwise it's possible for things like menus to obscure the view.
  if (params.z_order &&
      params.z_order.value() == ui::ZOrderLevel::kSecuritySurface) {
    return NativeWidgetType::kDesktopNativeWidgetAura;
  }

  const bool default_desktop_bubble =
      (params.type == views::Widget::InitParams::TYPE_BUBBLE ||
       params.type == views::Widget::InitParams::TYPE_POPUP) &&
      base::FeatureList::IsEnabled(features::kOzoneBubblesUsePlatformWidgets) &&
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_subwindows_as_accelerated_widgets;

  if (!params.child &&
      params.use_accelerated_widget_override.value_or(default_desktop_bubble)) {
    return NativeWidgetType::kDesktopNativeWidgetAura;
  }

  if (params.delegate && params.delegate->use_desktop_widget_override()) {
    return NativeWidgetType::kDesktopNativeWidgetAura;
  }

  return (params.parent &&
          params.type != views::Widget::InitParams::TYPE_MENU &&
          params.type != views::Widget::InitParams::TYPE_TOOLTIP)
             ? NativeWidgetType::kNativeWidgetAura
             : NativeWidgetType::kDesktopNativeWidgetAura;
}

}  // namespace

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  return ::CreateNativeWidget(GetNativeWidgetTypeForInitParams(*params), params,
                              delegate);
}

#if BUILDFLAG(IS_LINUX)
gfx::ImageSkia* ChromeViewsDelegate::GetDefaultWindowIcon() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(GetWindowIconResourceId());
}
#endif  // BUILDFLAG(IS_LINUX)

bool ChromeViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  // On Ubuntu Unity, the system always provides a title bar for
  // maximized windows.
  //
  // TODO(thomasanderson,crbug.com/40549424): Consider using the
  // _UNITY_SHELL wm hint when support for Ubuntu Trusty is dropped.
  if (!maximized) {
    return false;
  }

  static bool window_manager_draws_title_bar =
      WindowManagerDrawsTitleBarOverMaximizedWindows();
  return window_manager_draws_title_bar;
}
