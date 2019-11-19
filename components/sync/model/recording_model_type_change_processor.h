// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

// Augmented FakeModelTypeChangeProcessor that accumulates all instructions in
// members that can then be accessed for verification.
class RecordingModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  RecordingModelTypeChangeProcessor();
  ~RecordingModelTypeChangeProcessor() override;

  // FakeModelTypeChangeProcessor overrides.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_changes) override;
  void Delete(const std::string& storage_key,
              MetadataChangeList* metadata_changes) override;
  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override;
  void UntrackEntityForStorageKey(const std::string& storage_key) override;
  void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() override;
  std::string TrackedAccountId() override;

  const std::multimap<std::string, std::unique_ptr<EntityData>>& put_multimap()
      const {
    return put_multimap_;
  }

  const std::multimap<std::string, std::unique_ptr<EntityData>>&
  update_multimap() const {
    return update_multimap_;
  }

  const std::set<std::string>& delete_set() const { return delete_set_; }

  const std::set<std::string>& untrack_for_storage_key_set() const {
    return untrack_for_storage_key_set_;
  }

  const std::set<ClientTagHash>& untrack_for_client_tag_hash_set() const {
    return untrack_for_client_tag_hash_set_;
  }

  MetadataBatch* metadata() const { return metadata_.get(); }

  // Constructs the processor and assigns its raw pointer to the given address.
  // This way, the tests can keep the raw pointer for verifying expectations
  // while the unique pointer is owned by the bridge being tested.
  static std::unique_ptr<ModelTypeChangeProcessor>
  CreateProcessorAndAssignRawPointer(
      RecordingModelTypeChangeProcessor** processor_address);

 private:
  std::multimap<std::string, std::unique_ptr<EntityData>> put_multimap_;
  std::multimap<std::string, std::unique_ptr<EntityData>> update_multimap_;
  std::set<std::string> delete_set_;
  std::set<std::string> untrack_for_storage_key_set_;
  std::set<ClientTagHash> untrack_for_client_tag_hash_set_;
  std::unique_ptr<MetadataBatch> metadata_;
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
