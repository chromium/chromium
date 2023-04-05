// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
#define COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_

#include "components/sync/driver/data_type_manager.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  ~DataTypeManagerMock() override;
  MOCK_METHOD(void,
              Configure,
              (ModelTypeSet, const ConfigureContext&),
              (override));
  MOCK_METHOD(void, DataTypePreconditionChanged, (ModelType), (override));
  MOCK_METHOD(void, ResetDataTypeErrors, (), (override));
  MOCK_METHOD(void, PurgeForMigration, (ModelTypeSet), (override));
  MOCK_METHOD(void, Stop, (SyncStopMetadataFate), (override));
  MOCK_METHOD(ModelTypeSet, GetActiveDataTypes, (), (const override));
  MOCK_METHOD(ModelTypeSet, GetPurgedDataTypes, (), (const override));
  MOCK_METHOD(ModelTypeSet, GetActiveProxyDataTypes, (), (const override));
  MOCK_METHOD(ModelTypeSet,
              GetTypesWithPendingDownloadForInitialSync,
              (),
              (const override));
  MOCK_METHOD(State, state, (), (const override));

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_DATA_TYPE_MANAGER_MOCK_H_
