// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__

#include "components/sync/driver/data_type_manager.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  ~DataTypeManagerMock() override;

  MOCK_METHOD2(Configure, void(ModelTypeSet, const ConfigureContext&));
  MOCK_METHOD1(DataTypePreconditionChanged, void(ModelType));
  MOCK_METHOD0(ResetDataTypeErrors, void());
  MOCK_METHOD1(PurgeForMigration, void(ModelTypeSet));
  MOCK_METHOD1(Stop, void(ShutdownReason));
  MOCK_METHOD0(controllers, const DataTypeController::TypeMap&());
  MOCK_CONST_METHOD0(GetActiveDataTypes, ModelTypeSet());
  MOCK_CONST_METHOD0(IsNigoriEnabled, bool());
  MOCK_CONST_METHOD0(state, State());

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__
