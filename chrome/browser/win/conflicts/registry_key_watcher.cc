// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/registry_key_watcher.h"

#include <windows.h>

#include <utility>

#include "base/bind.h"

// static
std::unique_ptr<RegistryKeyWatcher> RegistryKeyWatcher::Create(
    HKEY root,
    const base::string16& subkey,
    REGSAM wow64access,
    base::OnceClosure on_registry_key_deleted) {
  std::unique_ptr<RegistryKeyWatcher> instance(new RegistryKeyWatcher(
      root, subkey, wow64access, std::move(on_registry_key_deleted)));

  if (!instance->IsWatching())
    return nullptr;

  return instance;
}

RegistryKeyWatcher::~RegistryKeyWatcher() = default;

RegistryKeyWatcher::RegistryKeyWatcher(
    HKEY root,
    const base::string16& subkey,
    REGSAM wow64access,
    base::OnceClosure on_registry_key_deleted)
    : registry_key_(std::make_unique<base::win::RegKey>(
          root,
          subkey.c_str(),
          KEY_NOTIFY | KEY_QUERY_VALUE | wow64access)),
      on_registry_key_deleted_(std::move(on_registry_key_deleted)) {
  if (registry_key_->Valid())
    StartWatching();
}

void RegistryKeyWatcher::StartWatching() {
  if (!registry_key_->StartWatching(base::BindRepeating(
          &RegistryKeyWatcher::OnRegistryKeyChanged, base::Unretained(this)))) {
    registry_key_.reset();
  }
}

bool RegistryKeyWatcher::IsWatching() {
  return registry_key_ && registry_key_->Valid();
}

void RegistryKeyWatcher::OnRegistryKeyChanged() {
  // This callback may be invoked for any modification on the registry key.
  // Since this class cares only about the deletion of the key, read the default
  // value of the key to figure out if it still exists.
  base::string16 value;
  if (registry_key_->ReadValue(nullptr, &value) == ERROR_KEY_DELETED) {
    // The registry key is no longer needed.
    registry_key_.reset();

    // Run the callback last, because it may delete the RegistryKeyWatcher
    // instance during its execution.
    std::move(on_registry_key_deleted_).Run();
    return;
  }

  // Keep watching.
  StartWatching();
}
