// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"

namespace autofill {

PersonalContextEntityMetadataManager::PersonalContextEntityMetadataManager(
    std::unique_ptr<PersonalContextEntityMetadataSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  CHECK(sync_bridge_);
  bridge_observation_.Observe(sync_bridge_.get());
}

PersonalContextEntityMetadataManager::~PersonalContextEntityMetadataManager() =
    default;

void PersonalContextEntityMetadataManager::AddObserver(
    PersonalContextEntityMetadataManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalContextEntityMetadataManager::RemoveObserver(
    PersonalContextEntityMetadataManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PersonalContextEntityMetadataManager::SuppressEntity(
    EntityInstance entity) {
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

bool PersonalContextEntityMetadataManager::UndoInSessionSuppressedEntity(
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

bool PersonalContextEntityMetadataManager::ConfirmEntity(
    const EntityInstance& entity) {
  CHECK_EQ(entity.record_type(), EntityInstance::RecordType::kPersonalContext);
  CHECK(entity.IsMaskedEntity());
  if (!sync_bridge_->IsLoaded()) {
    return false;
  }
  std::optional<EntityConfirmationHash> hash =
      GetEntityConfirmationHash(entity);
  return hash.has_value() && sync_bridge_->Confirm(*hash);
}

bool PersonalContextEntityMetadataManager::ClearEntries(base::Time delete_begin,
                                                        base::Time delete_end) {
  if (!sync_bridge_->IsLoaded()) {
    return false;
  }
  if (!sync_bridge_->ClearEntries(delete_begin, delete_end)) {
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

bool PersonalContextEntityMetadataManager::IsSuppressed(
    const EntityInstance& entity) const {
  return std::ranges::any_of(GetEntitySuppressionEntries(entity),
                             [this](const EntitySuppressionEntry& entry) {
                               return sync_bridge_->IsSuppressed(entry);
                             });
}

bool PersonalContextEntityMetadataManager::IsConfirmed(
    const EntityInstance& entity) const {
  CHECK_EQ(entity.record_type(), EntityInstance::RecordType::kPersonalContext);
  CHECK(entity.IsMaskedEntity());
  std::optional<EntityConfirmationHash> hash =
      GetEntityConfirmationHash(entity);
  return hash.has_value() && sync_bridge_->IsConfirmed(*hash);
}

size_t PersonalContextEntityMetadataManager::CountEntriesCreatedBetween(
    base::Time begin,
    base::Time end) const {
  if (!sync_bridge_->IsLoaded()) {
    return 0;
  }
  return sync_bridge_->CountEntriesCreatedBetween(begin, end);
}

void PersonalContextEntityMetadataManager::OnSuppressionsChanged() {
  observers_.Notify(&PersonalContextEntityMetadataManager::Observer::
                        OnEntitySuppressionsChanged);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
PersonalContextEntityMetadataManager::GetSyncControllerDelegate() {
  return sync_bridge_->GetControllerDelegate();
}

}  // namespace autofill
