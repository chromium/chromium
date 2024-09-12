// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_batch.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockDataTypeLocalChangeProcessor : public DataTypeLocalChangeProcessor {
 public:
  MockDataTypeLocalChangeProcessor();

  MockDataTypeLocalChangeProcessor(const MockDataTypeLocalChangeProcessor&) =
      delete;
  MockDataTypeLocalChangeProcessor& operator=(
      const MockDataTypeLocalChangeProcessor&) = delete;

  ~MockDataTypeLocalChangeProcessor() override;

  MOCK_METHOD(void,
              Put,
              (const std::string& storage_key,
               std::unique_ptr<EntityData> entity_data,
               MetadataChangeList* metadata_change_list),
              (override));
  MOCK_METHOD(void,
              Delete,
              (const std::string& storage_key,
               const DeletionOrigin& origin,
               MetadataChangeList* metadata_change_list),
              (override));
  MOCK_METHOD(void,
              UpdateStorageKey,
              (const EntityData& entity_data,
               const std::string& storage_key,
               MetadataChangeList* metadata_change_list),
              (override));
  MOCK_METHOD(void,
              UntrackEntityForStorageKey,
              (const std::string& storage_key),
              (override));
  MOCK_METHOD(void,
              UntrackEntityForClientTagHash,
              (const ClientTagHash& client_tag_hash),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAllTrackedStorageKeys,
              (),
              (const override));
  MOCK_METHOD(bool,
              IsEntityUnsynced,
              (const std::string& storage_key),
              (const override));
  MOCK_METHOD(base::Time,
              GetEntityCreationTime,
              (const std::string& storage_key),
              (const override));
  MOCK_METHOD(base::Time,
              GetEntityModificationTime,
              (const std::string& storage_key),
              (const override));
  MOCK_METHOD(void, OnModelStarting, (DataTypeSyncBridge * bridge), (override));
  MOCK_METHOD(void,
              ModelReadyToSync,
              (std::unique_ptr<MetadataBatch> batch),
              (override));
  MOCK_METHOD(bool, IsTrackingMetadata, (), (const override));
  MOCK_METHOD(std::string, TrackedAccountId, (), (const override));
  MOCK_METHOD(std::string, TrackedCacheGuid, (), (const override));
  MOCK_METHOD(void, ReportError, (const ModelError& error), (override));
  MOCK_METHOD(std::optional<ModelError>, GetError, (), (const override));
  MOCK_METHOD(base::WeakPtr<DataTypeControllerDelegate>,
              GetControllerDelegate,
              (),
              (override));
  MOCK_METHOD(const sync_pb::EntitySpecifics&,
              GetPossiblyTrimmedRemoteSpecifics,
              (const std::string& storage_key),
              (const override));
  MOCK_METHOD(sync_pb::UniquePosition,
              UniquePositionAfter,
              (const std::string&, const ClientTagHash&),
              (const));
  MOCK_METHOD(sync_pb::UniquePosition,
              UniquePositionBefore,
              (const std::string&, const ClientTagHash&),
              (const));
  MOCK_METHOD(sync_pb::UniquePosition,
              UniquePositionBetween,
              (const std::string&, const std::string&, const ClientTagHash&),
              (const));
  MOCK_METHOD(sync_pb::UniquePosition,
              UniquePositionForInitialEntity,
              (const ClientTagHash&),
              (const));
  MOCK_METHOD(sync_pb::UniquePosition,
              GetUniquePositionForStorageKey,
              (const std::string&),
              (const));

  base::WeakPtr<DataTypeLocalChangeProcessor> GetWeakPtr() override;

  // Returns a processor that forwards all calls to
  // |this|. |*this| must outlive the returned processor.
  std::unique_ptr<DataTypeLocalChangeProcessor> CreateForwardingProcessor();

  // Delegates all calls to another instance. |delegate| must not be null and
  // must outlive this object.
  void DelegateCallsByDefaultTo(DataTypeLocalChangeProcessor* delegate);

 private:
  base::WeakPtrFactory<MockDataTypeLocalChangeProcessor> weak_ptr_factory_{
      this};
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
