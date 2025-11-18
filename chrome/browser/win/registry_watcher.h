// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_REGISTRY_WATCHER_H_
#define CHROME_BROWSER_WIN_REGISTRY_WATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"

namespace base::win {
class RegKey;
}  // namespace base::win

// A helper that monitors a set of registry keys for changes. The owner of this
// object is responsible for its lifetime. When any of the watched keys change,
// the provided callback is run ONCE and the watcher stops monitoring.
class RegistryWatcher {
 public:
  // Constructs a watcher that monitors a `key_paths` under HKEY_CURRENT_USER.
  // `on_change_callback` is run ONCE when any of the keys change.
  RegistryWatcher(const std::vector<std::wstring>& key_paths,
                  base::OnceClosure on_change_callback);
  ~RegistryWatcher();

  RegistryWatcher(const RegistryWatcher&) = delete;
  RegistryWatcher& operator=(const RegistryWatcher&) = delete;

  // Returns the number of key that this watcher is watching.
  size_t GetRegistryKeyCount() const;

 private:
  // Called when a change is detected on one of the registry keys.
  void OnRegistryKeyChanged();

  // The callback to notify when a key changes.
  base::OnceClosure on_change_callback_;

  // Stores the watchers for each registry key.
  std::vector<std::unique_ptr<base::win::RegKey>> registry_key_watchers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_WIN_REGISTRY_WATCHER_H_
