// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/widget.h"

DesktopBrowserFrameAuraLinux::DesktopBrowserFrameAuraLinux(
    BrowserFrame* browser_frame,
    BrowserView* browser_view)
    : DesktopBrowserFrameAura(browser_frame, browser_view) {
  use_custom_frame_pref_.Init(
      prefs::kUseCustomChromeFrame,
      browser_view->browser()->profile()->GetPrefs(),
      base::BindRepeating(
          &DesktopBrowserFrameAuraLinux::OnUseCustomChromeFrameChanged,
          base::Unretained(this)));
}

DesktopBrowserFrameAuraLinux::~DesktopBrowserFrameAuraLinux() = default;

views::Widget::InitParams DesktopBrowserFrameAuraLinux::GetWidgetParams() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.native_widget = this;

  // Set up a custom WM_CLASS for some sorts of window types. This allows
  // task switchers in X11 environments to distinguish between main browser
  // windows and e.g app windows.
  const Browser& browser = *browser_view()->browser();
  params.wm_class_name =
      (browser.is_type_app() || browser.is_type_app_popup())
          ? shell_integration_linux::GetWMClassFromAppName(browser.app_name())
          // This window is a hosted app or v1 packaged app.
          // NOTE: v2 packaged app windows are created by
          // ChromeNativeAppWindowViews.
          : shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  params.wm_role_name = browser_view()->browser()->is_type_normal()
                            ? std::string(kX11WindowRoleBrowser)
                            : std::string(kX11WindowRolePopup);
  params.remove_standard_frame = UseCustomFrame();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  if ((browser.is_type_app() || browser.is_type_app_popup()) &&
      browser.profile()) {
    params.wayland_app_id = shell_integration_linux::GetXdgAppIdForWebApp(
        browser.app_name(), browser.profile()->GetPath());
  } else {
    params.wayland_app_id = params.wm_class_class;
  }

  return params;
}

bool DesktopBrowserFrameAuraLinux::UseCustomFrame() const {
  // If the platform does not support server side decorations, ignore the user
  // preference and return true.
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformRuntimeProperties()
           .supports_server_side_window_decorations) {
    return true;
  }

  // Normal browser windows get a custom frame (per the user's preference).
  if (use_custom_frame_pref_.GetValue() && browser_view()->GetIsNormalType()) {
    return true;
  }

  // Hosted app windows get a custom frame (if the desktop PWA experimental
  // feature is enabled), or if the window is picture in picture.
  return browser_view()->GetIsWebAppType() ||
         browser_view()->GetIsPictureInPictureType();
}

void DesktopBrowserFrameAuraLinux::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  host_->TabDraggingKindChanged(tab_drag_kind);
}

bool DesktopBrowserFrameAuraLinux::ShouldDrawRestoredFrameShadow() const {
  return host_->SupportsClientFrameShadow() && UseCustomFrame();
}

void DesktopBrowserFrameAuraLinux::OnUseCustomChromeFrameChanged() {
  // Tell the window manager to add or remove system borders.
  browser_frame()->set_frame_type(UseCustomFrame()
                                      ? views::Widget::FrameType::kForceCustom
                                      : views::Widget::FrameType::kForceNative);
  browser_frame()->FrameTypeChanged();
  host_->UpdateFrameHints();
}

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new DesktopBrowserFrameAuraLinux(browser_frame, browser_view);
}
