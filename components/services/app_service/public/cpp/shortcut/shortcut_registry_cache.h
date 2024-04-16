// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_REGISTRY_CACHE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_REGISTRY_CACHE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

namespace apps {

// A cache that manages and keeps track of all shortcuts on the system.
class COMPONENT_EXPORT(SHORTCUT) ShortcutRegistryCache {
 public:
  class COMPONENT_EXPORT(SHORTCUT) Observer : public base::CheckedObserver {
   public:
    // Called when a shortcut been updated (including added). `update` contains
    // the shortcut updating information to let the clients know which shortcut
    // has been updated and what changes have been made.
    virtual void OnShortcutUpdated(const ShortcutUpdate& update) {}

    // Called when a shortcut represented by `id` been removed from the system.
    virtual void OnShortcutRemoved(const ShortcutId& id) {}

    // Called when the ShortcutRegistryCache object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "cache->RemoveObserver(this)", whether directly or indirectly
    // (e.g. via base::ScopedObservation::Reset).
    virtual void OnShortcutRegistryCacheWillBeDestroyed(
        ShortcutRegistryCache* cache) = 0;
  };

  ShortcutRegistryCache();

  ShortcutRegistryCache(const ShortcutRegistryCache&) = delete;
  ShortcutRegistryCache& operator=(const ShortcutRegistryCache&) = delete;

  ~ShortcutRegistryCache();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Apply the shortcut update `delta` to an existing shortcut, or create a new
  // shortcut if it doesn't exists.
  void UpdateShortcut(ShortcutPtr delta);

  // Removes the shortcut represented by `id` from the cache.
  void RemoveShortcut(const ShortcutId& id);

  // Get the shortcut by the id, return nullptr if shortcut id doesn't exist.
  // Be careful about the lifetime when using this method, the ShortcutView is
  // only valid before the shortcut is removed from the cache. Please do not
  // store this data and always query a fresh one when using it.
  ShortcutView GetShortcut(const ShortcutId& shortcut_id);
  bool HasShortcut(const ShortcutId& shortcut_id);

  // Return a view of all shortcuts.
  // Be careful about the lifetime when using this method, the ShortcutView is
  // only valid before the shortcut is removed from the cache. Please do not
  // store this data and always query a fresh one when using it.
  std::vector<ShortcutView> GetAllShortcuts();

  // Returns the host app id for shortcut represented by 'id'.
  std::string GetShortcutHostAppId(const ShortcutId& id);

  // Returns the local id for shortcut represented by 'id'.
  std::string GetShortcutLocalId(const ShortcutId& id);

 private:
  // Maps from shortcut_id to the latest state: the "sum" of all previous
  // deltas.
  std::map<ShortcutId, ShortcutPtr> states_;

  // If currently an update is processing, we do not the notified observer to
  // update the shortcut cache again.
  // TODO(crbug.com/40255408): Handle observer updates if proved to be
  // necessary.
  bool is_updating_ = false;

  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_REGISTRY_CACHE_H_
