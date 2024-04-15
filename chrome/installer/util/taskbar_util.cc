// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/taskbar_util.h"

#include <objbase.h>

#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/files/file_path.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/install_static/install_util.h"

namespace {

enum class PinnedListModifyCaller { kExplorer = 4 };

constexpr GUID CLSID_TaskbandPin = {
    0x90aa3a4e,
    0x1cba,
    0x4233,
    {0xb8, 0xbb, 0x53, 0x57, 0x73, 0xd4, 0x84, 0x49}};

constexpr wchar_t kInstallerPinned[] = L"InstallerPinned";

// Set by tests to control whether or not pinning to taskbar is allowed.
CanPinToTaskBarDelegateFunctionPtr g_can_pin_to_taskbar;

// Undocumented COM interface for manipulating taskbar pinned list.
class __declspec(uuid("0DD79AE2-D156-45D4-9EEB-3B549769E940")) IPinnedList3
    : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE EnumObjects() = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPinnableInfo() = 0;
  virtual HRESULT STDMETHODCALLTYPE IsPinnable() = 0;
  virtual HRESULT STDMETHODCALLTYPE Resolve() = 0;
  virtual HRESULT STDMETHODCALLTYPE LegacyModify() = 0;
  virtual HRESULT STDMETHODCALLTYPE GetChangeCount() = 0;
  virtual HRESULT STDMETHODCALLTYPE IsPinned(PCIDLIST_ABSOLUTE) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPinnedItem() = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAppIDForPinnedItem() = 0;
  virtual HRESULT STDMETHODCALLTYPE ItemChangeNotify() = 0;
  virtual HRESULT STDMETHODCALLTYPE UpdateForRemovedItemsAsNecessary() = 0;
  virtual HRESULT STDMETHODCALLTYPE PinShellLink() = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPinnedItemForAppID() = 0;
  virtual HRESULT STDMETHODCALLTYPE Modify(PCIDLIST_ABSOLUTE unpin,
                                           PCIDLIST_ABSOLUTE pin,
                                           PinnedListModifyCaller caller) = 0;
};

// ScopedPIDLFromPath class, and the idea of using IPinnedList3::Modify,
// are thanks to Gee Law <https://geelaw.blog/entries/msedge-pins/>
class ScopedPIDLFromPath {
 public:
  explicit ScopedPIDLFromPath(PCWSTR path)
      : p_id_list_(ILCreateFromPath(path)) {}
  ~ScopedPIDLFromPath() {
    if (p_id_list_)
      ILFree(p_id_list_);
  }
  PIDLIST_ABSOLUTE Get() const { return p_id_list_; }

 private:
  PIDLIST_ABSOLUTE const p_id_list_;
};

// Returns the taskbar pinned list if successful, an empty ComPtr otherwise.
Microsoft::WRL::ComPtr<IPinnedList3> GetTaskbarPinnedList() {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS5)
    return nullptr;

  Microsoft::WRL::ComPtr<IPinnedList3> pinned_list;
  if (FAILED(CoCreateInstance(CLSID_TaskbandPin, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&pinned_list)))) {
    return nullptr;
  }

  return pinned_list;
}

// Use IPinnedList3 to pin shortcut to taskbar on WIN10_RS5 and above.
// Returns true if pinning was successful.
// static
bool PinShortcutWithIPinnedList3(const base::FilePath& shortcut) {
  Microsoft::WRL::ComPtr<IPinnedList3> pinned_list = GetTaskbarPinnedList();
  if (!pinned_list)
    return false;

  ScopedPIDLFromPath item_id_list(shortcut.value().data());
  HRESULT hr = pinned_list->Modify(nullptr, item_id_list.Get(),
                                   PinnedListModifyCaller::kExplorer);
  return SUCCEEDED(hr);
}

// Use IPinnedList3 to unpin shortcut to taskbar on WIN10_RS5 and above.
// Returns true if unpinning was successful.
// static
bool UnpinShortcutWithIPinnedList3(const base::FilePath& shortcut) {
  Microsoft::WRL::ComPtr<IPinnedList3> pinned_list = GetTaskbarPinnedList();
  if (!pinned_list)
    return false;

  ScopedPIDLFromPath item_id_list(shortcut.value().data());
  HRESULT hr = pinned_list->Modify(item_id_list.Get(), nullptr,
                                   PinnedListModifyCaller::kExplorer);
  return SUCCEEDED(hr);
}

}  // namespace

bool CanPinShortcutToTaskbar() {
  // "Pin to taskbar" isn't directly supported in Windows 10, but WIN10_RS5 has
  // some undocumented interfaces to do pinning.
  return base::win::GetVersion() >= base::win::Version::WIN10_RS5;
}

bool PinShortcutToTaskbar(const base::FilePath& shortcut) {
  if (g_can_pin_to_taskbar && !(*g_can_pin_to_taskbar)()) {
    return false;
  }
  return PinShortcutWithIPinnedList3(shortcut);
}

bool UnpinShortcutFromTaskbar(const base::FilePath& shortcut) {
  return UnpinShortcutWithIPinnedList3(shortcut);
}

std::optional<bool> IsShortcutPinnedToTaskbar(const base::FilePath& shortcut) {
  Microsoft::WRL::ComPtr<IPinnedList3> pinned_list = GetTaskbarPinnedList();
  if (!pinned_list.Get())
    return std::nullopt;

  ScopedPIDLFromPath item_id_list(shortcut.value().data());
  HRESULT hr = pinned_list->IsPinned(item_id_list.Get());
  // S_OK means `shortcut` is pinned, S_FALSE means it's not pinned.
  return SUCCEEDED(hr) ? std::optional<bool>(hr == S_OK) : std::nullopt;
}

void SetCanPinToTaskbarDelegate(CanPinToTaskBarDelegateFunctionPtr delegate) {
  g_can_pin_to_taskbar = delegate;
}

bool SetInstallerPinnedChromeToTaskbar(bool installed) {
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER, install_static::GetRegistryPath().c_str(),
                 KEY_SET_VALUE) == ERROR_SUCCESS) {
    return key.WriteValue(kInstallerPinned, installed ? 1 : 0) == ERROR_SUCCESS;
  }
  return false;
}

std::optional<bool> GetInstallerPinnedChromeToTaskbar() {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, install_static::GetRegistryPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD installer_pinned = 0;
    LONG result = key.ReadValueDW(kInstallerPinned, &installer_pinned);
    if (result == ERROR_SUCCESS)
      return installer_pinned != 0;
  }
  return std::nullopt;
}
