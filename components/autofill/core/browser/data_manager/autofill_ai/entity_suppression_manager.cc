// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

EntitySuppressionManager::EntitySuppressionManager(
    std::unique_ptr<EntitySuppressionSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  CHECK(sync_bridge_);
  bridge_observation_.Observe(sync_bridge_.get());
}

EntitySuppressionManager::~EntitySuppressionManager() = default;

void EntitySuppressionManager::AddObserver(
    EntitySuppressionManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void EntitySuppressionManager::RemoveObserver(
    EntitySuppressionManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool EntitySuppressionManager::SuppressEntity(EntityInstance entity) {
  if (!sync_bridge_->IsLoaded()) {
    return false;
  }
  bool modified = false;
  for (EntitySuppressionEntry& entry : GetEntitySuppressionEntries(entity)) {
    if (sync_bridge_->Suppress(entry)) {
      suppressed_entries_by_entity_id_[entity.guid()].push_back(
          std::move(entry));
      modified = true;
    }
  }
  return modified;
}

bool EntitySuppressionManager::UndoInSessionSuppressedEntity(
    const EntityInstance::EntityId& entity_id) {
  if (!sync_bridge_->IsLoaded()) {
    return false;
  }
  const std::vector<EntitySuppressionEntry>* entries =
      base::FindOrNull(suppressed_entries_by_entity_id_, entity_id);
  if (!entries) {
    return false;
  }
  bool modified = false;
  for (const EntitySuppressionEntry& entry : *entries) {
    if (sync_bridge_->Unsuppress(entry)) {
      modified = true;
    }
  }
  suppressed_entries_by_entity_id_.erase(entity_id);
  return modified;
}

bool EntitySuppressionManager::ClearSuppressions(base::Time delete_begin,
                                                 base::Time delete_end) {
  if (!sync_bridge_->IsLoaded()) {
    return false;
  }
  if (!sync_bridge_->ClearSuppressions(delete_begin, delete_end)) {
    return false;
  }
  absl::erase_if(suppressed_entries_by_entity_id_,
                 [this](std::pair<const EntityInstance::EntityId,
                                  std::vector<EntitySuppressionEntry>>& pair) {
                   std::erase_if(pair.second,
                                 [this](const EntitySuppressionEntry& entry) {
                                   return !sync_bridge_->IsSuppressed(entry);
                                 });
                   return pair.second.empty();
                 });
  return true;
}

bool EntitySuppressionManager::IsSuppressed(
    const EntityInstance& entity) const {
  return std::ranges::any_of(GetEntitySuppressionEntries(entity),
                             [this](const EntitySuppressionEntry& entry) {
                               return sync_bridge_->IsSuppressed(entry);
                             });
}

size_t EntitySuppressionManager::CountSuppressionsCreatedBetween(
    base::Time begin,
    base::Time end) const {
  if (!sync_bridge_->IsLoaded()) {
    return 0;
  }
  return sync_bridge_->CountSuppressionsCreatedBetween(begin, end);
}

void EntitySuppressionManager::OnSuppressionsChanged() {
  observers_.Notify(
      &EntitySuppressionManager::Observer::OnEntitySuppressionsChanged);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EntitySuppressionManager::GetSyncControllerDelegate() {
  return sync_bridge_->GetControllerDelegate();
}

}  // namespace autofill
