// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SYNC_METADATA_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SYNC_METADATA_STORE_H_

#include <memory>
#include <string>

#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/model/metadata_batch.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordSyncMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  MockPasswordSyncMetadataStore();
  ~MockPasswordSyncMetadataStore() override;

  // syncer::SyncMetadataStore:
  MOCK_METHOD(bool,
              UpdateSyncMetadata,
              (syncer::ModelType model_type,
               const std::string& storage_key,
               const sync_pb::EntityMetadata& metadata));
  MOCK_METHOD(bool,
              ClearSyncMetadata,
              (syncer::ModelType model_type, const std::string& storage_key));
  MOCK_METHOD(bool,
              UpdateModelTypeState,
              (syncer::ModelType model_type,
               const sync_pb::ModelTypeState& model_type_state));
  MOCK_METHOD(bool, ClearModelTypeState, (syncer::ModelType model_type));

  // PasswordStoreSync::MetadataStore:
  MOCK_METHOD(std::unique_ptr<syncer::MetadataBatch>,
              GetAllSyncMetadata,
              (syncer::ModelType model_type));
  MOCK_METHOD(void, DeleteAllSyncMetadata, (syncer::ModelType model_type));
  MOCK_METHOD(void,
              SetDeletionsHaveSyncedCallback,
              (syncer::ModelType model_type,
               base::RepeatingCallback<void(bool)> callback));
  MOCK_METHOD(bool, HasUnsyncedDeletions, (syncer::ModelType model_type));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SYNC_METADATA_STORE_H_
