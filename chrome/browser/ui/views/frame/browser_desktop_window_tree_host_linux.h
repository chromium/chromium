// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"  // nogncheck

#if defined(USE_DBUS_MENU)
#include "chrome/browser/ui/views/frame/dbus_appmenu.h"  // nogncheck
#endif

class BrowserFrame;
class BrowserView;
class DesktopBrowserFrameAuraLinux;
enum class TabDragKind;

namespace views {
class DesktopNativeWidgetAura;
}  // namespace views

namespace ui {
class LinuxUi;
class NativeTheme;
}  // namespace ui

class BrowserDesktopWindowTreeHostLinux
    : public BrowserDesktopWindowTreeHost,
      public views::DesktopWindowTreeHostLinux,
      ui::NativeThemeObserver,
      ui::DeviceScaleFactorObserver {
 public:
  BrowserDesktopWindowTreeHostLinux(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);

  BrowserDesktopWindowTreeHostLinux(const BrowserDesktopWindowTreeHostLinux&) =
      delete;
  BrowserDesktopWindowTreeHostLinux& operator=(
      const BrowserDesktopWindowTreeHostLinux&) = delete;

  ~BrowserDesktopWindowTreeHostLinux() override;

  // Called when the tab drag status changes for this window.
  void TabDraggingKindChanged(TabDragKind tab_drag_kind);

  // Returns true if the system supports client-drawn shadows.  We may still
  // choose not to draw a shadow eg. when the "system titlebar and borders"
  // setting is enabled, or when the window is maximized/fullscreen.
  bool SupportsClientFrameShadow() const;

  // views::DesktopWindowTreeHostLinux:
  void UpdateFrameHints() override;

 private:
  // DesktopWindowTreeHostPlatform:
  void AddAdditionalInitProperties(
      const views::Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

  // BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // BrowserWindowTreeHostPlatform:
  void FrameTypeChanged() override;

  // views::DesktopWindowTreeHostLinux:
  void Init(const views::Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  void CloseNow() override;
  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool SupportsMouseLock() override;
  void LockMouse(aura::Window* window) override;
  void UnlockMouse(aura::Window* window) override;

  // ui::X11ExtensionDelegate:
  bool IsOverrideRedirect() const override;

  // ui::PlatformWindowDelegate
  gfx::Insets CalculateInsetsInDIP(
      ui::PlatformWindowState window_state) const override;
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override;
  void OnWindowTiledStateChanged(ui::WindowTiledEdges new_tiled_edges) override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // views::OnDeviceScaleFactorChanged:
  void OnDeviceScaleFactorChanged() override;

  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<BrowserFrame> browser_frame_ = nullptr;
  raw_ptr<DesktopBrowserFrameAuraLinux> native_frame_ = nullptr;

#if defined(USE_DBUS_MENU)
  // Each browser frame maintains its own menu bar object because the lower
  // level dbus protocol associates a xid to a menu bar; we can't map multiple
  // xids to the same menu bar.
  std::unique_ptr<DbusAppmenu> dbus_appmenu_;
#endif

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};
  base::ScopedObservation<ui::LinuxUi, ui::DeviceScaleFactorObserver>
      scale_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
