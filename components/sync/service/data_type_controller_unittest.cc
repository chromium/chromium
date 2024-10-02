// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_controller.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/test/fake_data_type_processor.h"
#include "components/sync/test/mock_data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::InSequence;
using testing::Ne;
using testing::NiceMock;
using testing::NotNull;
using testing::SaveArg;

const DataType kTestDataType = AUTOFILL;
const char kCacheGuid[] = "SomeCacheGuid";
const char kAccountId[] = "SomeAccountId";

const char kStartFailuresHistogram[] = "Sync.DataTypeStartFailures2";
const char kRunFailuresHistogram[] = "Sync.DataTypeRunFailures2";

// Class used to expose ReportModelError() publicly.
class TestDataTypeController : public DataTypeController {
 public:
  explicit TestDataTypeController(
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode)
      : DataTypeController(kTestDataType,
                           std::move(delegate_for_full_sync_mode),
                           /*delegate_for_transport_mode=*/nullptr) {}
  ~TestDataTypeController() override = default;

  using DataTypeController::ReportModelError;
};

ConfigureContext MakeConfigureContext() {
  ConfigureContext context;
  context.authenticated_account_id = CoreAccountId::FromGaiaId(kAccountId);
  context.cache_guid = kCacheGuid;
  return context;
}

}  // namespace

class DataTypeControllerTest : public testing::Test {
 public:
  DataTypeControllerTest()
      : controller_(std::make_unique<ForwardingDataTypeControllerDelegate>(
            &mock_delegate_)) {}

  ~DataTypeControllerTest() override = default;

  bool LoadModels(bool initial_sync_done = false) {
    base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;

    DataTypeControllerDelegate::StartCallback start_callback;
    EXPECT_CALL(mock_delegate_, OnSyncStarting)
        .WillOnce(MoveArg<1>(&start_callback));

    controller_.LoadModels(MakeConfigureContext(), load_models_done.Get());
    if (!start_callback) {
      return false;
    }

    // Prepare an activation response, which is the outcome of OnSyncStarting().
    auto activation_response = std::make_unique<DataTypeActivationResponse>();
    activation_response->data_type_state.set_initial_sync_state(
        initial_sync_done
            ? sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE
            : sync_pb::
                  DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
    activation_response->type_processor =
        std::make_unique<FakeDataTypeProcessor>();

    // Mimic completion for OnSyncStarting().
    EXPECT_CALL(load_models_done, Run);
    std::move(start_callback).Run(std::move(activation_response));
    return true;
  }

  MockDataTypeControllerDelegate* delegate() { return &mock_delegate_; }
  TestDataTypeController* controller() { return &controller_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockDataTypeControllerDelegate> mock_delegate_;
  FakeDataTypeProcessor processor_;
  TestDataTypeController controller_;
};

TEST_F(DataTypeControllerTest, InitialState) {
  EXPECT_EQ(kTestDataType, controller()->type());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

TEST_F(DataTypeControllerTest, LoadModelsOnBackendThread) {
  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;

  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));

  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  EXPECT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(load_models_done, Run(/*error=*/Eq(std::nullopt)));
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
}

TEST_F(DataTypeControllerTest, Connect) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(LoadModels(/*initial_sync_done=*/false));
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  std::unique_ptr<DataTypeActivationResponse> activation_response =
      controller()->Connect();
  EXPECT_EQ(DataTypeController::RUNNING, controller()->state());

  ASSERT_THAT(activation_response, NotNull());
  EXPECT_THAT(activation_response->type_processor, NotNull());
  EXPECT_EQ(
      activation_response->data_type_state.initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);

  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
}

TEST_F(DataTypeControllerTest, ConnectWithInitialSyncDone) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(LoadModels(/*initial_sync_done=*/true));
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  std::unique_ptr<DataTypeActivationResponse> activation_response =
      controller()->Connect();
  EXPECT_EQ(DataTypeController::RUNNING, controller()->state());

  ASSERT_THAT(activation_response, NotNull());
  EXPECT_THAT(activation_response->type_processor, NotNull());
  EXPECT_EQ(activation_response->data_type_state.initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
}

TEST_F(DataTypeControllerTest, ConnectWithError) {
  DataTypeActivationRequest activation_request;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(SaveArg<0>(&activation_request));

  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;
  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);

  base::HistogramTester histogram_tester;
  // Mimic completion for OnSyncStarting(), with an error.
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  EXPECT_CALL(load_models_done, Run(/*error=*/Ne(std::nullopt)));
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectBucketCount(kStartFailuresHistogram,
                                     DataTypeHistogramValue(kTestDataType), 1);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

