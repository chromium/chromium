// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FORWARDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_FORWARDING_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/model/model_type_change_processor.h"

namespace syncer {

// A ModelTypeChangeProcessor implementation that forwards all calls to another
// ModelTypeChangeProcessor instance, useful when a client wants ownership of
// the processor.
class ForwardingModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  // |other| must not be nullptr and must outlive this object.
  explicit ForwardingModelTypeChangeProcessor(ModelTypeChangeProcessor* other);
  ~ForwardingModelTypeChangeProcessor() override;

  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override;
  void UntrackEntityForStorageKey(const std::string& storage_key) override;
  void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) override;
  std::vector<std::string> GetAllTrackedStorageKeys() const override;
  bool IsEntityUnsynced(const std::string& storage_key) override;
  base::Time GetEntityCreationTime(
      const std::string& storage_key) const override;
  base::Time GetEntityModificationTime(
      const std::string& storage_key) const override;
  void OnModelStarting(ModelTypeSyncBridge* bridge) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() const override;
  std::string TrackedAccountId() const override;
  std::string TrackedCacheGuid() const override;
  void ReportError(const ModelError& error) override;
  absl::optional<ModelError> GetError() const override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override;
  const sync_pb::EntitySpecifics& GetPossiblyTrimmedRemoteSpecifics(
      const std::string& storage_key) const override;
  base::WeakPtr<ModelTypeChangeProcessor> GetWeakPtr() override;

 private:
  const raw_ptr<ModelTypeChangeProcessor> other_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FORWARDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
