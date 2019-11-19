// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/fake_model_type_processor.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::NiceMock;
using testing::_;

const ModelType kTestModelType = AUTOFILL;
const char kCacheGuid[] = "SomeCacheGuid";
const char kAccountId[] = "SomeAccountId";

const char kStartFailuresHistogram[] = "Sync.DataTypeStartFailures2";
const char kRunFailuresHistogram[] = "Sync.DataTypeRunFailures2";

MATCHER(ErrorIsSet, "") {
  return arg.IsSet();
}

class MockDelegate : public ModelTypeControllerDelegate {
 public:
  MOCK_METHOD2(OnSyncStarting,
               void(const DataTypeActivationRequest& request,
                    StartCallback callback));
  MOCK_METHOD1(OnSyncStopping, void(SyncStopMetadataFate metadata_fate));
  MOCK_METHOD1(GetAllNodesForDebugging, void(AllNodesCallback callback));
  MOCK_METHOD1(GetStatusCountersForDebugging,
               void(StatusCountersCallback callback));
  MOCK_METHOD0(RecordMemoryUsageAndCountsHistograms, void());
};

// A simple processor that trackes connected state.
class TestModelTypeProcessor
    : public FakeModelTypeProcessor,
      public base::SupportsWeakPtr<TestModelTypeProcessor> {
 public:
  TestModelTypeProcessor() {}

  bool is_connected() const { return is_connected_; }

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> commit_queue) override {
    is_connected_ = true;
  }

  void DisconnectSync() override { is_connected_ = false; }

 private:
  bool is_connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestModelTypeProcessor);
};

// A ModelTypeConfigurer that just connects USS types.
class TestModelTypeConfigurer : public ModelTypeConfigurer {
 public:
  TestModelTypeConfigurer() {}
  ~TestModelTypeConfigurer() override {}

  void ConfigureDataTypes(ConfigureParams params) override {
    NOTREACHED() << "Not implemented.";
  }

  void RegisterDirectoryDataType(ModelType type,
                                 ModelSafeGroup group) override {
    NOTREACHED() << "Not implemented.";
  }

  void UnregisterDirectoryDataType(ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override {
    NOTREACHED() << "Not implemented.";
  }

  void DeactivateDirectoryDataType(ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateNonBlockingDataType(ModelType type,
                                   std::unique_ptr<DataTypeActivationResponse>
                                       activation_response) override {
    DCHECK_EQ(kTestModelType, type);
    DCHECK(!processor_);
    processor_ = std::move(activation_response->type_processor);
    processor_->ConnectSync(nullptr);
  }

  void DeactivateNonBlockingDataType(ModelType type) override {
    DCHECK_EQ(kTestModelType, type);
    DCHECK(processor_);
    processor_->DisconnectSync();
    processor_.reset();
  }

 private:
  std::unique_ptr<ModelTypeProcessor> processor_;
};

// Class used to expose ReportModelError() publicly.
class TestModelTypeController : public ModelTypeController {
 public:
  explicit TestModelTypeController(
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode)
      : ModelTypeController(kTestModelType,
                            std::move(delegate_for_full_sync_mode)) {}
  ~TestModelTypeController() override {}

  using ModelTypeController::ReportModelError;
};

ConfigureContext MakeConfigureContext() {
  ConfigureContext context;
  context.authenticated_account_id = CoreAccountId(kAccountId);
  context.cache_guid = kCacheGuid;
  return context;
}

}  // namespace

class ModelTypeControllerTest : public testing::Test {
 public:
  ModelTypeControllerTest()
      : controller_(std::make_unique<ForwardingModelTypeControllerDelegate>(
            &mock_delegate_)) {}

  ~ModelTypeControllerTest() {
    // Since we use ModelTypeProcessorProxy, which posts tasks, make sure we
    // don't have anything pending on teardown that would make a test fail or
    // crash.
    base::RunLoop().RunUntilIdle();
  }

