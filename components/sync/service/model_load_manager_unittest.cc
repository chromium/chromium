// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/model_load_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/sync_error.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace syncer {

namespace {

ConfigureContext BuildConfigureContext(SyncMode sync_mode = SyncMode::kFull) {
  ConfigureContext context;
  context.sync_mode = sync_mode;
  context.cache_guid = "test_cache_guid";
  return context;
}

}  // namespace

class MockModelLoadManagerDelegate : public ModelLoadManagerDelegate {
 public:
  MockModelLoadManagerDelegate() = default;
  ~MockModelLoadManagerDelegate() override = default;
  MOCK_METHOD(void, OnAllDataTypesReadyForConfigure, (), (override));
  MOCK_METHOD(void,
              OnSingleDataTypeWillStop,
              (DataType, const std::optional<SyncError>& error),
              (override));
};

class SyncModelLoadManagerTest : public testing::Test {
 public:
  SyncModelLoadManagerTest() = default;

  FakeDataTypeController* GetController(DataType data_type) {
    auto it = controllers_.find(data_type);
    if (it == controllers_.end()) {
      return nullptr;
    }
    return static_cast<FakeDataTypeController*>(it->second.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockModelLoadManagerDelegate> delegate_;
  DataTypeController::TypeMap controllers_;
};

// Start a type and make sure ModelLoadManager callst the |Start|
// method and calls the callback when it is done.
TEST_F(SyncModelLoadManagerTest, SimpleModelStart) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {BOOKMARKS, APPS};
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);

  // Configure() kicks off model loading.
  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
}

// Start a type, let it finish and then call stop.
TEST_F(SyncModelLoadManagerTest, StopAfterFinish) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types;
  types.Put(BOOKMARKS);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Test that a model that failed to load is reported and stopped properly.
TEST_F(SyncModelLoadManagerTest, ModelLoadFail) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types;
  types.Put(BOOKMARKS);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  EXPECT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());
}

// Test that a runtime error is handled by stopping the type.
TEST_F(SyncModelLoadManagerTest, StopAfterConfiguration) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types;
  types.Put(BOOKMARKS);

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
}

