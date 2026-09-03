// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/sequence_checker.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

EntitySuppressionSyncBridge::EntitySuppressionSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  // TODO(crbug.com/501036619): Initialize DataTypeStore and call
  // ModelReadyToSync().
}

EntitySuppressionSyncBridge::~EntitySuppressionSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
EntitySuppressionSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return nullptr;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::nullopt;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::nullopt;
}

void EntitySuppressionSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
}

std::unique_ptr<syncer::DataBatch>
EntitySuppressionSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::make_unique<syncer::MutableDataBatch>();
}

std::unique_ptr<syncer::DataBatch>
EntitySuppressionSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::make_unique<syncer::MutableDataBatch>();
}

std::string EntitySuppressionSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string EntitySuppressionSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.autofill_entity_suppression().guid();
}

bool EntitySuppressionSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_entity_suppression() &&
         !entity_data.specifics.autofill_entity_suppression().guid().empty();
}

sync_pb::EntitySpecifics
EntitySuppressionSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  return sync_pb::EntitySpecifics();
}

}  // namespace autofill
