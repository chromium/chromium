// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"

#include <algorithm>
#include <vector>

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

InMemoryEntitySuppressionManager::InMemoryEntitySuppressionManager() = default;

InMemoryEntitySuppressionManager::~InMemoryEntitySuppressionManager() = default;

void InMemoryEntitySuppressionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InMemoryEntitySuppressionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool InMemoryEntitySuppressionManager::SuppressEntity(
    const EntityInstance& entity) {
  std::vector<EntitySuppressionEntry> entries =
      GetEntitySuppressionEntries(entity);
  size_t original_size = suppressed_entries_.size();
  suppressed_entries_.insert(std::make_move_iterator(entries.begin()),
                             std::make_move_iterator(entries.end()));
  bool modified = suppressed_entries_.size() > original_size;
  if (modified) {
    observers_.Notify(&Observer::OnEntitySuppressionsChanged);
  }
  return modified;
}

bool InMemoryEntitySuppressionManager::UnsuppressEntity(
    const EntityInstance& entity) {
  std::vector<EntitySuppressionEntry> entries =
      GetEntitySuppressionEntries(entity);
  size_t original_size = suppressed_entries_.size();
  for (const EntitySuppressionEntry& entry : entries) {
    suppressed_entries_.erase(entry);
  }
  bool modified = suppressed_entries_.size() < original_size;
  if (modified) {
    observers_.Notify(&Observer::OnEntitySuppressionsChanged);
  }
  return modified;
}

bool InMemoryEntitySuppressionManager::IsSuppressed(
    const EntityInstance& entity) const {
  std::vector<EntitySuppressionEntry> entries =
      GetEntitySuppressionEntries(entity);
  return std::ranges::any_of(entries,
                             [this](const EntitySuppressionEntry& entry) {
                               return suppressed_entries_.contains(entry);
                             });
}

}  // namespace autofill