// Test that OnAllDataTypesReadyForConfigure is called when all datatypes that
// require LoadModels before configuration are loaded.
TEST_F(SyncModelLoadManagerTest, OnAllDataTypesReadyForConfigure) {
  // Create two controllers with delayed model load.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {BOOKMARKS, APPS};
  // OnAllDataTypesReadyForConfigure shouldn't be called, APPS data type is not
  // loaded yet.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_STARTING);

  // Finish loading BOOKMARKS, but APPS are still loading.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Finish loading APPS. This should trigger OnAllDataTypesReadyForConfigure.
  GetController(APPS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);

  // Call ModelLoadManager::Configure with reduced set of datatypes.
  // All datatypes in reduced set are already loaded.
  // OnAllDataTypesReadyForConfigure() should be called.
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  DataTypeSet reduced_types = {APPS};
  model_load_manager.Configure(
      /*preferred_types_without_errors=*/reduced_types,
      /*preferred_types=*/reduced_types, BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Test that OnAllDataTypesReadyForConfigure() is called correctly after
// LoadModels fails for one of datatypes.
TEST_F(SyncModelLoadManagerTest,
       OnAllDataTypesReadyForConfigure_FailedLoadModels) {
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {APPS};
  // OnAllDataTypesReadyForConfigure shouldn't be called, APPS data type is not
  // loaded yet.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_STARTING);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Simulate model load error for APPS and finish loading it. This should
  // trigger OnAllDataTypesReadyForConfigure.
  GetController(APPS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::FAILED);
}

// Test that if one of the types fails while another is still being loaded then
// OnAllDataTypesReadyForConfgiure is still called correctly.
TEST_F(SyncModelLoadManagerTest,
       OnAllDataTypesReadyForConfigure_TypeFailedAfterLoadModels) {
  // Create two controllers with delayed model load. Both should block
  // configuration.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {BOOKMARKS, APPS};

  // Apps will finish loading but bookmarks won't.
  // OnAllDataTypesReadyForConfigure shouldn't be called.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  GetController(APPS)->model()->SimulateModelStartFinished();

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  // Apps datatype reports failure.
  GetController(APPS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Finish loading BOOKMARKS. This should trigger
  // OnAllDataTypesReadyForConfigure().
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
}

// Test that Stop clears metadata for disabled type.
TEST_F(SyncModelLoadManagerTest, StopClearMetadata) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  DataTypeSet types = {BOOKMARKS};

  // Configure() kicks off model loading.
  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.Stop(SyncStopMetadataFate::CLEAR_METADATA);

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Test that stopping a single type clears the metadata for the disabled type.
TEST_F(SyncModelLoadManagerTest, StopDataType) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  // Configure() kicks off model loading.
  model_load_manager.Configure(
      /*preferred_types_without_errors=*/{BOOKMARKS},
      /*preferred_types=*/{BOOKMARKS}, BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.StopDatatype(
      BOOKMARKS, SyncStopMetadataFate::CLEAR_METADATA,
      SyncError(FROM_HERE, SyncError::PRECONDITION_ERROR_WITH_KEEP_DATA,
                "Data type is unready."));

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Test that stopping a single type is ignored when the type is not running.
TEST_F(SyncModelLoadManagerTest, StopDataType_NotRunning) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  model_load_manager.StopDatatype(
      BOOKMARKS, SyncStopMetadataFate::CLEAR_METADATA,
      SyncError(FROM_HERE, SyncError::PRECONDITION_ERROR_WITH_KEEP_DATA,
                "Data type is unready."));

  // The state should still be not running.
  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
}

// Test that Configure stops controllers with KEEP_METADATA for preferred
// types.
TEST_F(SyncModelLoadManagerTest, KeepsMetadataForPreferredDataType) {
  // Configure the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {BOOKMARKS, APPS};
  DataTypeSet desired_types = preferred_types;

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Stop one data type without disabling sync.
  desired_types.Remove(APPS);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(0, GetController(APPS)->model()->clear_metadata_count());
}

// Test that Configure stops controllers with CLEAR_METADATA for
// no-longer-preferred types.
TEST_F(SyncModelLoadManagerTest, ClearsMetadataForNotPreferredDataType) {
  // Configure the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {BOOKMARKS, APPS};
  DataTypeSet desired_types = preferred_types;

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Disable one data type.
  preferred_types.Remove(APPS);
  desired_types.Remove(APPS);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(APPS)->model()->clear_metadata_count());
}

TEST_F(SyncModelLoadManagerTest,
       SwitchFromFullSyncToTransportModeRestartsTypes) {
  // Configure the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(
      BOOKMARKS, /*enable_transport_only_model=*/true);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(
      APPS, /*enable_transport_only_model=*/true);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {BOOKMARKS, APPS};

  ConfigureContext configure_context;
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.cache_guid = "test_cache_guid";

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(preferred_types, preferred_types,
                               configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Switch to transport mode.
  configure_context.sync_mode = SyncMode::kTransportOnly;
  // For this test, assume that APPS is not supported in transport mode, but
  // BOOKMARKS is.
  preferred_types.Remove(APPS);

  // Data types should get restarted.
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(preferred_types, preferred_types,
                               configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);

  // When switching modes, the Sync-the-feature mode metadata should get cleared
  // for all datatypes, including datatypes that restarted in transport mode
  // (BOOKMARKS) and datatypes that were excluded (APPS).
  // Note that for BOOKMARKS, it actually gets cleared twice: Once by
  // ModelLoadManager itself, and again via ClearMetadataWhileStopped() from
  // DataTypeController::LoadModels().
  EXPECT_EQ(
      1, GetController(APPS)->model(SyncMode::kFull)->clear_metadata_count());
  EXPECT_EQ(
      2,
      GetController(BOOKMARKS)->model(SyncMode::kFull)->clear_metadata_count());
}

TEST_F(SyncModelLoadManagerTest,
       SwitchFromTransportOnlyToFullSyncRestartsTypes) {
  // Configure the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(
      BOOKMARKS, /*enable_transport_only_model=*/true);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(
      APPS, /*enable_transport_only_model=*/true);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {BOOKMARKS, APPS};
  DataTypeSet desired_types = preferred_types;

  ConfigureContext configure_context;
  configure_context.sync_mode = SyncMode::kTransportOnly;
  configure_context.cache_guid = "test_cache_guid";

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Switch to full-sync mode.
  configure_context.sync_mode = SyncMode::kFull;
  desired_types.Remove(APPS);
  preferred_types.Remove(APPS);

  // Data types should get restarted.
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Configure(desired_types, preferred_types,
                               configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  // The transport-mode metadata for all types should get cleared. Note that for
  // BOOKMARKS, it actually gets cleared twice: Once by ModelLoadManager itself,
  // and again via ClearMetadataWhileStopped() from
  // DataTypeController::LoadModels().
  EXPECT_EQ(1, GetController(APPS)
                   ->model(SyncMode::kTransportOnly)
                   ->clear_metadata_count());
  EXPECT_EQ(2, GetController(BOOKMARKS)
                   ->model(SyncMode::kTransportOnly)
                   ->clear_metadata_count());
}

TEST_F(SyncModelLoadManagerTest, ShouldClearMetadataAfterStopped) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types;
  types.Put(BOOKMARKS);

  // Bring the type to a stopped state.
  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());
  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  model_load_manager.Stop(SyncStopMetadataFate::CLEAR_METADATA);
  // Clearing metadata should work even though the type is already stopped.
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(SyncModelLoadManagerTest, ShouldClearMetadataIfNotRunning) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  model_load_manager.Stop(SyncStopMetadataFate::CLEAR_METADATA);

  // Clearing metadata should work even though the type is not running.
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(SyncModelLoadManagerTest, ShouldNotClearMetadataIfFailed) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _)).Times(2);

  // Bring the underlying model to a failed state. Note that this does *not*
  // bring the controller into the FAILED state yet.
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types{BOOKMARKS};

  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  // Trying to configure exposes the error to the controller (which calls back
  // into the ModelLoadManager).
  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());
  ASSERT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());
  // Failure during model load does *not* clear metadata (see
  // ModelLoadManager::ModelLoadCallback()).
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());

  // Clearing metadata should (again) have no effect since the type is not
  // considered stopped.
  model_load_manager.Stop(SyncStopMetadataFate::CLEAR_METADATA);
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Test that Configure waits for desired types in STOPPING state to stop and
// reload before notifying data type manager.
TEST_F(SyncModelLoadManagerTest,
       ShouldWaitForStoppingDesiredTypesBeforeLoading) {
  // Create two controllers, one with delayed model load.
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {APPS, BOOKMARKS};

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // Bring BOOKMARKS to a STOPPING state.
  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);

  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // It should wait for BOOKMARKS to finish loading before notifying the data
  // type manager.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure).Times(0);

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // APPS is started right away.
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  // BOOKMARKS needs to finish stopping first before it can start again.
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // Finish loading of BOOKMARKS for the first time. This should first move the
  // state to NOT_RUNNING. But, as part of the load callback,
  // DataTypeController::LoadModels() will be called which will set its state
  // to MODEL_STARTING.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  // Finish loading of BOOKMARKS. This will lead to a call to notify the
  // delegate that all the types are ready.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure);
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
}

