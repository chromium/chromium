// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/registry_watcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/win/registry.h"

RegistryWatcher::RegistryWatcher(const std::vector<std::wstring>& key_paths,
                                 base::OnceClosure on_change_callback)
    : on_change_callback_(std::move(on_change_callback)) {
  CHECK(on_change_callback_);

  for (const auto& path : key_paths) {
    auto reg_key = std::make_unique<base::win::RegKey>(
        HKEY_CURRENT_USER, path.c_str(), KEY_NOTIFY);

    if (!reg_key->Valid()) {
      continue;
    }

    // base::Unretained is safe here because this RegistryWatcher owns the
    // `reg_key`. The subscription will be implicitly cancelled when `reg_key`
    // is destroyed as part of this object's destruction, preventing the
    // callback from being run on a dangling pointer.
    if (!reg_key->StartWatching(base::BindOnce(
            &RegistryWatcher::OnRegistryKeyChanged, base::Unretained(this)))) {
      continue;
    }

    registry_key_watchers_.push_back(std::move(reg_key));
  }
}

RegistryWatcher::~RegistryWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

size_t RegistryWatcher::GetRegistryKeyCount() const {
  return registry_key_watchers_.size();
}

void RegistryWatcher::OnRegistryKeyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop watching all other keys to prevent multiple callbacks.
  registry_key_watchers_.clear();

  // Run the callback and let the owner decide what to do next.
  std::move(on_change_callback_).Run();
}