  bool LoadModels(bool initial_sync_done = false) {
    base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;

    ModelTypeControllerDelegate::StartCallback start_callback;
    EXPECT_CALL(mock_delegate_, OnSyncStarting(_, _))
        .WillOnce([&](const DataTypeActivationRequest& request,
                      ModelTypeControllerDelegate::StartCallback callback) {
          start_callback = std::move(callback);
        });

    controller_.LoadModels(MakeConfigureContext(), load_models_done.Get());
    if (!start_callback) {
      return false;
    }

    // Prepare an activation response, which is the outcome of OnSyncStarting().
    auto activation_response = std::make_unique<DataTypeActivationResponse>();
    activation_response->model_type_state.set_initial_sync_done(
        initial_sync_done);
    activation_response->type_processor =
        std::make_unique<ModelTypeProcessorProxy>(
            base::AsWeakPtr(&processor_),
            base::SequencedTaskRunnerHandle::Get());

    // Mimic completion for OnSyncStarting().
    EXPECT_CALL(load_models_done, Run(_, _));
    std::move(start_callback).Run(std::move(activation_response));
    return true;
  }

  void RegisterWithBackend(bool expect_downloaded) {
    auto result = expect_downloaded
                      ? DataTypeController::TYPE_ALREADY_DOWNLOADED
                      : DataTypeController::TYPE_NOT_YET_DOWNLOADED;
    EXPECT_EQ(result, controller_.RegisterWithBackend(&configurer_));
    // ModelTypeProcessorProxy does posting of tasks.
    base::RunLoop().RunUntilIdle();
  }

  void StartAssociating() {
    base::MockCallback<DataTypeController::StartCallback> callback;
    EXPECT_CALL(callback, Run(DataTypeController::OK, _, _));
    controller_.StartAssociating(callback.Get());
  }

  void StopAndWait(ShutdownReason shutdown_reason) {
    // ModelTypeProcessorProxy does posting of tasks, so we need a runloop. This
    // also verifies that the completion callback is run.
    base::RunLoop loop;
    controller_.Stop(shutdown_reason, loop.QuitClosure());
    loop.Run();
  }

  void DeactivateDataTypeAndStop(ShutdownReason shutdown_reason) {
    controller_.DeactivateDataType(&configurer_);
    StopAndWait(shutdown_reason);
  }

  MockDelegate* delegate() { return &mock_delegate_; }
  TestModelTypeProcessor* processor() { return &processor_; }
  TestModelTypeController* controller() { return &controller_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockDelegate> mock_delegate_;
  TestModelTypeConfigurer configurer_;
  TestModelTypeProcessor processor_;
  TestModelTypeController controller_;
};

TEST_F(ModelTypeControllerTest, InitialState) {
  EXPECT_EQ(kTestModelType, controller()->type());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

TEST_F(ModelTypeControllerTest, LoadModelsOnBackendThread) {
  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;

  ModelTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });

  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  EXPECT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(load_models_done, Run(kTestModelType, _));
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
}

TEST_F(ModelTypeControllerTest, Activate) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(LoadModels());
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  RegisterWithBackend(/*expect_downloaded=*/false);
  EXPECT_TRUE(processor()->is_connected());

  StartAssociating();
  EXPECT_EQ(DataTypeController::RUNNING, controller()->state());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
}

TEST_F(ModelTypeControllerTest, ActivateWithInitialSyncDone) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(LoadModels(/*initial_sync_done=*/true));
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  RegisterWithBackend(/*expect_downloaded=*/true);
  EXPECT_TRUE(processor()->is_connected());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
}

TEST_F(ModelTypeControllerTest, ActivateWithError) {
  ModelErrorHandler error_handler;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        error_handler = request.error_handler;
      });

  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;
  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(error_handler);

  base::HistogramTester histogram_tester;
  // Mimic completion for OnSyncStarting(), with an error.
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  EXPECT_CALL(load_models_done, Run(_, ErrorIsSet()));
  error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // ModelTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectBucketCount(
      kStartFailuresHistogram, ModelTypeHistogramValue(kTestModelType), 1);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

TEST_F(ModelTypeControllerTest, Stop) {
  ASSERT_TRUE(LoadModels());
  RegisterWithBackend(/*expect_downloaded=*/false);
  EXPECT_TRUE(processor()->is_connected());

  StartAssociating();

  DeactivateDataTypeAndStop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates normal browser shutdown. Ensures that metadata was not cleared.
TEST_F(ModelTypeControllerTest, StopWhenDatatypeEnabled) {
  ASSERT_TRUE(LoadModels());
  StartAssociating();

  // Ensures that metadata was not cleared.
  EXPECT_CALL(*delegate(), OnSyncStopping(KEEP_METADATA));
  DeactivateDataTypeAndStop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
  EXPECT_FALSE(processor()->is_connected());
}

// Test emulates scenario when user disables datatype. Metadata should be
// cleared.
TEST_F(ModelTypeControllerTest, StopWhenDatatypeDisabled) {
  ASSERT_TRUE(LoadModels());
  StartAssociating();

  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA));
  DeactivateDataTypeAndStop(DISABLE_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
  EXPECT_FALSE(processor()->is_connected());
}

