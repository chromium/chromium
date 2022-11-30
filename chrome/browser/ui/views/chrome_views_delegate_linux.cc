// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/version_info/channel.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/linux/linux_ui.h"

namespace {

bool IsDesktopEnvironmentUnity() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
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
    return NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
  }

  if (params.requires_accelerated_widget)
    return NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;

  return (params.parent &&
          params.type != views::Widget::InitParams::TYPE_MENU &&
          params.type != views::Widget::InitParams::TYPE_TOOLTIP)
             ? NativeWidgetType::NATIVE_WIDGET_AURA
             : NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
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
  // TODO(thomasanderson,crbug.com/784010): Consider using the
  // _UNITY_SHELL wm hint when support for Ubuntu Trusty is dropped.
  if (!maximized)
    return false;
  static bool is_desktop_environment_unity = IsDesktopEnvironmentUnity();
  return is_desktop_environment_unity;
}
