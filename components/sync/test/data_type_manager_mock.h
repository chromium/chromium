// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
#define COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_

#include "components/sync/model/sync_error.h"
#include "components/sync/service/data_type_manager.h"
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
  MOCK_METHOD(DataTypeSet, GetPurgedDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet, GetActiveProxyDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet,
              GetTypesWithPendingDownloadForInitialSync,
              (),
              (const override));
  MOCK_METHOD(DataTypeSet,
              GetDataTypesWithPermanentErrors,
              (),
              (const override));
  MOCK_METHOD(State, state, (), (const override));
  MOCK_METHOD(const DataTypeController::TypeMap&,
              GetControllerMap,
              (),
              (const override));

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
