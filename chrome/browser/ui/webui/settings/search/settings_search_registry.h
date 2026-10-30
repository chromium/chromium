// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_REGISTRY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_REGISTRY_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"

namespace settings_search {

class SettingsSearchItem;

// Owns the searchable settings, keyed by SettingId, and notifies observers
// when they change.
class SettingsSearchRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called after items are added, replaced or removed, before any replaced or
    // removed item is destroyed. Observers must not call AddItem() or
    // RemoveItem() from within this call.
    virtual void OnRegistryUpdated() = 0;
  };

  SettingsSearchRegistry();
  SettingsSearchRegistry(const SettingsSearchRegistry&) = delete;
  SettingsSearchRegistry& operator=(const SettingsSearchRegistry&) = delete;
  ~SettingsSearchRegistry();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds `item`, replacing any item with the same id.
  void AddItem(std::unique_ptr<SettingsSearchItem> item);
  // Removes the item with `id`; no-op (and does not notify observers) if
  // absent.
  void RemoveItem(mojom::SettingId id);

  // Returns the items ordered by id. Returned pointers are invalidated when the
  // registry updates.
  std::vector<raw_ptr<SettingsSearchItem>> GetItems() const;

 private:
  void NotifyRegistryUpdated();

  bool notifying_ = false;
  base::flat_map<mojom::SettingId, std::unique_ptr<SettingsSearchItem>> items_;
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_REGISTRY_H_
