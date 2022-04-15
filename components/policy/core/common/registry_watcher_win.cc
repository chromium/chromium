// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_watcher_win.h"

#include <algorithm>

#include "base/logging.h"
#include "base/win/registry.h"

namespace policy {

const wchar_t kKeyRegistryDynamicRefreshEnabled[] =
    L"RegistryDynamicRefreshEnabled";

namespace {

// Returns true if the dynamic refresh of policies from the Registry should be
// blocked.
bool ShouldBlockDynamicRefreshFromRegistry(
    const std::wstring& key_path_str_to_watch,
    bool is_dev_registry_key_supported) {
  // This registry key is only for development.
  if (!is_dev_registry_key_supported)
    return false;

  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, key_path_str_to_watch.c_str(),
               KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return false;
  }
  DWORD value = 0;
  if (key.ReadValueDW(kKeyRegistryDynamicRefreshEnabled, &value) !=
      ERROR_SUCCESS) {
    return false;
  }
  return value == 0;
}

}  // namespace

// static
absl::optional<RegistryWatcherWin> RegistryWatcherWin::MaybeCreate(
    const std::wstring& key_path_str_to_watch,
    bool is_dev_registry_key_supported) {
  if (ShouldBlockDynamicRefreshFromRegistry(key_path_str_to_watch,
                                            is_dev_registry_key_supported)) {
    return absl::nullopt;
  }
  return absl::optional<RegistryWatcherWin>(key_path_str_to_watch);
}

RegistryWatcherWin::RegistryWatcherWin(
    const std::wstring& key_path_str_to_watch)
    : key_path_str_to_watch_(key_path_str_to_watch) {}

RegistryWatcherWin::~RegistryWatcherWin() = default;

void RegistryWatcherWin::StartWatching(base::RepeatingClosure callback) {
  // No-op if already initialized.
  if (!keys_to_watch_.empty())
    return;

  callback_ = std::move(callback);

  static const HKEY kHives[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (HKEY hive : kHives)
    AddKeyToWatchList(hive, key_path_str_to_watch_);
}

void RegistryWatcherWin::AddKeyToWatchList(HKEY rootkey,
                                           const std::wstring& subkey) {
  auto key = std::make_unique<base::win::RegKey>();
  if (key->Open(rootkey, subkey.c_str(), KEY_NOTIFY) != ERROR_SUCCESS)
    return;

  if (!key->StartWatching(base::BindOnce(&RegistryWatcherWin::OnRegistryChanged,
                                         base::Unretained(this),
                                         base::Unretained(key.get())))) {
    DLOG(WARNING) << "Failed to start watch for registry change event";
    return;
  }

  keys_to_watch_.push_back(std::move(key));
}

void RegistryWatcherWin::OnRegistryChanged(base::win::RegKey* key) {
  // Keep watching the registry key.
  if (!key->StartWatching(base::BindOnce(&RegistryWatcherWin::OnRegistryChanged,
                                         base::Unretained(this),
                                         base::Unretained(key)))) {
    DLOG(WARNING) << "Failed to keep watching for registry change event";
    keys_to_watch_.erase(
        std::remove_if(keys_to_watch_.begin(), keys_to_watch_.end(),
                       [key](const std::unique_ptr<base::win::RegKey>& ptr) {
                         return ptr.get() == key;
                       }));
  }
  callback_.Run();
}

}  // namespace policy
