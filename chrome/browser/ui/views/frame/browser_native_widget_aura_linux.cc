// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_aura_linux.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/common/pref_names.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/widget.h"

BrowserNativeWidgetAuraLinux::BrowserNativeWidgetAuraLinux(
    BrowserWidget* browser_widget,
    BrowserView* browser_view)
    : BrowserNativeWidgetAura(browser_widget, browser_view) {
  use_custom_frame_pref_.Init(
      prefs::kUseCustomChromeFrame,
      browser_view->browser()->GetProfile()->GetPrefs(),
      base::BindRepeating(
          &BrowserNativeWidgetAuraLinux::OnUseCustomChromeFrameChanged,
          base::Unretained(this)));
  desktop_entry_subscription_ =
      shell_integration_linux::RegisterPerProfileDesktopEntryChangedCallback(
          base::BindRepeating(
              &BrowserNativeWidgetAuraLinux::OnPerProfileDesktopEntryChanged,
              weak_ptr_factory_.GetWeakPtr()));
}

BrowserNativeWidgetAuraLinux::~BrowserNativeWidgetAuraLinux() = default;

void BrowserNativeWidgetAuraLinux::OnHostClosed() {
  host_ = nullptr;
  BrowserNativeWidgetAura::OnHostClosed();
}

views::Widget::InitParams BrowserNativeWidgetAuraLinux::GetWidgetParams(
    views::Widget::InitParams::Ownership ownership) {
  views::Widget::InitParams params(ownership);
  params.native_widget = this;

  const WindowClass window_class = GetWindowClass();
  params.wm_class_name = window_class.wm_class_name;
  params.wm_class_class = window_class.wm_class_class;
  params.wayland_app_id = window_class.wayland_app_id;
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  params.wm_role_name = browser_view()->browser()->GetType() ==
                                BrowserWindowInterface::Type::TYPE_NORMAL
                            ? std::string(kX11WindowRoleBrowser)
                            : std::string(kX11WindowRolePopup);
  params.remove_standard_frame = UseCustomFrame();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  return params;
}

BrowserNativeWidgetAuraLinux::WindowClass
BrowserNativeWidgetAuraLinux::GetWindowClass() const {
  WindowClass window_class;
  // Set up a custom WM_CLASS for some sorts of window types. This allows
  // task switchers in X11 environments to distinguish between main browser
  // windows and e.g app windows.
  const BrowserWindowInterface& browser = *browser_view()->browser();
  const bool is_app =
      browser.GetType() == BrowserWindowInterface::Type::TYPE_APP ||
      browser.GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP;
  window_class.wm_class_name =
      is_app ? shell_integration_linux::GetWMClassFromAppName(
                   BrowserInitState::From(&browser)->create_params().app_name)
             // This window is a hosted app or v1 packaged app.
             // NOTE: v2 packaged app windows are created by
             // ChromeNativeAppWindowViews.
             : shell_integration_linux::GetProgramClassName();
  window_class.wm_class_class = shell_integration_linux::GetProgramClassClass();

  if (is_app && browser.GetProfile()) {
    window_class.wayland_app_id = shell_integration_linux::GetXdgAppIdForWebApp(
        BrowserInitState::From(&browser)->create_params().app_name,
        browser.GetProfile()->GetPath());
  } else {
    window_class.wayland_app_id = window_class.wm_class_class;
    // Give windows of non-default profiles their own identity if a matching
    // desktop entry is installed, so the desktop environment can show them
    // with the profile's own launcher and icon. Off-the-record windows use the
    // identity of their original profile.
    if (!is_app && browser.GetProfile()) {
      const std::string desktop_id =
          shell_integration_linux::GetInstalledPerProfileDesktopId(
              browser.GetProfile()->GetOriginalProfile()->GetPath());
      if (!desktop_id.empty()) {
        window_class.wm_class_name = desktop_id;
        window_class.wayland_app_id = desktop_id;
      }
    }
  }
  return window_class;
}

void BrowserNativeWidgetAuraLinux::OnPerProfileDesktopEntryChanged(
    const base::FilePath& profile_path) {
  const BrowserWindowInterface* browser =
      browser_view() ? browser_view()->browser() : nullptr;
  if (!host_ || !browser || !browser->GetProfile() ||
      browser->GetProfile()->GetOriginalProfile()->GetPath() != profile_path) {
    return;
  }
  // The profile's desktop entry was installed or removed while this window
  // was open, so switch the window to the identity new windows of the profile
  // get. When an entry is removed, this runs before the file is deleted, so
  // the window never refers to a missing entry.
  const WindowClass window_class = GetWindowClass();
  host_->platform_window()->SetWindowClass(window_class.wm_class_name,
                                           window_class.wm_class_class,
                                           window_class.wayland_app_id);
}

bool BrowserNativeWidgetAuraLinux::UseCustomFrame() const {
  if (!browser_view()) {
    return false;
  }

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
  // feature is enabled), or if the window is a floating window.
  return browser_view()->GetIsWebAppType() ||
         browser_view()->GetIsFloatingType();
}

void BrowserNativeWidgetAuraLinux::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  host_->TabDraggingKindChanged(tab_drag_kind);
}

void BrowserNativeWidgetAuraLinux::ClientDestroyedWidget() {
  use_custom_frame_pref_.Destroy();
  BrowserNativeWidgetAura::ClientDestroyedWidget();
}

bool BrowserNativeWidgetAuraLinux::ShouldDrawRestoredFrameShadow() const {
  return host_->SupportsClientFrameShadow() && UseCustomFrame();
}

void BrowserNativeWidgetAuraLinux::OnUseCustomChromeFrameChanged() {
  if (!browser_widget()) {
    return;
  }

  // Tell the window manager to add or remove system borders.
  browser_widget()->set_frame_type(
      UseCustomFrame() ? views::Widget::FrameType::kForceCustom
                       : views::Widget::FrameType::kForceNative);
  browser_widget()->FrameTypeChanged();
  host_->UpdateFrameHints();
}

BrowserNativeWidget* BrowserNativeWidgetFactory::Create(
    BrowserWidget* browser_widget,
    BrowserView* browser_view) {
  return new BrowserNativeWidgetAuraLinux(browser_widget, browser_view);
}