TEST_F(DataTypeControllerTest, Stop) {
  ASSERT_TRUE(LoadModels());
  controller()->Connect();
  ASSERT_EQ(DataTypeController::RUNNING, controller()->state());

  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::KEEP_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates normal browser shutdown. Ensures that metadata was not cleared.
TEST_F(DataTypeControllerTest, StopWhenDatatypeEnabled) {
  ASSERT_TRUE(LoadModels());

  // Ensures that metadata was not cleared.
  EXPECT_CALL(*delegate(), OnSyncStopping(KEEP_METADATA));

  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::KEEP_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates scenario when user disables datatype. Metadata should be
// cleared.
TEST_F(DataTypeControllerTest, StopWhenDatatypeDisabled) {
  ASSERT_TRUE(LoadModels());

  // Ensures that metadata was cleared.
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA));

  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// When Stop() is called with SyncStopMetadataFate::CLEAR_METADATA, while
// the controller is still stopping, data is indeed cleared, regardless of the
// ShutdownReason of previous calls.
TEST_F(DataTypeControllerTest, StopWhileStopping) {
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));
  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());

  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());

  // Stop() should be deferred until OnSyncStarting() finishes.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);

  controller()->Stop(SyncStopMetadataFate::KEEP_METADATA,
                     stop_completion.Get());
  ASSERT_EQ(DataTypeController::STOPPING, controller()->state());

  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  ASSERT_EQ(DataTypeController::STOPPING, controller()->state());

  // Data should be cleared.
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA));

  // The |stop_completion| callback should be called twice, because Stop() was
  // called once while the state was MODEL_STARTING and another while the state
  // was STOPPING.
  EXPECT_CALL(stop_completion, Run()).Times(2);
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates disabling sync when datatype is not loaded yet.
TEST_F(DataTypeControllerTest, StopBeforeLoadModels) {
  // OnSyncStopping() should not be called, since the delegate was never
  // started. Instead, ClearMetadataIfStopped() should get called.
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  EXPECT_CALL(*delegate(), ClearMetadataIfStopped());

  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());

  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates disabling sync when datatype is in error state. Metadata should
// not be cleared as the delegate is potentially not ready to handle it.
TEST_F(DataTypeControllerTest, StopDuringFailedState) {
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA)).Times(0);

  DataTypeActivationRequest activation_request;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(SaveArg<0>(&activation_request));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);
  // Mimic completion for OnSyncStarting(), with an error.
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(DataTypeController::FAILED, controller()->state());

  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());

  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Test emulates disabling sync when datatype is loading. The controller should
// wait for completion of the delegate, before stopping it.
TEST_F(DataTypeControllerTest, StopWhileStarting) {
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));

  // A cancelled start never issues completion for the load.
  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;
  EXPECT_CALL(load_models_done, Run).Times(0);

  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // Stop() should be deferred until OnSyncStarting() finishes.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping);
  EXPECT_CALL(stop_completion, Run());
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates disabling sync when datatype is loading. The controller should
// wait for completion of the delegate, before stopping it. In this test,
// loading produces an error, so the resulting state should be FAILED.
TEST_F(DataTypeControllerTest, StopWhileStartingWithError) {
  DataTypeActivationRequest activation_request;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(SaveArg<0>(&activation_request));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);

  // Stop() should be deferred until OnSyncStarting() finishes.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  base::HistogramTester histogram_tester;
  // Mimic completion for OnSyncStarting(), with an error.
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  EXPECT_CALL(stop_completion, Run());
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectBucketCount(kStartFailuresHistogram,
                                     DataTypeHistogramValue(kTestDataType),
                                     /*count=*/1);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

// Test emulates a controller talking to a delegate (processor) in a backend
// thread, which necessarily involves task posting (usually via
// ProxyDataTypeControllerDelegate), where the backend posts an error
// simultaneously to the UI stopping the datatype.
TEST_F(DataTypeControllerTest, StopWhileErrorInFlight) {
  DataTypeControllerDelegate::StartCallback start_callback;
  DataTypeActivationRequest activation_request;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(
          DoAll(SaveArg<0>(&activation_request), MoveArg<1>(&start_callback)));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);
  ASSERT_TRUE(activation_request.error_handler);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  // At this point, the UI stops the datatype, but it's possible that the
  // backend has already posted a task to the UI thread, which we'll process
  // later below.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run());
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  base::HistogramTester histogram_tester;
  // In the next loop iteration, the UI thread receives the error.
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