// Test emulates disabling sync when datatype is not loaded yet. Metadata should
// not be cleared as the delegate is potentially not ready to handle it.
TEST_F(ModelTypeControllerTest, StopBeforeLoadModels) {
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA)).Times(0);

  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  StopAndWait(DISABLE_SYNC);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates disabling sync when datatype is in error state. Metadata should
// not be cleared as the delegate is potentially not ready to handle it.
TEST_F(ModelTypeControllerTest, StopDuringFailedState) {
  EXPECT_CALL(*delegate(), OnSyncStopping(CLEAR_METADATA)).Times(0);

  ModelErrorHandler error_handler;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        error_handler = request.error_handler;
      });

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(error_handler);
  // Mimic completion for OnSyncStarting(), with an error.
  error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // ModelTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(DataTypeController::FAILED, controller()->state());

  StopAndWait(DISABLE_SYNC);

  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Test emulates disabling sync when datatype is loading. The controller should
// wait for completion of the delegate, before stopping it.
TEST_F(ModelTypeControllerTest, StopWhileStarting) {
  ModelTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });

  // A cancelled start never issues completion for the load.
  base::MockCallback<DataTypeController::ModelLoadCallback> load_models_done;
  EXPECT_CALL(load_models_done, Run(_, _)).Times(0);

  controller()->LoadModels(MakeConfigureContext(), load_models_done.Get());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // Stop() should be deferred until OnSyncStarting() finishes.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  controller()->Stop(DISABLE_SYNC, stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping(_));
  EXPECT_CALL(stop_completion, Run());
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates disabling sync when datatype is loading. The controller should
// wait for completion of the delegate, before stopping it. In this test,
// loading produces an error, so the resulting state should be FAILED.
TEST_F(ModelTypeControllerTest, StopWhileStartingWithError) {
  ModelErrorHandler error_handler;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        error_handler = request.error_handler;
      });

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(error_handler);

  // Stop() should be deferred until OnSyncStarting() finishes.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  controller()->Stop(DISABLE_SYNC, stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  base::HistogramTester histogram_tester;
  // Mimic completion for OnSyncStarting(), with an error.
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  EXPECT_CALL(stop_completion, Run());
  error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // ModelTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectBucketCount(kStartFailuresHistogram,
                                     ModelTypeHistogramValue(kTestModelType),
                                     /*count=*/1);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

// Test emulates a controller talking to a delegate (processor) in a backend
// thread, which necessarily involves task posting (usually via
// ProxyModelTypeControllerDelegate), where the backend posts an error
// simultaneously to the UI stopping the datatype.
TEST_F(ModelTypeControllerTest, StopWhileErrorInFlight) {
  ModelTypeControllerDelegate::StartCallback start_callback;
  ModelErrorHandler error_handler;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
        error_handler = request.error_handler;
      });

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);
  ASSERT_TRUE(error_handler);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  // At this point, the UI stops the datatype, but it's possible that the
  // backend has already posted a task to the UI thread, which we'll process
  // later below.
  StopAndWait(DISABLE_SYNC);
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  base::HistogramTester histogram_tester;
  // In the next loop iteration, the UI thread receives the error.
  error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // ModelTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
  histogram_tester.ExpectTotalCount(kRunFailuresHistogram, 0);
}

