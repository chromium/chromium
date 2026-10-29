// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCREEN_CAPTURE_NOTIFICATION_VIRTUAL_DESKTOP_HELPER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_SCREEN_CAPTURE_NOTIFICATION_VIRTUAL_DESKTOP_HELPER_WIN_H_

#include <shobjidl.h>
#include <windows.h>

#include <wrl/client.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/win/registry.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

// Keeps the screen capture notification bar reachable across Windows virtual
// desktops while capture continues.
//
// While the bar is visible, `WS_EX_TOOLWINDOW` is applied so that the shell's
// Virtual Desktop Manager shows the (unowned) window on all virtual desktops
// instead of cloaking it. While the bar is minimized, `WS_EX_TOOLWINDOW` is
// removed so the window appears in the taskbar and can be restored, and the
// window is moved to the active virtual desktop whenever the user switches
// desktops. The active desktop is tracked by watching an undocumented Explorer
// registry key.
//
// The helper starts managing `widget` (which must not be minimized) on
// construction and stops when either the helper or the widget's native window
// is destroyed, whichever is first.
class ScreenCaptureNotificationVirtualDesktopHelperWin
    : public views::WidgetObserver {
 public:
  explicit ScreenCaptureNotificationVirtualDesktopHelperWin(
      views::Widget* widget);
  ScreenCaptureNotificationVirtualDesktopHelperWin(
      const ScreenCaptureNotificationVirtualDesktopHelperWin&) = delete;
  ScreenCaptureNotificationVirtualDesktopHelperWin& operator=(
      const ScreenCaptureNotificationVirtualDesktopHelperWin&) = delete;
  ~ScreenCaptureNotificationVirtualDesktopHelperWin() override;

  // Returns the path, relative to `HKEY_CURRENT_USER`, of the undocumented
  // Windows Explorer key that contains the `VirtualDesktops` key, which tracks
  // the active virtual desktop GUID. `VirtualDesktops` is persistent under
  // Explorer on Win11+, and per-session under `Explorer\SessionInfo\<id>` on
  // Win10. Returns `std::nullopt` on failure.
  static std::optional<std::wstring> GetVirtualDesktopsParentRegKeyPath();

  // Overrides the `IVirtualDesktopManager`, which is otherwise created lazily.
  void SetVirtualDesktopManagerForTesting(
      Microsoft::WRL::ComPtr<IVirtualDesktopManager> manager);

  // Returns whether a registry key is open to watch for virtual desktop
  // changes.
  bool IsWatchingRegistryForTesting() const;

  // views::WidgetObserver:
  void OnWidgetShowStateChanged(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Adds `WS_EX_TOOLWINDOW` and removes `WS_EX_APPWINDOW`, or vice versa.
  // Returns true if the extended style changed.
  bool UpdateToolWindowStyle(bool show_on_all_virtual_desktops);

  // Arms a one-shot watcher on the `VirtualDesktops` registry key, or on its
  // parent key if `VirtualDesktops` does not exist yet. Must only be called
  // when no watcher is armed (on construction, or from `OnRegistryChanged()`).
  void StartWatchingRegistry();
  void OnRegistryChanged();

  // Returns the active virtual desktop's GUID, or `std::nullopt` if the
  // registry value is unavailable.
  std::optional<GUID> GetCurrentVirtualDesktopId() const;

  // Returns the `IVirtualDesktopManager`, creating it if needed. Returns
  // `nullptr` on failure.
  IVirtualDesktopManager* GetVirtualDesktopManager();

  // Moves the window to the active virtual desktop. If `validate_and_report`
  // is true, reports a `DumpWithoutCrashing()` when the active desktop cannot
  // be determined even though the window is not on it.
  void MoveWindowToCurrentVirtualDesktop(bool validate_and_report);

  raw_ptr<views::Widget> widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // Explorer's `VirtualDesktops` key, which holds the `CurrentVirtualDesktop`
  // value. Watched for changes once it exists.
  base::win::RegKey virtual_desktops_reg_key_;
  // The parent of the `VirtualDesktops` key, which is watched only until the
  // `VirtualDesktops` key is created (when the user first switches desktops).
  base::win::RegKey virtual_desktops_parent_reg_key_;

  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SCREEN_CAPTURE_NOTIFICATION_VIRTUAL_DESKTOP_HELPER_WIN_H_
