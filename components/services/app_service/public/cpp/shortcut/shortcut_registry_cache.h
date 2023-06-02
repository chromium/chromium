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
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

namespace apps {

// A cache that manages and keeps track of all shortcuts on the system.
class COMPONENT_EXPORT(SHORTCUT) ShortcutRegistryCache {
 public:
  // A view class to reduce the risk of lifetime issues by preventing preventing
  // long-term storage on the heap.
  class ShortcutView {
   public:
    explicit ShortcutView(const Shortcut* shortcut) : shortcut_(shortcut) {}
    const Shortcut* operator->() const { return shortcut_.get(); }
    explicit operator bool() const { return shortcut_; }

   private:
    const raw_ptr<const Shortcut> shortcut_;
    STACK_ALLOCATED();
  };

  class COMPONENT_EXPORT(SHORTCUT) Observer : public base::CheckedObserver {
   public:
    // Called when a shortcut been updated (including added). `update` contains
    // the shortcut updating information to let the clients know which shortcut
    // has been updated and what changes have been made.
    virtual void OnShortcutUpdated(const ShortcutUpdate& update) {}

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

  // TODO(crbug.com/1412708): Add remove flow.

  // Get the shortcut by the id, return nullptr if shortcut id doesn't exist.
  // Be careful about the lifetime when using this method, the ShortcutView is
  // only valid before the shortcut is removed from the cache. Please do not
  // store this data and always query a fresh one when using it.
  ShortcutView GetShortcut(const ShortcutId& shortcut_id);
  bool HasShortcut(const ShortcutId& shortcut_id);

  // Return a copy of all shortcuts.
  std::vector<ShortcutPtr> GetAllShortcuts();

 private:
  // Maps from shortcut_id to the latest state: the "sum" of all previous
  // deltas.
  std::map<ShortcutId, ShortcutPtr> states_;

  // If currently an update is processing, we do not the notified observer to
  // update the shortcut cache again.
  // TODO(crbug.com/1412708): Handle observer updates if proved to be necessary.
  bool is_updating_ = false;

  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};
using ShortcutView = ShortcutRegistryCache::ShortcutView;

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_REGISTRY_CACHE_H_
