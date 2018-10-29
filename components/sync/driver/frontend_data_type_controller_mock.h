// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__

#include <memory>
#include <string>

#include "components/sync/driver/frontend_data_type_controller.h"
#include "components/sync/driver/model_associator.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class FrontendDataTypeControllerMock : public FrontendDataTypeController {
 public:
  FrontendDataTypeControllerMock();
  ~FrontendDataTypeControllerMock() override;

  // DataTypeController mocks.
  MOCK_METHOD1(StartAssociating, void(StartCallback start_callback));
  MOCK_METHOD2(LoadModels,
               void(const ConfigureContext& configure_context,
                    const ModelLoadCallback& model_load_callback));
  MOCK_METHOD1(Stop, void(ShutdownReason));
  MOCK_CONST_METHOD0(type, ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(state, State());

  // FrontendDataTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD0(Associate, void());
  MOCK_METHOD0(CreateSyncComponents, void());
  MOCK_METHOD0(CleanUpState, void());
  MOCK_CONST_METHOD0(model_associator, AssociatorInterface*());
  MOCK_METHOD1(set_model_associator,
               void(std::unique_ptr<AssociatorInterface> associator));
  MOCK_CONST_METHOD0(change_processor, ChangeProcessor*());
  MOCK_METHOD1(set_change_processor,
               void(std::unique_ptr<ChangeProcessor> processor));
  MOCK_METHOD1(RecordAssociationTime, void(base::TimeDelta time));
  MOCK_METHOD1(RecordStartFailure, void(ConfigureResult result));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
