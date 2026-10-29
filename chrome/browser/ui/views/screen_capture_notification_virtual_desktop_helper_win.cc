// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/screen_capture_notification_virtual_desktop_helper_win.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/win/windows_version.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace {

constexpr wchar_t kVirtualDesktopsRegKeyName[] = L"VirtualDesktops";
constexpr wchar_t kCurrentVirtualDesktopRegValueName[] =
    L"CurrentVirtualDesktop";

}  // namespace

ScreenCaptureNotificationVirtualDesktopHelperWin::
    ScreenCaptureNotificationVirtualDesktopHelperWin(views::Widget* widget)
    : widget_(widget) {
  CHECK(widget_);
  CHECK(!widget_->IsMinimized());
  UpdateToolWindowStyle(/*show_on_all_virtual_desktops=*/true);
  widget_observation_.Observe(widget_);
  StartWatchingRegistry();
}

ScreenCaptureNotificationVirtualDesktopHelperWin::
    ~ScreenCaptureNotificationVirtualDesktopHelperWin() = default;

// static
std::optional<std::wstring> ScreenCaptureNotificationVirtualDesktopHelperWin::
    GetVirtualDesktopsParentRegKeyPath() {
  constexpr wchar_t kExplorerRegKeyPath[] =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    return kExplorerRegKeyPath;
  }
  DWORD session_id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id)) {
    return std::nullopt;
  }
  return base::StrCat({kExplorerRegKeyPath, L"\\SessionInfo\\",
                       base::NumberToWString(session_id)});
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::
    SetVirtualDesktopManagerForTesting(
        Microsoft::WRL::ComPtr<IVirtualDesktopManager> manager) {
  virtual_desktop_manager_ = std::move(manager);
}

