// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_lacros.h"
#else  // defined(OS_LINUX)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#endif

#if defined(USE_DBUS_MENU)

namespace {

#if defined(USE_DBUS_MENU)
bool CreateGlobalMenuBar() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return ui::OzonePlatform::GetInstance()
        ->GetPlatformProperties()
        .supports_global_application_menus;
  }
#endif
  return true;
}
#endif

}  // namespace

#endif  // defined(USE_DBUS_MENU)

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux, public:

BrowserDesktopWindowTreeHostLinux::BrowserDesktopWindowTreeHostLinux(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostLinuxImpl(native_widget_delegate,
                                     desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  using DesktopBrowserFrameAuraPlatform = DesktopBrowserFrameLacros;
#elif defined(OS_LINUX)
  using DesktopBrowserFrameAuraPlatform = DesktopBrowserFrameAuraLinux;
#else
#error Unknown platform
#endif
  static_cast<DesktopBrowserFrameAuraPlatform*>(
      browser_frame->native_browser_frame())
      ->set_host(this);
  browser_frame->set_frame_type(browser_frame->UseCustomFrame()
                                    ? views::Widget::FrameType::kForceCustom
                                    : views::Widget::FrameType::kForceNative);
}

BrowserDesktopWindowTreeHostLinux::~BrowserDesktopWindowTreeHostLinux() =
    default;

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostLinux::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostLinux::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostLinux::UsesNativeSystemMenu() const {
  return false;
}

bool BrowserDesktopWindowTreeHostLinux::SupportsMouseLock() {
  auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
  if (!wayland_extension)
    return false;

  return wayland_extension->SupportsPointerLock();
}

void BrowserDesktopWindowTreeHostLinux::LockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::LockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(true /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::UnlockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::UnlockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(false /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  // If there's no tabs left, the browser window is about to close, so don't
  // call SetOverrideRedirect() to prevent the window from flashing.
  if (!browser_view_->tabstrip()->GetModelCount())
    return;

  auto* x11_extension = GetX11Extension();
  if (x11_extension && x11_extension->IsWmTiling()) {
    bool was_dragging_window =
        browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs;
    bool is_dragging_window = tab_drag_kind == TabDragKind::kAllTabs;
    if (is_dragging_window != was_dragging_window)
      x11_extension->SetOverrideRedirect(is_dragging_window);
  }

  if (auto* wayland_extension = ui::GetWaylandExtension(*platform_window())) {
    if (tab_drag_kind != TabDragKind::kNone)
      wayland_extension->StartWindowDraggingSessionIfNeeded();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     DesktopWindowTreeHostLinuxImpl implementation:

void BrowserDesktopWindowTreeHostLinux::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostLinuxImpl::Init(std::move(params));

#if defined(USE_DBUS_MENU)
  // We have now created our backing X11 window.  We now need to (possibly)
  // alert the desktop environment that there's a menu bar attached to it.
  if (CreateGlobalMenuBar()) {
    dbus_appmenu_ =
        std::make_unique<DbusAppmenu>(browser_view_, GetAcceleratedWidget());
  }
#endif
}

void BrowserDesktopWindowTreeHostLinux::CloseNow() {
#if defined(USE_DBUS_MENU)
  dbus_appmenu_.reset();
#endif
  DesktopWindowTreeHostLinuxImpl::CloseNow();
}

bool BrowserDesktopWindowTreeHostLinux::IsOverrideRedirect(
    bool is_tiling_wm) const {
  return (browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs) &&
         is_tiling_wm;
}

void BrowserDesktopWindowTreeHostLinux::OnWindowStateChanged(
    ui::PlatformWindowState old_window_show_state,
    ui::PlatformWindowState new_window_show_state) {
  DesktopWindowTreeHostLinux::OnWindowStateChanged(old_window_show_state,
                                                   new_window_show_state);

  bool fullscreen_changed =
      new_window_show_state == ui::PlatformWindowState::kFullScreen ||
      old_window_show_state == ui::PlatformWindowState::kFullScreen;
  if (old_window_show_state != new_window_show_state && fullscreen_changed) {
    // If the browser view initiated this state change,
    // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
    browser_view_->FullscreenStateChanging();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// TODO(crbug.com/1221374): Separate Lacros specific codes into
// browser_desktop_window_tree_host_lacros.cc.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostLinux(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               browser_view, browser_frame);
}
#endif
