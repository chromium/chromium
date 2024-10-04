// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
#define COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_

#include "base/functional/callback.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  ~DataTypeManagerMock() override;

  MOCK_METHOD(void,
              ClearMetadataWhileStoppedExceptFor,
              (DataTypeSet),
              (override));
  MOCK_METHOD(void, SetConfigurer, (DataTypeConfigurer*), (override));
  MOCK_METHOD(void,
              Configure,
              (DataTypeSet, const ConfigureContext&),
              (override));
  MOCK_METHOD(void, DataTypePreconditionChanged, (DataType), (override));
  MOCK_METHOD(void, ResetDataTypeErrors, (), (override));
  MOCK_METHOD(void, PurgeForMigration, (DataTypeSet), (override));
  MOCK_METHOD(void, Stop, (SyncStopMetadataFate), (override));
  MOCK_METHOD(DataTypeSet, GetRegisteredDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet,
              GetDataTypesForTransportOnlyMode,
              (),
              (const override));
  MOCK_METHOD(DataTypeSet, GetActiveDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet,
              GetStoppedDataTypesExcludingNigori,
              (),
              (const override));
  MOCK_METHOD(DataTypeSet, GetActiveProxyDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet,
              GetTypesWithPendingDownloadForInitialSync,
              (),
              (const override));
  MOCK_METHOD(DataTypeSet,
              GetDataTypesWithPermanentErrors,
              (),
              (const override));
  MOCK_METHOD(void,
              GetTypesWithUnsyncedData,
              (DataTypeSet, base::OnceCallback<void(DataTypeSet)>),
              (const override));
  MOCK_METHOD(
      void,
      GetLocalDataDescriptions,
      (DataTypeSet,
       base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>),
      (override));
  MOCK_METHOD(void, TriggerLocalDataMigration, (DataTypeSet), (override));
  MOCK_METHOD(State, state, (), (const override));
  MOCK_METHOD(TypeStatusMapForDebugging,
              GetTypeStatusMapForDebugging,
              (DataTypeSet, DataTypeSet),
              (const override));
  MOCK_METHOD(void,
              GetAllNodesForDebugging,
              (base::OnceCallback<void(base::Value::List)>),
              (const override));
  MOCK_METHOD(void,
              GetEntityCountsForDebugging,
              (base::RepeatingCallback<void(const TypeEntitiesCount&)>),
              (const override));
  MOCK_METHOD(DataTypeController*,
              GetControllerForTest,
              (DataType type),
              (override));

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