bool ScreenCaptureNotificationVirtualDesktopHelperWin::
    IsWatchingRegistryForTesting() const {
  return virtual_desktops_reg_key_.Valid() ||
         virtual_desktops_parent_reg_key_.Valid();
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::OnWidgetShowStateChanged(
    views::Widget* widget) {
  const bool is_minimized = widget->IsMinimized();
  // Move the window only when the style actually changes, since
  // `SWP_FRAMECHANGED` synchronously re-enters `OnWidgetShowStateChanged()`.
  if (UpdateToolWindowStyle(
          /*show_on_all_virtual_desktops=*/!is_minimized) &&
      is_minimized) {
    MoveWindowToCurrentVirtualDesktop(/*validate_and_report=*/true);
  }
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
  widget_ = nullptr;
  // Resetting (rather than `Close()`) also destroys the internal key watchers.
  virtual_desktops_reg_key_ = base::win::RegKey();
  virtual_desktops_parent_reg_key_ = base::win::RegKey();
  virtual_desktop_manager_.Reset();
}

// Windows shows unowned `WS_EX_TOOLWINDOW` windows on all virtual desktops, at
// the cost of hiding them from the taskbar and Alt+Tab. Clear
// `WS_EX_TOOLWINDOW` when minimized so the window appears in the taskbar and
// can be restored. `WS_EX_APPWINDOW` forces a taskbar button even for a tool
// window, so it is cleared while showing on all virtual desktops, and set while
// minimized to guarantee a taskbar button.
bool ScreenCaptureNotificationVirtualDesktopHelperWin::UpdateToolWindowStyle(
    bool show_on_all_virtual_desktops) {
  CHECK(widget_);
  HWND hwnd = views::HWNDForWidget(widget_);
  LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  LONG new_ex_style = show_on_all_virtual_desktops
                          ? ((ex_style | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW)
                          : ((ex_style & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW);
  if (new_ex_style == ex_style) {
    return false;
  }
  ::SetWindowLong(hwnd, GWL_EXSTYLE, new_ex_style);
  // Flush the cached extended style so the shell updates taskbar state.
  ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
  return true;
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::StartWatchingRegistry() {
  const std::optional<std::wstring> parent_path =
      GetVirtualDesktopsParentRegKeyPath();
  if (!parent_path.has_value()) {
    return;
  }

  if (!virtual_desktops_reg_key_.Valid()) {
    virtual_desktops_reg_key_ = base::win::RegKey(
        HKEY_CURRENT_USER,
        base::StrCat({*parent_path, L"\\", kVirtualDesktopsRegKeyName}).c_str(),
        KEY_QUERY_VALUE | KEY_NOTIFY);
  }

  // `StartWatching()` is one-shot and is re-armed in `OnRegistryChanged()`.
  // `Unretained(this)` is safe because `this` owns both keys, and destroying a
  // key also destroys its watcher.
  auto on_changed = base::BindOnce(
      &ScreenCaptureNotificationVirtualDesktopHelperWin::OnRegistryChanged,
      base::Unretained(this));

  if (virtual_desktops_reg_key_.Valid()) {
    // Stop watching the parent key if it was watched until now. It is safe to
    // destroy the parent key's watcher from within its own callback.
    virtual_desktops_parent_reg_key_ = base::win::RegKey();
    virtual_desktops_reg_key_.StartWatching(std::move(on_changed));
    return;
  }

  // The `VirtualDesktops` key may not exist until the user first switches
  // virtual desktops. Watch its parent key until `VirtualDesktops` is created,
  // so the bar can still follow desktop switches if it is minimized before
  // then. The watch includes the parent's subtree, so it also fires for
  // unrelated changes, but only until `VirtualDesktops` is created.
  if (!virtual_desktops_parent_reg_key_.Valid()) {
    virtual_desktops_parent_reg_key_ =
        base::win::RegKey(HKEY_CURRENT_USER, parent_path->c_str(), KEY_NOTIFY);
  }
  if (virtual_desktops_parent_reg_key_.Valid()) {
    virtual_desktops_parent_reg_key_.StartWatching(std::move(on_changed));
  }
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::OnRegistryChanged() {
  StartWatchingRegistry();
  if (!widget_ || !widget_->IsMinimized()) {
    return;
  }

  // Do not report a missing value here: this also fires for unrelated registry
  // changes, and Explorer may create the `VirtualDesktops` key before writing
  // `CurrentVirtualDesktop`.
  MoveWindowToCurrentVirtualDesktop(/*validate_and_report=*/false);
}

std::optional<GUID>
ScreenCaptureNotificationVirtualDesktopHelperWin::GetCurrentVirtualDesktopId()
    const {
  if (!virtual_desktops_reg_key_.Valid()) {
    return std::nullopt;
  }

  GUID desktop_id = GUID_NULL;
  DWORD size = sizeof(desktop_id);
  DWORD type = REG_NONE;
  LONG result = virtual_desktops_reg_key_.ReadValue(
      kCurrentVirtualDesktopRegValueName, &desktop_id, &size, &type);
  // Value does not exist (`ERROR_FILE_NOT_FOUND`) until the user switches
  // virtual desktops for the first time.
  if (result != ERROR_SUCCESS || type != REG_BINARY ||
      size != sizeof(desktop_id) || desktop_id == GUID_NULL) {
    return std::nullopt;
  }

  return desktop_id;
}

IVirtualDesktopManager*
ScreenCaptureNotificationVirtualDesktopHelperWin::GetVirtualDesktopManager() {
  if (!virtual_desktop_manager_) {
    HRESULT hr =
        ::CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(&virtual_desktop_manager_));
    if (FAILED(hr)) {
      virtual_desktop_manager_.Reset();
    }
  }
  return virtual_desktop_manager_.Get();
}

void ScreenCaptureNotificationVirtualDesktopHelperWin::
    MoveWindowToCurrentVirtualDesktop(bool validate_and_report) {
  CHECK(widget_);
  HWND hwnd = views::HWNDForWidget(widget_);
  std::optional<GUID> desktop_id = GetCurrentVirtualDesktopId();
  if (!desktop_id.has_value()) {
    // If the window is on another virtual desktop, the user has switched
    // desktops and the registry value should have been populated unless
    // Windows changed the undocumented registry key.
    if (validate_and_report && GetVirtualDesktopManager() &&
        !gfx::IsWindowOnCurrentVirtualDesktop(hwnd, virtual_desktop_manager_)
             .value_or(true)) {
      base::debug::DumpWithoutCrashing();
    }
    return;
  }

  if (IVirtualDesktopManager* manager = GetVirtualDesktopManager()) {
    manager->MoveWindowToDesktop(hwnd, *desktop_id);
  }
}
