// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REGISTRY_WATCHER_WIN_H_
#define COMPONENTS_POLICY_CORE_COMMON_REGISTRY_WATCHER_WIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

namespace policy {

// This class monitors registry subkeys below both HKEY_LOCAL_MACHINE and
// HKEY_CURRENT_USER to detect if any of its values have changed.
class POLICY_EXPORT RegistryWatcherWin {
 public:
  // May returns null if the dynamic refresh of policies from the Registry is
  // blocked. If not null, this instance will observe changes of
  // |key_path_str_to_watch| in both HKLM and HKCU.
  static absl::optional<RegistryWatcherWin> MaybeCreate(
      const std::wstring& key_path_str_to_watch,
      bool is_dev_registry_key_supported);

  explicit RegistryWatcherWin(const std::wstring& key_path_str_to_watch);
  RegistryWatcherWin(const RegistryWatcherWin&) = delete;
  RegistryWatcherWin& operator=(const RegistryWatcherWin&) = delete;
  ~RegistryWatcherWin();

  // Installs the watchers for the Registry value changes.
  void StartWatching(base::RepeatingClosure callback);

 private:
  // Creates a new key and appends it to |keys_to_watch_|. If the key fails to
  // be created, it is not appended to the list.
  void AddKeyToWatchList(HKEY rootkey, const std::wstring& subkey);

  // Called when the Registry value of |key| is changed.
  void OnRegistryChanged(base::win::RegKey* key);

  const std::wstring key_path_str_to_watch_;
  base::RepeatingClosure callback_;
  std::vector<std::unique_ptr<base::win::RegKey>> keys_to_watch_;
};

// Exposed for testing.
extern const POLICY_EXPORT wchar_t kKeyRegistryDynamicRefreshEnabled[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REGISTRY_WATCHER_WIN_H_
