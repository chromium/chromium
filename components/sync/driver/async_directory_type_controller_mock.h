// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_MOCK_H_

#include <string>

#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class AsyncDirectoryTypeControllerMock : public AsyncDirectoryTypeController {
 public:
  AsyncDirectoryTypeControllerMock();
  ~AsyncDirectoryTypeControllerMock() override;

  // DataTypeController mocks.
  MOCK_METHOD1(StartAssociating, void(StartCallback start_callback));
  MOCK_METHOD2(LoadModels,
               void(const ConfigureContext& configure_context,
                    const ModelLoadCallback& model_load_callback));
  MOCK_METHOD1(Stop, void(ShutdownReason));
  MOCK_CONST_METHOD0(type, ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(state, State());

  // AsyncDirectoryTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD0(StopModels, void());
  MOCK_METHOD2(PostTaskOnModelThread,
               bool(const base::Location&, const base::Closure&));
  MOCK_METHOD3(StartDone,
               void(DataTypeController::ConfigureResult result,
                    const SyncMergeResult& local_merge_result,
                    const SyncMergeResult& syncer_merge_result));
  MOCK_METHOD1(RecordStartFailure, void(ConfigureResult result));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_MOCK_H_
