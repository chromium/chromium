// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FORWARDING_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_FORWARDING_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/model/data_type_local_change_processor.h"

namespace syncer {

// A DataTypeLocalChangeProcessor implementation that forwards all calls to
// another DataTypeLocalChangeProcessor instance, useful when a client wants
// ownership of the processor.
class ForwardingDataTypeLocalChangeProcessor
    : public DataTypeLocalChangeProcessor {
 public:
  // |other| must not be nullptr and must outlive this object.
  explicit ForwardingDataTypeLocalChangeProcessor(
      DataTypeLocalChangeProcessor* other);
  ~ForwardingDataTypeLocalChangeProcessor() override;

  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              const DeletionOrigin& origin,
              MetadataChangeList* metadata_change_list) override;
  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override;
  void UntrackEntityForStorageKey(const std::string& storage_key) override;
  void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) override;
  std::vector<std::string> GetAllTrackedStorageKeys() const override;
  bool IsEntityUnsynced(const std::string& storage_key) const override;
  base::Time GetEntityCreationTime(
      const std::string& storage_key) const override;
  base::Time GetEntityModificationTime(
      const std::string& storage_key) const override;
  void OnModelStarting(DataTypeSyncBridge* bridge) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() const override;
  std::string TrackedAccountId() const override;
  std::string TrackedCacheGuid() const override;
  void ReportError(const ModelError& error) override;
  std::optional<ModelError> GetError() const override;
  base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() override;
  const sync_pb::EntitySpecifics& GetPossiblyTrimmedRemoteSpecifics(
      const std::string& storage_key) const override;
  sync_pb::UniquePosition UniquePositionAfter(
      const std::string& storage_key_before,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionBefore(
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionBetween(
      const std::string& storage_key_before,
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionForInitialEntity(
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition GetUniquePositionForStorageKey(
      const std::string& storage_key) const override;
  base::WeakPtr<DataTypeLocalChangeProcessor> GetWeakPtr() override;

 private:
  const raw_ptr<DataTypeLocalChangeProcessor> other_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FORWARDING_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
