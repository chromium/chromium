// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  MockModelTypeChangeProcessor();
  ~MockModelTypeChangeProcessor() override;

  MOCK_METHOD3(Put,
               void(const std::string& storage_key,
                    std::unique_ptr<EntityData> entity_data,
                    MetadataChangeList* metadata_change_list));
  MOCK_METHOD2(Delete,
               void(const std::string& storage_key,
                    MetadataChangeList* metadata_change_list));
  MOCK_METHOD3(UpdateStorageKey,
               void(const EntityData& entity_data,
                    const std::string& storage_key,
                    MetadataChangeList* metadata_change_list));
  MOCK_METHOD1(UntrackEntityForStorageKey,
               void(const std::string& storage_key));
  MOCK_METHOD1(UntrackEntityForClientTagHash,
               void(const ClientTagHash& client_tag_hash));
  MOCK_METHOD1(IsEntityUnsynced, bool(const std::string& storage_key));
  MOCK_CONST_METHOD1(GetEntityCreationTime,
                     base::Time(const std::string& storage_key));
  MOCK_CONST_METHOD1(GetEntityModificationTime,
                     base::Time(const std::string& storage_key));
  MOCK_METHOD1(OnModelStarting, void(ModelTypeSyncBridge* bridge));
  MOCK_METHOD1(ModelReadyToSync, void(std::unique_ptr<MetadataBatch> batch));
  MOCK_METHOD0(IsTrackingMetadata, bool());
  MOCK_METHOD0(TrackedAccountId, std::string());
  MOCK_METHOD0(TrackedCacheGuid, std::string());
  MOCK_METHOD1(ReportError, void(const ModelError& error));
  MOCK_CONST_METHOD0(GetError, base::Optional<ModelError>());
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<ModelTypeControllerDelegate>());

  // Returns a processor that forwards all calls to
  // |this|. |*this| must outlive the returned processor.
  std::unique_ptr<ModelTypeChangeProcessor> CreateForwardingProcessor();

  // Delegates all calls to another instance. |delegate| must not be null and
  // must outlive this object.
  void DelegateCallsByDefaultTo(ModelTypeChangeProcessor* delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModelTypeChangeProcessor);
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_