// Test emulates a controller subclass issuing ReportModelError() (e.g. custom
// passphrase was enabled and the type should be disabled) while the delegate
// is starting.
TEST_F(DataTypeControllerTest, ReportErrorWhileStarting) {
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // The delegate should receive no OnSyncStopping() while starting despite
  // the subclass issuing ReportModelError().
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  controller()->ReportModelError(ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping);
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Test emulates a controller subclass issuing ReportModelError() (e.g. custom
// passphrase was enabled and the type should be disabled) AND the controller
// being requested to stop, both of which are received while the delegate is
// starting.
TEST_F(DataTypeControllerTest, StopAndReportErrorWhileStarting) {
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // The controller receives Stop() which should be deferred until
  // OnSyncStarting() finishes or ReportModelError() is called.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                     stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  // The subclass issues ReportModelError(), which should be treated as stop
  // completion, but shouldn't lead to an immediate OnSyncStopping() until
  // loading completes.
  EXPECT_CALL(stop_completion, Run());
  EXPECT_CALL(*delegate(), OnSyncStopping).Times(0);
  controller()->ReportModelError(ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping);
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Tests that SyncMode is honored when the controller has been constructed
// with two delegates.
TEST(DataTypeControllerWithMultiDelegateTest, ToggleSyncMode) {
  base::test::SingleThreadTaskEnvironment task_environment;
  NiceMock<MockDataTypeControllerDelegate> delegate_for_full_sync_mode;
  NiceMock<MockDataTypeControllerDelegate> delegate_for_transport_mode;

  DataTypeController controller(
      kTestDataType,
      std::make_unique<ForwardingDataTypeControllerDelegate>(
          &delegate_for_full_sync_mode),
      std::make_unique<ForwardingDataTypeControllerDelegate>(
          &delegate_for_transport_mode));

  ConfigureContext context;
  context.authenticated_account_id = CoreAccountId::FromGaiaId(kAccountId);
  context.cache_guid = kCacheGuid;

  DataTypeControllerDelegate::StartCallback start_callback;

  // Start sync with SyncMode::kTransportOnly.
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStarting).Times(0);
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));
  context.sync_mode = SyncMode::kTransportOnly;
  controller.LoadModels(context, base::DoNothing());

  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller.state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller.state());

  // Stop sync.
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStopping).Times(0);
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStopping);
  controller.Stop(SyncStopMetadataFate::CLEAR_METADATA, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller.state());

  // Start sync with SyncMode::kFull.
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStarting).Times(0);
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStarting)
      .WillOnce(MoveArg<1>(&start_callback));
  context.sync_mode = SyncMode::kFull;
  controller.LoadModels(context, base::DoNothing());

  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller.state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller.state());

  // Stop sync.
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStopping).Times(0);
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStopping);
  controller.Stop(SyncStopMetadataFate::CLEAR_METADATA, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller.state());
}

TEST_F(DataTypeControllerTest, ReportErrorAfterLoaded) {
  base::HistogramTester histogram_tester;
  // Capture the callbacks.
  DataTypeActivationRequest activation_request;
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(
          DoAll(SaveArg<0>(&activation_request), MoveArg<1>(&start_callback)));
  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  // Now trigger the run-time error.
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
  histogram_tester.ExpectBucketCount(kStartFailuresHistogram,
                                     DataTypeHistogramValue(kTestDataType),
                                     /*count=*/1);
}

TEST_F(DataTypeControllerTest, ReportErrorAfterRegisteredWithBackend) {
  base::HistogramTester histogram_tester;
  // Capture the callbacks.
  DataTypeActivationRequest activation_request;
  DataTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(
          DoAll(SaveArg<0>(&activation_request), MoveArg<1>(&start_callback)));
  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);
  ASSERT_TRUE(start_callback);

  // An activation response with a non-null processor is required for
  // registering with the backend.
  auto activation_response = std::make_unique<DataTypeActivationResponse>();
  activation_response->type_processor =
      std::make_unique<FakeDataTypeProcessor>();

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::move(activation_response));
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  controller()->Connect();
  ASSERT_EQ(DataTypeController::RUNNING, controller()->state());

  // Now trigger the run-time error.
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // DataTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
  histogram_tester.ExpectBucketCount(kRunFailuresHistogram,
                                     DataTypeHistogramValue(kTestDataType),
                                     /*count=*/1);
}

TEST_F(DataTypeControllerTest, ClearMetadataWhenDatatypeNotRunning) {
  // Start sync and then stop it(without clearing the metadata) to bring it
  // to NOT_RUNNING state.
  ASSERT_TRUE(LoadModels());
  controller()->Connect();

  {
    InSequence s;
    EXPECT_CALL(*delegate(), OnSyncStopping(KEEP_METADATA));
    EXPECT_CALL(*delegate(), ClearMetadataIfStopped);
  }
  controller()->Stop(SyncStopMetadataFate::KEEP_METADATA, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  // ClearMetadataIfStopped() should be called on Stop() even if state is
  // NOT_RUNNING.
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

TEST_F(DataTypeControllerTest,
       ShouldNotClearMetadataWhenDatatypeInFailedState) {
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA)).Times(0);

  // Start sync and simulate an error to bring it to a FAILED state.
  DataTypeActivationRequest activation_request;
  EXPECT_CALL(*delegate(), OnSyncStarting)
      .WillOnce(SaveArg<0>(&activation_request));

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(activation_request.error_handler);
  // Mimic completion for OnSyncStarting(), with an error.
  activation_request.error_handler.Run(ModelError(FROM_HERE, "Test error"));
  base::RunLoop().RunUntilIdle();

  // ClearMetadataIfStopped() should not be called on Stop() if the state is
  // FAILED.
  ASSERT_EQ(DataTypeController::FAILED, controller()->state());
  EXPECT_CALL(*delegate(), ClearMetadataIfStopped).Times(0);
  controller()->Stop(SyncStopMetadataFate::CLEAR_METADATA, base::DoNothing());
  ASSERT_EQ(DataTypeController::FAILED, controller()->state());
}

}  // namespace syncer