// Test that Configure will not wait for no-longer-desired types in STOPPING
// state to stop before loading.
TEST_F(SyncModelLoadManagerTest,
       ShouldNotWaitForStoppingUndesiredTypesBeforeLoading) {
  // Create two controllers, one with delayed model load.
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {APPS, BOOKMARKS};
  DataTypeSet preferred_types_without_errors = preferred_types;

  model_load_manager.Configure(preferred_types_without_errors, preferred_types,
                               BuildConfigureContext());

  // Bring BOOKMARKS to a STOPPING state.
  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);

  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // Remove BOOKMARKS from `preferred_types_without_errors` which may happen in
  // case of failures/timeouts.
  preferred_types_without_errors.Remove(BOOKMARKS);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure);
  model_load_manager.Configure(preferred_types_without_errors, preferred_types,
                               BuildConfigureContext());

  // APPS is started and DataTypeManager informed.
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  // BOOKMARKS remains in STOPPING state.
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);
}

// Test that if one of the type is stuck at loading,
// OnAllDataTypesReadyForConfigure will get called after a timeout.
TEST_F(SyncModelLoadManagerTest, ShouldTimeoutIfNotAllTypesLoaded) {
  // Create two controllers with delayed model load. Both should block
  // configuration.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(APPS)->model()->EnableManualModelStart();

  // No calls to OnAllDataTypesReadyForConfigure() yet.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure).Times(0);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {BOOKMARKS, APPS};

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  // Simulate successful loading of APPS only.
  GetController(APPS)->model()->SimulateModelStartFinished();
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  // BOOKMARKS blocks the configuration.
  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure);
  // Types not loaded till now are skipped.
  task_environment_.FastForwardBy(kSyncLoadModelsTimeoutDuration);
}

