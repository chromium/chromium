// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_CONTROLLER_DELEGATE_H_

#include "components/sync/model/data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockDataTypeControllerDelegate : public DataTypeControllerDelegate {
 public:
  MockDataTypeControllerDelegate();
  ~MockDataTypeControllerDelegate() override;

  MOCK_METHOD(void,
              OnSyncStarting,
              (const DataTypeActivationRequest& request,
               StartCallback callback),
              (override));
  MOCK_METHOD(void,
              OnSyncStopping,
              (SyncStopMetadataFate metadata_fate),
              (override));
  MOCK_METHOD(void,
              HasUnsyncedData,
              (base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              GetAllNodesForDebugging,
              (AllNodesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetTypeEntitiesCountForDebugging,
              (base::OnceCallback<void(const TypeEntitiesCount&)> callback),
              (const override));
  MOCK_METHOD(void, RecordMemoryUsageAndCountsHistograms, (), (override));
  MOCK_METHOD(void, ClearMetadataIfStopped, (), (override));
  MOCK_METHOD(void, ReportBridgeErrorForTest, (), (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_CONTROLLER_DELEGATE_H_
