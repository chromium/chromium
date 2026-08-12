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
#include "ui/gfx/win/hwnd_util.h"

namespace {

// Events observed on the Settings window once it has been found. Hooked
// individually rather than as a range: the events are not contiguous, and
// [EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE] would also subscribe to every
// EVENT_OBJECT_SHOW in the session.
constexpr DWORD kObservedWindowEvents[] = {
    // The window moved or was resized: re-dock to it.
    EVENT_OBJECT_LOCATIONCHANGE,
    // The user grabbed the title bar or a border. These bracket the run of
    // location changes the drag produces, and distinguish it from the window
    // moving itself.
    EVENT_SYSTEM_MOVESIZESTART,
    EVENT_SYSTEM_MOVESIZEEND,
    // The window went away. Closing a UWP window such as Settings often
    // cloaks or hides its frame rather than destroying it.
    EVENT_OBJECT_DESTROY,
    EVENT_OBJECT_HIDE,
    EVENT_OBJECT_CLOAKED,
};

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

// Installs an out-of-context hook for a single `event`, scoped to `pid` when
// non-zero, and records it in `hooks` when installation succeeds.
void InstallWinEventHook(DWORD event,
                         DWORD pid,
                         WINEVENTPROC callback,
                         std::vector<HWINEVENTHOOK>& hooks) {
  if (HWINEVENTHOOK hook = ::SetWinEventHook(
          event, event, nullptr, callback, pid,
          /*idThread=*/0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)) {
    hooks.push_back(hook);
  }
}

void UnhookAll(std::vector<HWINEVENTHOOK>& hooks) {
  for (HWINEVENTHOOK hook : hooks) {
    ::UnhookWinEvent(hook);
  }
  hooks.clear();
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
  // These hooks cannot be scoped to a process: they exist to discover a window
  // that does not necessarily exist yet, so its owner is unknown. The
  // observation hooks in StartObservingLocationChanges() are scoped.
  winevent_hook_ =
      ::SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr,
                        &SettingsWindowFinderWin::WinEventCallback, 0, 0,
                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  //  Launching Settings window can be transiently DWM-cloaked, which
  //  IsLikelySettingsWindow() rejects. Listen for the uncloak so such a window
  //  is matched the moment it becomes visible.
  uncloak_hook_ =
      ::SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, nullptr,
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

  if (uncloak_hook_) {
    ::UnhookWinEvent(uncloak_hook_);
    uncloak_hook_ = nullptr;
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

  // The observed window is known, so scope the hooks to the process that owns
  // it: a system-wide hook would marshal each matching event in the session
  // onto the browser UI thread only for it to be discarded. Falls back to a
  // system-wide hook if the process cannot be determined.
  DWORD observed_pid = 0;
  ::GetWindowThreadProcessId(settings_hwnd, &observed_pid);

  for (DWORD event : kObservedWindowEvents) {
    InstallWinEventHook(event, observed_pid,
                        &SettingsWindowFinderWin::WinEventCallback,
                        observation_hooks_);
  }

  UpdateGlobalInstance();
}

void SettingsWindowFinderWin::SetMoveSizeCallback(
    WindowMoveSizeCallback on_move_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_move_size_ = std::move(on_move_size);
}

void SettingsWindowFinderWin::StopObservingLocationChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UnhookAll(observation_hooks_);

  observed_hwnd_ = nullptr;
  on_resized_.Reset();
  on_move_size_.Reset();

  UpdateGlobalInstance();
}

void SettingsWindowFinderWin::UpdateGlobalInstance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (winevent_hook_ || uncloak_hook_ || !observation_hooks_.empty()) {
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
  if (!::IsWindow(hwnd) || !::IsWindowVisible(hwnd) ||
      gfx::IsWindowCloaked(hwnd)) {
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
  if (FAILED(::SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&prop_store))) ||
      !prop_store) {
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
  SettingsWindowFinderWin* finder = GetGlobalFinderInstance().get();
  if (!finder) {
    return;
  }

  // WINEVENT_OUTOFCONTEXT hooks are guaranteed by the OS to be dispatched via
  // the message pump of the thread that called SetWinEventHook.
  DCHECK_CALLED_ON_VALID_SEQUENCE(finder->sequence_checker_);

  finder->HandleWinEvent(event, hwnd, idObject);
}

void SettingsWindowFinderWin::HandleWinEvent(DWORD event,
                                             HWND hwnd,
                                             LONG idObject) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hwnd || idObject != OBJID_WINDOW) {
    return;
  }

  if (hwnd == observed_hwnd_ && (event == EVENT_SYSTEM_MOVESIZESTART ||
                                 event == EVENT_SYSTEM_MOVESIZEEND)) {
    if (on_move_size_) {
      // Run a copy: as with on_resized_, the callback may reentrantly stop the
      // observation while the invocation is still on the stack.
      WindowMoveSizeCallback callback = on_move_size_;
      callback.Run(event == EVENT_SYSTEM_MOVESIZESTART);
    }
    return;
  }

  // Events on the observed Settings window: location changes re-dock it, and
  // destroy/hide/cloak transitions let the owner detect that the user closed
  // it (a UWP window is often cloaked or hidden rather than destroyed).
  if (hwnd == observed_hwnd_ && on_resized_ &&
      (event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_OBJECT_DESTROY ||
       event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_CLOAKED)) {
    // Run a copy: the callback may reentrantly stop the observation (e.g.
    // the owner tears down on a destroy/cloak event), resetting on_resized_
    // while the invocation is still on the stack.
    WindowResizedCallback callback = on_resized_;
    callback.Run();
    return;
  }

  if (event != EVENT_OBJECT_CREATE && event != EVENT_OBJECT_SHOW &&
      event != EVENT_OBJECT_UNCLOAKED) {
    return;
  }

  if (!on_found_) {
    return;
  }

  HWND root_hwnd = GetRootWindow(hwnd);
  if (!IsLikelySettingsWindow(root_hwnd)) {
    return;
  }

  // Copy the callback and stop the finder BEFORE executing the callback.
  // This prevents use-after-free if the callback destroys the finder.
  WindowFoundCallback callback = std::move(on_found_);
  Stop();
  std::move(callback).Run(root_hwnd);
}

HWND SettingsWindowFinderWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}