// Test emulates a controller subclass issuing ReportModelError() (e.g. custom
// passphrase was enabled and the type should be disabled) while the delegate
// is starting.
TEST_F(ModelTypeControllerTest, ReportErrorWhileStarting) {
  ModelTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // The delegate should receive no OnSyncStopping() while starting despite
  // the subclass issuing ReportModelError().
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  controller()->ReportModelError(syncer::SyncError::DATATYPE_POLICY_ERROR,
                                 ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping(_));
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Test emulates a controller subclass issuing ReportModelError() (e.g. custom
// passphrase was enabled and the type should be disabled) AND the controller
// being requested to stop, both of which are received while the delegate is
// starting.
TEST_F(ModelTypeControllerTest, StopAndReportErrorWhileStarting) {
  ModelTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });

  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(start_callback);

  // The controller receives Stop() which should be deferred until
  // OnSyncStarting() finishes or ReportModelError() is called.
  base::MockCallback<base::OnceClosure> stop_completion;
  EXPECT_CALL(stop_completion, Run()).Times(0);
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  controller()->Stop(DISABLE_SYNC, stop_completion.Get());
  EXPECT_EQ(DataTypeController::STOPPING, controller()->state());

  // The subclass issues ReportModelError(), which should be treated as stop
  // completion, but shouldn't lead to an immediate OnSyncStopping() until
  // loading completes.
  EXPECT_CALL(stop_completion, Run());
  EXPECT_CALL(*delegate(), OnSyncStopping(_)).Times(0);
  controller()->ReportModelError(syncer::SyncError::DATATYPE_POLICY_ERROR,
                                 ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());

  // Mimic completion for OnSyncStarting().
  EXPECT_CALL(*delegate(), OnSyncStopping(_));
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
}

// Tests that SyncMode is honored when the controller has been constructed
// with two delegates.
TEST(ModelTypeControllerWithMultiDelegateTest, ToggleSyncMode) {
  base::test::SingleThreadTaskEnvironment task_environment;
  NiceMock<MockDelegate> delegate_for_full_sync_mode;
  NiceMock<MockDelegate> delegate_for_transport_mode;

  ModelTypeController controller(
      kTestModelType,
      std::make_unique<ForwardingModelTypeControllerDelegate>(
          &delegate_for_full_sync_mode),
      std::make_unique<ForwardingModelTypeControllerDelegate>(
          &delegate_for_transport_mode));

  ConfigureContext context;
  context.authenticated_account_id = CoreAccountId(kAccountId);
  context.cache_guid = kCacheGuid;

  ModelTypeControllerDelegate::StartCallback start_callback;

  // Start sync with SyncMode::kTransportOnly.
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStarting(_, _)).Times(0);
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });
  context.sync_mode = SyncMode::kTransportOnly;
  controller.LoadModels(context, base::DoNothing());

  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller.state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller.state());

  // Stop sync.
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStopping(_)).Times(0);
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStopping(_));
  controller.Stop(DISABLE_SYNC, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller.state());

  // Start sync with SyncMode::kFull.
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStarting(_, _)).Times(0);
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        start_callback = std::move(callback);
      });
  context.sync_mode = SyncMode::kFull;
  controller.LoadModels(context, base::DoNothing());

  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller.state());
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller.state());

  // Stop sync.
  EXPECT_CALL(delegate_for_transport_mode, OnSyncStopping(_)).Times(0);
  EXPECT_CALL(delegate_for_full_sync_mode, OnSyncStopping(_));
  controller.Stop(DISABLE_SYNC, base::DoNothing());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller.state());
}

TEST_F(ModelTypeControllerTest, ReportErrorAfterLoaded) {
  base::HistogramTester histogram_tester;
  // Capture the callbacks.
  ModelErrorHandler error_handler;
  ModelTypeControllerDelegate::StartCallback start_callback;
  EXPECT_CALL(*delegate(), OnSyncStarting(_, _))
      .WillOnce([&](const DataTypeActivationRequest& request,
                    ModelTypeControllerDelegate::StartCallback callback) {
        error_handler = request.error_handler;
        start_callback = std::move(callback);
      });
  controller()->LoadModels(MakeConfigureContext(), base::DoNothing());
  ASSERT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  ASSERT_TRUE(error_handler);
  ASSERT_TRUE(start_callback);

  // Mimic completion for OnSyncStarting().
  std::move(start_callback).Run(std::make_unique<DataTypeActivationResponse>());
  ASSERT_EQ(DataTypeController::MODEL_LOADED, controller()->state());

  StartAssociating();
  ASSERT_EQ(DataTypeController::RUNNING, controller()->state());

  // Now trigger the run-time error.
  error_handler.Run(ModelError(FROM_HERE, "Test error"));
  // TODO(mastiz): We shouldn't need RunUntilIdle() here, but
  // ModelTypeController currently uses task-posting for errors.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::FAILED, controller()->state());
  histogram_tester.ExpectTotalCount(kStartFailuresHistogram, 0);
  histogram_tester.ExpectBucketCount(kRunFailuresHistogram,
                                     ModelTypeHistogramValue(kTestModelType),
                                     /*count=*/1);
}

}  // namespace syncer
