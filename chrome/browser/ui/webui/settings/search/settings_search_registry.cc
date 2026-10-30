// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"

namespace settings_search {

SettingsSearchRegistry::SettingsSearchRegistry() = default;

SettingsSearchRegistry::~SettingsSearchRegistry() = default;

void SettingsSearchRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SettingsSearchRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SettingsSearchRegistry::AddItem(std::unique_ptr<SettingsSearchItem> item) {
  CHECK(item);
  CHECK(!notifying_);
  const mojom::SettingId id = item->id();
  // Keep a replaced item alive until observers have stopped using it.
  std::unique_ptr<SettingsSearchItem> old_item =
      std::exchange(items_[id], std::move(item));
  NotifyRegistryUpdated();
}

void SettingsSearchRegistry::RemoveItem(mojom::SettingId id) {
  CHECK(!notifying_);
  auto it = items_.find(id);
  if (it == items_.end()) {
    return;
  }
  // Keep the removed item alive until observers have stopped using it.
  std::unique_ptr<SettingsSearchItem> old_item = std::move(it->second);
  items_.erase(it);
  NotifyRegistryUpdated();
}

std::vector<raw_ptr<SettingsSearchItem>> SettingsSearchRegistry::GetItems()
    const {
  std::vector<raw_ptr<SettingsSearchItem>> result;
  result.reserve(items_.size());
  for (const auto& [id, item] : items_) {
    result.push_back(item.get());
  }
  return result;
}

void SettingsSearchRegistry::NotifyRegistryUpdated() {
  base::AutoReset<bool> notifying(&notifying_, true);
  for (Observer& observer : observers_) {
    observer.OnRegistryUpdated();
  }
}

}  // namespace settings_search
