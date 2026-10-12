// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"

#include <algorithm>
#include <vector>

#include "base/containers/map_util.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
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

bool InMemoryEntitySuppressionManager::SuppressEntity(EntityInstance entity) {
  bool modified = false;
  const base::Time now = base::Time::Now();
  for (EntitySuppressionEntry& entry : GetEntitySuppressionEntries(entity)) {
    if (suppressed_entries_.try_emplace(entry, now).second) {
      suppressed_entries_by_entity_id_[entity.guid()].push_back(
          std::move(entry));
      modified = true;
    }
  }
  if (modified) {
    observers_.Notify(&Observer::OnEntitySuppressionsChanged);
  }
  return modified;
}

bool InMemoryEntitySuppressionManager::UndoInSessionSuppressedEntity(
    const EntityInstance::EntityId& entity_id) {
  const std::vector<EntitySuppressionEntry>* entries =
      base::FindOrNull(suppressed_entries_by_entity_id_, entity_id);
  if (!entries) {
    return false;
  }
  size_t original_size = suppressed_entries_.size();
  for (const EntitySuppressionEntry& entry : *entries) {
    suppressed_entries_.erase(entry);
  }
  suppressed_entries_by_entity_id_.erase(entity_id);
  bool modified = suppressed_entries_.size() < original_size;
  if (modified) {
    observers_.Notify(&Observer::OnEntitySuppressionsChanged);
  }
  return modified;
}

bool InMemoryEntitySuppressionManager::ClearSuppressions(
    base::Time delete_begin,
    base::Time delete_end) {
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }
  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  const size_t removed_count = absl::erase_if(
      suppressed_entries_,
      [&](const std::pair<const EntitySuppressionEntry, base::Time>& pair) {
        return pair.second >= delete_begin && pair.second <= delete_end;
      });
  if (removed_count == 0) {
    return false;
  }

  absl::erase_if(suppressed_entries_by_entity_id_,
                 [this](std::pair<const EntityInstance::EntityId,
                                  std::vector<EntitySuppressionEntry>>& pair) {
                   std::erase_if(pair.second,
                                 [this](const EntitySuppressionEntry& entry) {
                                   return !suppressed_entries_.contains(entry);
                                 });
                   return pair.second.empty();
                 });

  observers_.Notify(&Observer::OnEntitySuppressionsChanged);
  return true;
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

base::WeakPtr<syncer::DataTypeControllerDelegate>
InMemoryEntitySuppressionManager::GetSyncControllerDelegate() {
  return nullptr;
}

}  // namespace autofill
