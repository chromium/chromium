// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_REGISTRY_KEY_WATCHER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_REGISTRY_KEY_WATCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/win/registry.h"

// This class monitors a registry key to detect if it gets deleted. Delete the
// watcher to stop the monitoring.
class RegistryKeyWatcher {
 public:
  // Creates a watcher that immediately starts monitoring the |subkey|. Returns
  // null if there was an error during the initialization.
  static std::unique_ptr<RegistryKeyWatcher> Create(
      HKEY root,
      const base::string16& subkey,
      REGSAM wow64access,
      base::OnceClosure on_registry_key_deleted);

  ~RegistryKeyWatcher();

 private:
  RegistryKeyWatcher(HKEY root,
                     const base::string16& subkey,
                     REGSAM wow64access,
                     base::OnceClosure on_registry_key_deleted);

  // Starts the monitoring on the registry key.
  void StartWatching();

  // Returns true if the registry key is being watched.
  bool IsWatching();

  // Callback for modifications on the registry key.
  void OnRegistryKeyChanged();

  // The registry key being watched.
  std::unique_ptr<base::win::RegKey> registry_key_;

  // Invoked when the registry key is deleted.
  base::OnceClosure on_registry_key_deleted_;

  DISALLOW_COPY_AND_ASSIGN(RegistryKeyWatcher);
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_REGISTRY_KEY_WATCHER_H_