// Test that if Stop() is called before all data types finish loading,
// OnAllDataTypesReadyForConfigure will *not* get called after a timeout.
// Regression test for crbug.com/333865298.
TEST_F(SyncModelLoadManagerTest, ShouldNotTimeoutAfterStop) {
  // Create a controllers with delayed model load.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  // No calls to OnAllDataTypesReadyForConfigure() should happen.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure).Times(0);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet types = {BOOKMARKS};

  model_load_manager.Configure(/*preferred_types_without_errors=*/types,
                               /*preferred_types=*/types,
                               BuildConfigureContext());

  // BOOKMARKS blocks the configuration.
  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  // The ModelLoadManager gets stopped again before BOOKMARKS finishes loading
  // or times out.
  model_load_manager.Stop(SyncStopMetadataFate::CLEAR_METADATA);

  // Even after the loading timeout period, OnAllDataTypesReadyForConfigure()
  // should *not* get called.
  task_environment_.FastForwardBy(kSyncLoadModelsTimeoutDuration);
}

// Regression test for crbug.com/1506701.
// Tests that if LoadModels is called for a failed type, it's a no-op.
TEST_F(SyncModelLoadManagerTest, ShouldNotStartFailedTypesUponLoadModels) {
  // Create two controllers, one with delayed model load.
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {APPS, BOOKMARKS};

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // Bring BOOKMARKS to a STOPPING state.
  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);

  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // APPS is started right away.
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  // BOOKMARKS needs to finish stopping first before it can start again.
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // Simulate model error while the type is stopping. ModelLoadManager should
  // continue and not wait for the failed type.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure);
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // No crash from LoadModels.
}

// Regression test for crbug.com/1519806.
// Tests that stop callbacks for a type which is not in NOT_RUNNING state
// anymore are ignored.
TEST_F(SyncModelLoadManagerTest,
       ShouldHandleMultipleStopCallbacksForStoppingType) {
  // Create a controller with manual loading.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  DataTypeSet preferred_types = {BOOKMARKS};

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // Bring BOOKMARKS to a STOPPING state.
  model_load_manager.Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // It should wait for BOOKMARKS to finish loading before notifying the data
  // type manager.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure).Times(0);

  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // BOOKMARKS needs to finish stopping first before it can start again.
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // Add the same stop callback again to be called after the type has finished
  // stopping.
  model_load_manager.Configure(
      /*preferred_types_without_errors=*/preferred_types, preferred_types,
      BuildConfigureContext());

  // BOOKMARKS needs to finish stopping first before it can start again.
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);

  // Finish loading of BOOKMARKS for the first time. This should first move the
  // state to NOT_RUNNING. But, as part of the load callback,
  // DataTypeController::LoadModels() will be called which will set its state
  // to MODEL_STARTING.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  // Finish loading of BOOKMARKS. This will lead to a call to notify the
  // delegate that all the types are ready.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure);
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  // Note: The second stop callback didn't do anything and was a no-op.
}

}  // namespace syncer
