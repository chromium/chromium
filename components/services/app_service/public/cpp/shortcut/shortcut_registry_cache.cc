// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

namespace apps {

ShortcutRegistryCache::ShortcutRegistryCache() = default;

ShortcutRegistryCache::~ShortcutRegistryCache() {
  for (auto& obs : observers_) {
    obs.OnShortcutRegistryCacheWillBeDestroyed(this);
  }
  CHECK(observers_.empty());
}

void ShortcutRegistryCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShortcutRegistryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ShortcutRegistryCache::UpdateShortcut(ShortcutPtr delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do not allow notified observer to modify shortcut cache again.
  DCHECK(!is_updating_);
  is_updating_ = true;
  const ShortcutId shortcut_id = delta->shortcut_id;

  Shortcut* state =
      HasShortcut(shortcut_id) ? states_[shortcut_id].get() : nullptr;

  ShortcutPtr state_before_update = state ? state->Clone() : nullptr;

  if (state) {
    ShortcutUpdate::Merge(state, delta.get());
  } else {
    // Call ShortcutUpdate::Merge to set the init value for the icon key's
    // `update_version`.
    auto shortcut =
        std::make_unique<Shortcut>(delta->host_app_id, delta->local_id);
    ShortcutUpdate::Merge(shortcut.get(), delta.get());
    states_.emplace(shortcut_id, std::move(shortcut));
  }

  for (auto& obs : observers_) {
    obs.OnShortcutUpdated(
        ShortcutUpdate(state_before_update.get(), delta.get()));
  }

  is_updating_ = false;
}

void ShortcutRegistryCache::RemoveShortcut(const ShortcutId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do not allow notified observer to modify shortcut cache again.
  CHECK(!is_updating_);
  is_updating_ = true;
  states_.erase(id);
  for (auto& obs : observers_) {
    obs.OnShortcutRemoved(id);
  }
  is_updating_ = false;
}

ShortcutView ShortcutRegistryCache::GetShortcut(const ShortcutId& shortcut_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto shortcut = states_.find(shortcut_id);
  return ShortcutView((shortcut != states_.end()) ? shortcut->second.get()
                                                  : nullptr);
}

bool ShortcutRegistryCache::HasShortcut(const ShortcutId& shortcut_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(states_, shortcut_id);
}

std::vector<ShortcutView> ShortcutRegistryCache::GetAllShortcuts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ShortcutView> shortcuts;
  for (const auto& [shortcut_id, shortcut] : states_) {
    shortcuts.emplace_back(shortcut.get());
  }
  return shortcuts;
}

// Returns the host app id for shortcut represented by 'id'.
std::string ShortcutRegistryCache::GetShortcutHostAppId(const ShortcutId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShortcutView shortcut = GetShortcut(id);
  if (!shortcut) {
    return "";
  }
  return shortcut->host_app_id;
}

// Returns the local id for shortcut represented by 'id'.
std::string ShortcutRegistryCache::GetShortcutLocalId(const ShortcutId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShortcutView shortcut = GetShortcut(id);
  if (!shortcut) {
    return "";
  }
  return shortcut->local_id;
}

}  // namespace apps
