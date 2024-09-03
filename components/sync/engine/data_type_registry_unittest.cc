// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_registry.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_worker.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/test/fake_data_type_processor.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/mock_nudge_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class DataTypeRegistryTest : public ::testing::Test {
 public:
  void SetUp() override {
    registry_ = std::make_unique<DataTypeRegistry>(
        &mock_nudge_handler_, &cancelation_signal_, &encryption_handler_);
  }

  void TearDown() override { registry_.reset(); }

  DataTypeRegistry* registry() { return registry_.get(); }

  static sync_pb::DataTypeState MakeInitialDataTypeState(DataType type) {
    sync_pb::DataTypeState state;
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(type));
    return state;
  }

  static std::unique_ptr<DataTypeActivationResponse>
  MakeDataTypeActivationResponse(
      const sync_pb::DataTypeState& data_type_state) {
    auto context = std::make_unique<DataTypeActivationResponse>();
    context->data_type_state = data_type_state;
    context->type_processor = std::make_unique<FakeDataTypeProcessor>();
    return context;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeSyncEncryptionHandler encryption_handler_;
  CancelationSignal cancelation_signal_;
  std::unique_ptr<DataTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
};

TEST_F(DataTypeRegistryTest, ConnectDataTypes) {
  EXPECT_TRUE(registry()->GetConnectedTypes().empty());

  registry()->ConnectDataType(
      THEMES, MakeDataTypeActivationResponse(MakeInitialDataTypeState(THEMES)));
  EXPECT_EQ(DataTypeSet({THEMES}), registry()->GetConnectedTypes());

  registry()->ConnectDataType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialDataTypeState(SESSIONS)));
  EXPECT_EQ(DataTypeSet({THEMES, SESSIONS}), registry()->GetConnectedTypes());

  registry()->DisconnectDataType(THEMES);
  EXPECT_EQ(DataTypeSet({SESSIONS}), registry()->GetConnectedTypes());

  // Allow DataTypeRegistry destruction to delete the
  // Sessions' DataTypeSyncWorker.
}

// Tests correct result returned from GetInitialSyncEndedTypes.
TEST_F(DataTypeRegistryTest, GetInitialSyncEndedTypes) {
  // Themes has finished initial sync.
  sync_pb::DataTypeState data_type_state = MakeInitialDataTypeState(THEMES);
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  registry()->ConnectDataType(THEMES,
                              MakeDataTypeActivationResponse(data_type_state));

  // SESSIONS has NOT finished initial sync.
  registry()->ConnectDataType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialDataTypeState(SESSIONS)));

  EXPECT_EQ(DataTypeSet({THEMES}), registry()->GetInitialSyncEndedTypes());
}

}  // namespace

}  // namespace syncer
