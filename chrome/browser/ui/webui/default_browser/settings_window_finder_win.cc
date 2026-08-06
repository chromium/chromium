// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/settings_window_finder_win.h"

#include <objbase.h>

#include <propkey.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"

namespace {

base::WeakPtr<SettingsWindowFinderWin>& GetGlobalFinderInstance() {
  static base::NoDestructor<base::WeakPtr<SettingsWindowFinderWin>> instance;
  return *instance;
}

bool GetWindowProcessImagePath(HWND hwnd, std::wstring* path_out) {
  DWORD pid;
  if (!::GetWindowThreadProcessId(hwnd, &pid)) {
    return false;
  }

  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.is_valid()) {
    return false;
  }

  wchar_t buffer[MAX_PATH];
  DWORD size = std::size(buffer);
  if (!::QueryFullProcessImageNameW(process.get(), 0, buffer, &size)) {
    return false;
  }

  *path_out = buffer;
  return true;
}

}  // namespace

SettingsWindowFinderWin::SettingsWindowFinderWin() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SettingsWindowFinderWin::~SettingsWindowFinderWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  StopObservingLocationChanges();
}

void SettingsWindowFinderWin::Start(base::TimeDelta timeout,
                                    WindowFoundCallback on_found,
                                    base::OnceClosure on_timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_found_ = std::move(on_found);
  on_timeout_ = std::move(on_timeout);

  if (HWND existing_hwnd = FindSettingsTopLevelWindow()) {
    WindowFoundCallback callback = std::move(on_found_);
    on_timeout_.Reset();
    std::move(callback).Run(existing_hwnd);
    return;
  }

  is_active_ = true;
  winevent_hook_ =
      ::SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr,
                        &SettingsWindowFinderWin::WinEventCallback, 0, 0,
                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  UpdateGlobalInstance();

  timeout_timer_.Start(FROM_HERE, timeout, this,
                       &SettingsWindowFinderWin::OnTimeout);
}

void SettingsWindowFinderWin::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timeout_timer_.Stop();

  if (winevent_hook_) {
    ::UnhookWinEvent(winevent_hook_);
    winevent_hook_ = nullptr;
  }

  is_active_ = false;
  UpdateGlobalInstance();

  on_found_.Reset();
  on_timeout_.Reset();
}

void SettingsWindowFinderWin::StartObservingLocationChanges(
    HWND settings_hwnd,
    WindowResizedCallback on_resized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopObservingLocationChanges();

  observed_hwnd_ = settings_hwnd;
  on_resized_ = std::move(on_resized);

  location_change_hook_ = ::SetWinEventHook(
      EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
      &SettingsWindowFinderWin::WinEventCallback, 0, 0,
      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  UpdateGlobalInstance();
}

void SettingsWindowFinderWin::StopObservingLocationChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (location_change_hook_) {
    ::UnhookWinEvent(location_change_hook_);
    location_change_hook_ = nullptr;
  }
  observed_hwnd_ = nullptr;
  on_resized_.Reset();

  UpdateGlobalInstance();
}

void SettingsWindowFinderWin::UpdateGlobalInstance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (winevent_hook_ || location_change_hook_) {
    if (GetGlobalFinderInstance().get() != this) {
      if (auto old_finder = GetGlobalFinderInstance()) {
        old_finder->Stop();
        old_finder->StopObservingLocationChanges();
      }
      GetGlobalFinderInstance() = weak_ptr_factory_.GetWeakPtr();
    }
  } else if (GetGlobalFinderInstance().get() == this) {
    // No hooks are active, reset the global weak pointer.
    GetGlobalFinderInstance().reset();
  }
}

void SettingsWindowFinderWin::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!on_timeout_) {
    return;
  }
  base::OnceClosure callback = std::move(on_timeout_);
  Stop();
  std::move(callback).Run();
}

HWND SettingsWindowFinderWin::FindSettingsTopLevelWindow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HWND hwnd = nullptr;
  while ((hwnd = ::FindWindowExW(nullptr, hwnd, L"ApplicationFrameWindow",
                                 nullptr)) != nullptr) {
    if (IsLikelySettingsWindow(hwnd)) {
      return hwnd;
    }
  }
  return nullptr;
}

bool SettingsWindowFinderWin::IsLikelySettingsWindow(HWND hwnd) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!::IsWindow(hwnd) || !::IsWindowVisible(hwnd)) {
    return false;
  }

  base::FilePath system_dir;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_dir)) {
    return false;
  }

  std::wstring path;
  if (!GetWindowProcessImagePath(hwnd, &path)) {
    return false;
  }

  bool is_app_frame = base::FilePath::CompareEqualIgnoreCase(
      path,
      system_dir.Append(FILE_PATH_LITERAL("ApplicationFrameHost.exe")).value());
  if (!is_app_frame) {
    return false;
  }

  // First check via PKEY_AppUserModel_ID.
  Microsoft::WRL::ComPtr<IPropertyStore> prop_store;
  if (FAILED(::SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&prop_store))) &&
      prop_store) {
    return false;
  }

  base::win::ScopedPropVariant prop_var;
  if (FAILED(prop_store->GetValue(PKEY_AppUserModel_ID, prop_var.Receive()))) {
    return false;
  }

  if (prop_var.get().vt == VT_LPWSTR && prop_var.get().pwszVal) {
    std::wstring_view app_id(prop_var.get().pwszVal);
    if (base::EqualsCaseInsensitiveASCII(
            app_id,
            L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft."
            L"windows.immersivecontrolpanel")) {
      return true;
    }
  }

  return false;
}

// static
void CALLBACK
SettingsWindowFinderWin::WinEventCallback(HWINEVENTHOOK hWinEventHook,
                                          DWORD event,
                                          HWND hwnd,
                                          LONG idObject,
                                          LONG idChild,
                                          DWORD dwEventThread,
                                          DWORD dwmsEventTime) {
  if (!hwnd || idObject != OBJID_WINDOW) {
    return;
  }

  SettingsWindowFinderWin* finder = GetGlobalFinderInstance().get();
  if (!finder) {
    return;
  }

  // WINEVENT_OUTOFCONTEXT hooks are guaranteed by the OS to be dispatched via
  // the message pump of the thread that called SetWinEventHook.
  DCHECK_CALLED_ON_VALID_SEQUENCE(finder->sequence_checker_);

  if (event == EVENT_OBJECT_LOCATIONCHANGE) {
    if (hwnd == finder->observed_hwnd_ && finder->on_resized_) {
      finder->on_resized_.Run();
    }
    return;
  }

  if (!finder->on_found_) {
    return;
  }

  HWND root_hwnd = ::GetAncestor(hwnd, GA_ROOT);
  if (!finder->IsLikelySettingsWindow(root_hwnd)) {
    return;
  }

  // Copy the callback and stop the finder BEFORE executing the callback.
  // This prevents use-after-free if the callback destroys the finder.
  WindowFoundCallback callback = std::move(finder->on_found_);
  finder->Stop();
  std::move(callback).Run(root_hwnd);
}
