// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_registry.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/fake_model_type_processor.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/mock_nudge_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class ModelTypeRegistryTest : public ::testing::Test {
 public:
  void SetUp() override {
    registry_ = std::make_unique<ModelTypeRegistry>(
        &mock_nudge_handler_, &cancelation_signal_, &encryption_handler_);
  }

  void TearDown() override { registry_.reset(); }

  ModelTypeRegistry* registry() { return registry_.get(); }

  static sync_pb::ModelTypeState MakeInitialModelTypeState(ModelType type) {
    sync_pb::ModelTypeState state;
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type));
    return state;
  }

  static std::unique_ptr<DataTypeActivationResponse>
  MakeDataTypeActivationResponse(
      const sync_pb::ModelTypeState& model_type_state) {
    auto context = std::make_unique<DataTypeActivationResponse>();
    context->model_type_state = model_type_state;
    context->type_processor = std::make_unique<FakeModelTypeProcessor>();
    return context;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeSyncEncryptionHandler encryption_handler_;
  CancelationSignal cancelation_signal_;
  std::unique_ptr<ModelTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
};

TEST_F(ModelTypeRegistryTest, ConnectDataTypes) {
  EXPECT_TRUE(registry()->GetConnectedTypes().Empty());

  registry()->ConnectDataType(THEMES, MakeDataTypeActivationResponse(
                                          MakeInitialModelTypeState(THEMES)));
  EXPECT_EQ(ModelTypeSet({THEMES}), registry()->GetConnectedTypes());

  registry()->ConnectDataType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialModelTypeState(SESSIONS)));
  EXPECT_EQ(ModelTypeSet({THEMES, SESSIONS}), registry()->GetConnectedTypes());

  registry()->DisconnectDataType(THEMES);
  EXPECT_EQ(ModelTypeSet({SESSIONS}), registry()->GetConnectedTypes());

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' ModelTypeSyncWorker.
}

// Tests correct result returned from GetInitialSyncEndedTypes.
TEST_F(ModelTypeRegistryTest, GetInitialSyncEndedTypes) {
  // Themes has finished initial sync.
  sync_pb::ModelTypeState model_type_state = MakeInitialModelTypeState(THEMES);
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  registry()->ConnectDataType(THEMES,
                              MakeDataTypeActivationResponse(model_type_state));

  // SESSIONS has NOT finished initial sync.
  registry()->ConnectDataType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialModelTypeState(SESSIONS)));

  EXPECT_EQ(ModelTypeSet({THEMES}), registry()->GetInitialSyncEndedTypes());
}

}  // namespace

}  // namespace syncer
