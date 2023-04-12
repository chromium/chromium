// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_manager_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::UnorderedElementsAre;

namespace syncer {

namespace {

// Helpers for unioning with control types.
ModelTypeSet AddControlTypesTo(ModelType type) {
  return Union(ControlTypes(), ModelTypeSet(type));
}

ConfigureContext BuildConfigureContext(ConfigureReason reason,
                                       SyncMode sync_mode = SyncMode::kFull) {
  ConfigureContext context;
  context.reason = reason;
  context.sync_mode = sync_mode;
  context.cache_guid = "test_cache_guid";
  return context;
}

DataTypeStatusTable BuildStatusTable(ModelTypeSet crypto_errors,
                                     ModelTypeSet datatype_errors,
                                     ModelTypeSet unready_errors) {
  DataTypeStatusTable::TypeErrorMap error_map;
  for (ModelType type : crypto_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::CRYPTO_ERROR,
                                "crypto error expected", type);
  }
  for (ModelType type : datatype_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                "datatype error expected", type);
  }
  for (ModelType type : unready_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::UNREADY_ERROR,
                                "unready error expected", type);
  }
  DataTypeStatusTable status_table;
  status_table.UpdateFailedDataTypes(error_map);
  return status_table;
}

// Fake ModelTypeConfigurer implementation that simply stores away the
// callback passed into ConfigureDataTypes.
class FakeModelTypeConfigurer : public ModelTypeConfigurer {
 public:
  FakeModelTypeConfigurer() = default;
  ~FakeModelTypeConfigurer() override = default;

  void ConfigureDataTypes(ConfigureParams params) override {
    configure_call_count_++;
    last_params_ = std::move(params);
  }

  void ConnectDataType(ModelType type,
                       std::unique_ptr<DataTypeActivationResponse>
                           activation_response) override {
    connected_types_.Put(type);
  }

  void DisconnectDataType(ModelType type) override {
    connected_types_.Remove(type);
  }

  void SetProxyTabsDatatypeEnabled(bool enabled) override {}

  void FinishDownload(ModelTypeSet types_to_configure,
                      ModelTypeSet failed_download_types) {
    ASSERT_FALSE(last_params_.ready_task.is_null());
    std::move(last_params_.ready_task)
        .Run(Difference(types_to_configure, failed_download_types),
             failed_download_types);
  }

  const ModelTypeSet connected_types() { return connected_types_; }

  int configure_call_count() const { return configure_call_count_; }

  const ConfigureParams& last_params() const { return last_params_; }

 private:
  ModelTypeSet connected_types_;
  int configure_call_count_ = 0;
  ConfigureParams last_params_;
};

// DataTypeManagerObserver implementation.
class FakeDataTypeManagerObserver : public DataTypeManagerObserver {
 public:
  FakeDataTypeManagerObserver() { ResetExpectations(); }
  ~FakeDataTypeManagerObserver() override {
    EXPECT_FALSE(start_expected_);
    DataTypeManager::ConfigureResult default_result;
    EXPECT_EQ(done_expectation_.status, default_result.status);
  }

  void ExpectStart(base::OnceClosure start_callback) {
    start_expected_ = true;
    start_callback_ = std::move(start_callback);
  }
  void ExpectDone(const DataTypeManager::ConfigureResult& result) {
    done_expectation_ = result;
  }
  void ResetExpectations() {
    start_expected_ = false;
    done_expectation_ = DataTypeManager::ConfigureResult();
  }

  void OnConfigureDone(
      const DataTypeManager::ConfigureResult& result) override {
    EXPECT_EQ(done_expectation_.status, result.status);
    DataTypeStatusTable::TypeErrorMap errors =
        result.data_type_status_table.GetAllErrors();
    DataTypeStatusTable::TypeErrorMap expected_errors =
        done_expectation_.data_type_status_table.GetAllErrors();
    ASSERT_EQ(expected_errors.size(), errors.size());
    for (const auto& [type, error] : expected_errors) {
      ASSERT_TRUE(errors.find(type) != errors.end());
      ASSERT_EQ(error.error_type(), errors.find(type)->second.error_type());
    }
    done_expectation_ = DataTypeManager::ConfigureResult();
  }

  void OnConfigureStart() override {
    EXPECT_TRUE(start_expected_);
    start_expected_ = false;
    if (start_callback_) {
      std::move(start_callback_).Run();
    }
  }

 private:
  bool start_expected_ = true;
  base::OnceClosure start_callback_;
  DataTypeManager::ConfigureResult done_expectation_;
};

class FakeDataTypeEncryptionHandler : public DataTypeEncryptionHandler {
 public:
  FakeDataTypeEncryptionHandler() = default;
  ~FakeDataTypeEncryptionHandler() override = default;

  bool HasCryptoError() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  void set_crypto_error(bool crypto_error) { crypto_error_ = crypto_error; }
  void set_encrypted_types(ModelTypeSet encrypted_types) {
    encrypted_types_ = encrypted_types;
  }

 private:
  bool crypto_error_ = false;
  ModelTypeSet encrypted_types_;
};

bool FakeDataTypeEncryptionHandler::HasCryptoError() const {
  return crypto_error_;
}

ModelTypeSet FakeDataTypeEncryptionHandler::GetEncryptedDataTypes() const {
  return encrypted_types_;
}

}  // namespace

class SyncDataTypeManagerImplTest : public testing::Test {
 public:
  SyncDataTypeManagerImplTest() = default;
  ~SyncDataTypeManagerImplTest() override = default;

 protected:
  void SetUp() override { RecreateDataTypeManager(); }

  void RecreateDataTypeManager() {
    dtm_ = std::make_unique<DataTypeManagerImpl>(
        &controllers_, &encryption_handler_, &configurer_, &observer_);
  }

  void SetConfigureStartExpectation(
      base::OnceClosure start_callback = base::OnceClosure()) {
    observer_.ExpectStart(std::move(start_callback));
  }

  void SetConfigureDoneExpectation(DataTypeManager::ConfigureStatus status,
                                   const DataTypeStatusTable& status_table) {
    DataTypeManager::ConfigureResult result;
    result.status = status;
    result.data_type_status_table = status_table;
    observer_.ExpectDone(result);
  }

  // Configure the given DTM with the given desired types.
  void Configure(ModelTypeSet desired_types,
                 ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION) {
    dtm_->Configure(desired_types, BuildConfigureContext(reason));
  }

  void Configure(ModelTypeSet desired_types,
                 SyncMode sync_mode,
                 ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION) {
    dtm_->Configure(desired_types, BuildConfigureContext(reason, sync_mode));
  }

  // Finish downloading for the given DTM. Should be done only after
  // a call to Configure().
  void FinishDownload(ModelTypeSet types_to_configure,
                      ModelTypeSet failed_download_types) {
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    configurer_.FinishDownload(types_to_configure, failed_download_types);
  }

  void FinishDownloadWhileStopped(ModelTypeSet types_to_configure,
                                  ModelTypeSet failed_download_types) {
    configurer_.FinishDownload(types_to_configure, failed_download_types);
  }

  // Adds a fake controller for the given type to |controllers_|.
  // Should be called only before setting up the DTM.
  void AddController(ModelType model_type, bool enable_transport_mode = false) {
    controllers_[model_type] = std::make_unique<FakeDataTypeController>(
        model_type, enable_transport_mode);
  }

  // Gets the fake controller for the given type, which should have
  // been previously added via AddController().
  FakeDataTypeController* GetController(ModelType model_type) const {
    auto it = controllers_.find(model_type);
    if (it == controllers_.end()) {
      return nullptr;
    }
    return static_cast<FakeDataTypeController*>(it->second.get());
  }

  void FailEncryptionFor(ModelTypeSet encrypted_types) {
    encryption_handler_.set_crypto_error(true);
    encryption_handler_.set_encrypted_types(encrypted_types);
  }

  const ModelTypeConfigurer::ConfigureParams& last_configure_params() const {
    return configurer_.last_params();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  DataTypeController::TypeMap controllers_;
  FakeModelTypeConfigurer configurer_;
  FakeDataTypeManagerObserver observer_;
  std::unique_ptr<DataTypeManagerImpl> dtm_;
  FakeDataTypeEncryptionHandler encryption_handler_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_F(SyncDataTypeManagerImplTest, NoControllers) {
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, finish starting the controller, and then stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOne) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Empty());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThatSkipsEngineConnection) {
  AddController(BOOKMARKS);

  GetController(BOOKMARKS)
      ->model()
      ->EnableSkipEngineConnectionForActivationResponse();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));

  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // priority types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_TRUE(dtm_->GetActiveProxyDataTypes().Has(BOOKMARKS));

  // Even if all APIs above indicate the datatype is active, in reality the
  // configurer (SyncEngine) hasn't been activated/connected.
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with a single controller, configure it, but stop it
// before finishing the download.  It should still be safe to run the
// download callback even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileDownloadPending) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());
    EXPECT_TRUE(
        dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));

    dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.connected_types().Empty());
    EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Empty());
  }

  FinishDownloadWhileStopped(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(ModelTypeSet(BOOKMARKS));
    ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    ASSERT_EQ(DataTypeController::MODEL_STARTING,
              GetController(BOOKMARKS)->state());

    dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.connected_types().Empty());
    dtm_.reset();
  }

  EXPECT_EQ(DataTypeController::STOPPING, GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with a single controller.  Then:
//
//   1) Configure.
//   2) Finish the download for step 1.
//   3) The download determines a crypto error.
//   4) Complete download for the reconfiguration without the controller.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, OneWaitingForCrypto) {
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(PASSWORDS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // priority types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 3.
  FailEncryptionFor(ModelTypeSet(PASSWORDS));

  // Step 4.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(/*crypto_errors=*/ModelTypeSet(PASSWORDS),
                       ModelTypeSet(), ModelTypeSet()));
  Configure(ModelTypeSet(PASSWORDS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // priority types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Configure with both controllers.
//   4) Finish the download for step 3.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenBoth) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS),
            dtm_->GetTypesWithPendingDownloadForInitialSync());

  // Step 2.
  FinishDownload(ModelTypeSet(NIGORI), ModelTypeSet());  // control types
  EXPECT_EQ(ModelTypeSet(BOOKMARKS),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), dtm_->GetTypesWithPendingDownloadForInitialSync());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 3.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.connected_types());
  EXPECT_EQ(ModelTypeSet(PREFERENCES),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());

  // Step 4.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(ModelTypeSet(PREFERENCES),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), dtm_->GetTypesWithPendingDownloadForInitialSync());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Configure with second controller.
//   4) Finish the download for step 3.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenSwitch) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 3.
  Configure(ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(PREFERENCES), configurer_.connected_types());
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());

  // Step 4.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

TEST_F(SyncDataTypeManagerImplTest, ConfigureModelLoading) {
  // Set up a DTM with two controllers.
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1: Configure with first controller (model stays loading).
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());

  // Step 2: Configure with both controllers, which gets postponed because
  //         there's an ongoing configuration that cannot complete before the
  //         model loads.
  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  ASSERT_FALSE(dtm_->needs_reconfigure_for_test());
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(dtm_->needs_reconfigure_for_test());

  // Step 3: Finish starting the first controller. This triggers a
  //         reconfiguration with both data types.
  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_FALSE(dtm_->needs_reconfigure_for_test());

  // Step 4: Finish the download of both data types. This completes the
  //         configuration.
  ASSERT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.connected_types());

  // Step 5: Stop the DTM.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with one controller. Then configure and start the controller
// with a datatype error. DTM should proceed without the affected datatype.
TEST_F(SyncDataTypeManagerImplTest, OneFailingController) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::UNKNOWN,
      BuildStatusTable(ModelTypeSet(),
                       /*datatype_errors=*/ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());

  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ASSERT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  // This should be CONFIGURED but is not properly handled in
  // DataTypeManagerImpl::OnAllDataTypesReadyForConfigure().
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1.
//   4) Finish the download for step 2.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());

  // Step 2.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // regular types
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.connected_types());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1 with a failed data type.
//   4) Finish the download for step 2 successfully.
//   5) Stop the DTM.
//
// The failure from step 3 should be ignored since there's a
// reconfigure pending from step 2.
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPendingWithFailure) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.connected_types());

  // Step 2.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.connected_types());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Tests a Purge then Configure.  This is similar to the sequence of
// operations that would be invoked by the BackendMigrator.
TEST_F(SyncDataTypeManagerImplTest, MigrateAll) {
  AddController(PRIORITY_PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Initial setup.
  Configure(ModelTypeSet(PRIORITY_PREFERENCES));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());

  // We've now configured priority prefs and (implicitly) the control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Pretend we were told to migrate all types.
  ModelTypeSet to_migrate;
  to_migrate.Put(PRIORITY_PREFERENCES);
  to_migrate.PutAll(ControlTypes());

  EXPECT_EQ(0, GetController(PRIORITY_PREFERENCES)
                   ->model()
                   ->clear_metadata_call_count());
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->PurgeForMigration(to_migrate);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(1, GetController(PRIORITY_PREFERENCES)
                   ->model()
                   ->clear_metadata_call_count());

  // The DTM will call ConfigureDataTypes(), even though it is unnecessary.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // no enabled types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Re-enable the migrated types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(to_migrate);
  FinishDownload(ControlTypes(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1, GetController(PRIORITY_PREFERENCES)
                   ->model()
                   ->clear_metadata_call_count());
}

// Test receipt of a Configure request while a purge is in flight.
TEST_F(SyncDataTypeManagerImplTest, ConfigureDuringPurge) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Purge the Nigori type.
  SetConfigureStartExpectation();
  dtm_->PurgeForMigration(ModelTypeSet(NIGORI));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  observer_.ResetExpectations();

  // Before the backend configuration completes, ask for a different
  // set of types.  This request asks for
  // - BOOKMARKS: which is redundant because it was already enabled,
  // - PREFERENCES: which is new and will need to be downloaded, and
  // - NIGORI: (added implicitly because it is a control type) which
  //   the DTM is part-way through purging.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Invoke the callback we've been waiting for since we asked to purge NIGORI.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // regular types
  observer_.ResetExpectations();

  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Now invoke the callback for the second configure request.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
  EXPECT_EQ(0,
            GetController(PREFERENCES)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfiguration) {
  // The order of priorities is:
  // 1. Control types, i.e. NIGORI - included implicitly.
  // 2. Priority types.
  AddController(PRIORITY_PREFERENCES);
  // 3. Regular types.
  AddController(BOOKMARKS);
  // 4. Low-priority types.
  AddController(HISTORY);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Start the configuration.
  ASSERT_EQ(0, configurer_.configure_call_count());
  Configure(ModelTypeSet(BOOKMARKS, HISTORY, PRIORITY_PREFERENCES));
  // This causes an immediate ConfigureDataTypes() call for control types, i.e.
  // Nigori. It's important that this does *not* ask for any types to be
  // downloaded, see crbug.com/1170318 and crbug.com/1187914.
  ASSERT_NE(0, configurer_.configure_call_count());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Finishing the no-op download of the control types causes the next
  // ConfigureDataTypes() call, for priority types.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(AddControlTypesTo(PRIORITY_PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is downloaded after PRIORITY_PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);

  // HISTORY is downloaded after BOOKMARKS finishes.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(HISTORY), last_configure_params().to_download);

  FinishDownload(ModelTypeSet(HISTORY), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationReconfigure) {
  AddController(PRIORITY_PREFERENCES);
  AddController(BOOKMARKS);
  AddController(APPS);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Start a configuration with BOOKMARKS and PRIORITY_PREFERENCES, and finish
  // the download of PRIORITY_PREFERENCES.
  Configure(ModelTypeSet(BOOKMARKS, PRIORITY_PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(AddControlTypesTo(PRIORITY_PREFERENCES),
            last_configure_params().to_download);

  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);

  // Enable syncing for APPS while the download of BOOKMARKS is still pending.
  Configure(ModelTypeSet(BOOKMARKS, PRIORITY_PREFERENCES, APPS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfiguration starts after downloading of previous types finishes.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  // Priority types: Nothing to download, since PRIORITY_PREFERENCES was
  // downloaded before.
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Regular types: Only the newly-enabled APPS still needs to be downloaded.
  EXPECT_EQ(AddControlTypesTo(APPS), last_configure_params().to_download);

  FinishDownload(ModelTypeSet(BOOKMARKS, APPS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationStop) {
  AddController(PRIORITY_PREFERENCES);
  AddController(BOOKMARKS);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED, DataTypeStatusTable());

  // Initially only PRIORITY_PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PRIORITY_PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(AddControlTypesTo(PRIORITY_PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PRIORITY_PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);

  // PRIORITY_PREFERENCES controller is running while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationDownloadError) {
  AddController(PRIORITY_PREFERENCES);
  AddController(BOOKMARKS);

  // Initial configure. Bookmarks will fail to associate due to the download
  // failure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(),
                       /*datatype_errors=*/ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));

  // Initially only PRIORITY_PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PRIORITY_PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(AddControlTypesTo(PRIORITY_PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PRIORITY_PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);

  // PRIORITY_PREFERENCES controller is running while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Make BOOKMARKS download fail. PRIORITY_PREFERENCES is still running.
  FinishDownload(ModelTypeSet(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());

  // Finish downloading of PRIORITY_PREFERENCES. This will trigger a
  // reconfiguration to disable bookmarks.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, FilterDesiredTypes) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure({BOOKMARKS, APPS});
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, FailingPreconditionKeepData) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS)));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.connected_types().Size());
  observer_.ResetExpectations();

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());

  // Should do nothing.
  observer_.ResetExpectations();
  dtm_->DataTypePreconditionChanged(BOOKMARKS);

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());

  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, FailingPreconditionClearData) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);

  // Bookmarks is never started due to failing preconditions.
  DataTypeStatusTable::TypeErrorMap error_map;
  error_map[BOOKMARKS] =
      SyncError(FROM_HERE, SyncError::DATATYPE_POLICY_ERROR, "", BOOKMARKS);
  DataTypeStatusTable expected_status_table;
  expected_status_table.UpdateFailedDataTypes(error_map);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, expected_status_table);

  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types

  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.connected_types().Size());

  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

// Tests that unready types are not started after ResetDataTypeErrors and
// reconfiguration.
TEST_F(SyncDataTypeManagerImplTest, UnreadyTypeResetReconfigure) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS)));
  Configure(ModelTypeSet(BOOKMARKS));
  // Second Configure sets a flag to perform reconfiguration after the first one
  // is done.
  Configure(ModelTypeSet(BOOKMARKS));

  // Reset errors before triggering reconfiguration.
  dtm_->ResetDataTypeErrors();

  // Reconfiguration should update unready errors. Bookmarks shouldn't start.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // regular types
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.connected_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest, UnreadyTypeLaterReady) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS)));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().Size());

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  // Set the expectations for the reconfiguration - no unready errors now.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest,
       MultipleUnreadyTypesLaterReadyAtTheSameTime) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);
  GetController(PREFERENCES)
      ->SetPreconditionState(
          DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Both types are never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(
          ModelTypeSet(), ModelTypeSet(),
          /*unready_errors=*/ModelTypeSet(BOOKMARKS, PREFERENCES)));
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().Size());

  // Both types should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  GetController(PREFERENCES)
      ->SetPreconditionState(
          DataTypeController::PreconditionState::kPreconditionsMet);

  // Just triggering state change for one of them causes reconfiguration for all
  // that are ready to start (which is both BOOKMARKS and PREFERENCES).
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());

  // Set new expectations for the reconfiguration - no unready errors any more.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.connected_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest, MultipleUnreadyTypesLaterOneOfThemReady) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);
  GetController(PREFERENCES)
      ->SetPreconditionState(
          DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Both types are never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(
          ModelTypeSet(), ModelTypeSet(),
          /*unready_errors=*/ModelTypeSet(BOOKMARKS, PREFERENCES)));
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().Size());

  // Bookmarks should start normally now. Preferences should still not start.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());

  // Set the expectations for the reconfiguration - just prefs are unready now.
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(PREFERENCES)));

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest,
       NoOpDataTypePreconditionChangedWhileStillUnready) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS)));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().Size());

  // Bookmarks is still unready so DataTypePreconditionChanged() should be
  // ignored.
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest,
       NoOpDataTypePreconditionChangedWhileStillReady) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Bookmarks is still ready so DataTypePreconditionChanged() should be
  // ignored.
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, ModelLoadError) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));

  // Bookmarks is never started due to hitting a model load error.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(),
                       /*datatype_errors=*/ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  // No need to finish the download of BOOKMARKS since it was never started.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.connected_types().Size());
}

// Checks that DTM handles the case when a controller is already in a FAILED
// state at the time the DTM is created. Regression test for crbug.com/967344.
TEST_F(SyncDataTypeManagerImplTest, ErrorBeforeStartup) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Produce an error (FAILED) state in the BOOKMARKS controller.
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));
  SetConfigureStartExpectation();
  Configure({BOOKMARKS});
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // Now create a fresh DTM, simulating a Sync restart.
  RecreateDataTypeManager();

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // Now a configuration attempt for both types should complete successfully,
  // but exclude the failed type.
  SetConfigureStartExpectation();
  Configure({BOOKMARKS, PREFERENCES});

  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(/*crypto_errors=*/{},
                                               /*datatype_errors=*/{BOOKMARKS},
                                               /*unready_errors=*/{}));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());

  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(PREFERENCES));
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_FALSE(
      dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));
}

// Test that sync configures properly if all types are already downloaded.
TEST_F(SyncDataTypeManagerImplTest, AllTypesReady) {
  AddController(PRIORITY_PREFERENCES);
  AddController(BOOKMARKS);

  // Mark both types as already downloaded.
  sync_pb::ModelTypeState already_downloaded;
  already_downloaded.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  GetController(PRIORITY_PREFERENCES)
      ->model()
      ->SetModelTypeStateForActivationResponse(already_downloaded);
  GetController(BOOKMARKS)->model()->SetModelTypeStateForActivationResponse(
      already_downloaded);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(PRIORITY_PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Both types were downloaded already, so they aren't downloading initial
  // data even during the CONFIGURING state.
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Empty());

  // This started the configuration of control types, which aren't tracked by
  // DataTypeManagerImpl, so always considered already downloaded.
  ASSERT_EQ(1, configurer_.configure_call_count());
  EXPECT_TRUE(last_configure_params().to_download.Empty());

  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Finish downloading (configuring, really) control types.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  // This started the configuration of priority types, i.e.
  // PRIORITY_PREFERENCES, which is already downloaded.
  ASSERT_EQ(2, configurer_.configure_call_count());
  EXPECT_TRUE(last_configure_params().to_download.Empty());

  // Finish downloading (configuring, really) priority types.
  FinishDownload(ModelTypeSet(PRIORITY_PREFERENCES), ModelTypeSet());
  // This started the configuration of regular types, i.e. BOOKMARKS, which is
  // already downloaded.
  ASSERT_EQ(3, configurer_.configure_call_count());
  EXPECT_TRUE(last_configure_params().to_download.Empty());

  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finish downloading (configuring, really) regular types. This finishes the
  // configuration.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  ASSERT_EQ(3, configurer_.configure_call_count());  // Not increased.

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.connected_types().Size());
  EXPECT_TRUE(dtm_->GetActiveProxyDataTypes().Empty());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Empty());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
}

// Test that DataTypeManagerImpl delays configuration until all data types
// loaded their models.
TEST_F(SyncDataTypeManagerImplTest, DelayConfigureForUSSTypes) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Bookmarks model isn't loaded yet and it is required to complete before
  // call to configure. Ensure that configure wasn't called.
  EXPECT_EQ(0, configurer_.configure_call_count());
  EXPECT_EQ(0, GetController(BOOKMARKS)->activate_call_count());

  // Finishing model load should trigger configure.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(1, configurer_.configure_call_count());
  EXPECT_EQ(1, GetController(BOOKMARKS)->activate_call_count());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());
}

// Test that when encryption fails for a given type, the corresponding
// data type is not activated.
TEST_F(SyncDataTypeManagerImplTest, ConnectDataTypeOnEncryptionError) {
  AddController(BOOKMARKS);
  AddController(PASSWORDS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(PASSWORDS)->model()->EnableManualModelStart();
  SetConfigureStartExpectation();

  FailEncryptionFor(ModelTypeSet(BOOKMARKS));
  Configure(ModelTypeSet(BOOKMARKS, PASSWORDS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(PASSWORDS)->state());
  EXPECT_EQ(0, GetController(BOOKMARKS)->activate_call_count());
  EXPECT_EQ(0, GetController(PASSWORDS)->activate_call_count());

  GetController(PASSWORDS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(0, GetController(BOOKMARKS)->activate_call_count());
  EXPECT_EQ(1, GetController(PASSWORDS)->activate_call_count());
}

// Test that Connect is not called for datatypes that failed
// LoadModels().
TEST_F(SyncDataTypeManagerImplTest, ConnectDataTypeAfterLoadModelsError) {
  // Initiate configuration for two datatypes but block them at LoadModels.
  AddController(BOOKMARKS);
  AddController(PASSWORDS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(PASSWORDS)->model()->EnableManualModelStart();
  SetConfigureStartExpectation();
  Configure(ModelTypeSet(BOOKMARKS, PASSWORDS));
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(PASSWORDS)->state());

  // Make bookmarks fail LoadModels. Passwords load normally.
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));
  GetController(PASSWORDS)->model()->SimulateModelStartFinished();

  // Connect should be called for passwords, but not bookmarks.
  EXPECT_EQ(0, GetController(BOOKMARKS)->activate_call_count());
  EXPECT_EQ(1, GetController(PASSWORDS)->activate_call_count());
}

// Test that Stop with DISABLE_SYNC_AND_CLEAR_DATA calls DTC Stop with
// CLEAR_METADATA for active data types.
TEST_F(SyncDataTypeManagerImplTest, StopWithDisableSync) {
  AddController(BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  dtm_->Stop(SyncStopMetadataFate::CLEAR_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().Empty());
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PurgeDataOnStarting) {
  AddController(BOOKMARKS);
  AddController(AUTOFILL_WALLET_DATA, /*enable_transport_mode=*/true);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.connected_types().Size());

  // The user temporarily turns off Sync.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(DataTypeManager::STOPPED, dtm_->state());
  ASSERT_TRUE(configurer_.connected_types().Empty());
  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());

  // Now we restart with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.connected_types().Size());

  // This should have purged the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Has(BOOKMARKS));
  // Stop(CLEAR_METADATA) has *not* been called on the controller though; that
  // happens only when stopping or reconfiguring, not when (re)starting without
  // the type.
  // TODO(crbug.com/897628): Metadata *should* probably be cleared here.
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PurgeDataOnReconfiguring) {
  AddController(BOOKMARKS);
  controllers_[AUTOFILL_WALLET_DATA] = std::make_unique<FakeDataTypeController>(
      AUTOFILL_WALLET_DATA,
      /*enable_transport_only_model=*/true);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.connected_types().Size());

  // Now we reconfigure with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.connected_types().Size());

  // This should have purged the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Has(BOOKMARKS));
  // Also Stop(CLEAR_METADATA) has been called on the controller since the type
  // is no longer enabled.
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, ShouldRecordInitialConfigureTimeHistogram) {
  base::HistogramTester histogram_tester;
  AddController(BOOKMARKS);

  // Configure as first sync.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS), SyncMode::kFull,
            CONFIGURE_REASON_NEW_CLIENT);

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  histogram_tester.ExpectTotalCount("Sync.ConfigureTime_Initial.OK", 1);
}

TEST_F(SyncDataTypeManagerImplTest,
       ShouldRecordSubsequentConfigureTimeHistogram) {
  base::HistogramTester histogram_tester;
  AddController(BOOKMARKS);
  // Configure as subsequent sync.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS), SyncMode::kFull,
            CONFIGURE_REASON_RECONFIGURATION);

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  histogram_tester.ExpectTotalCount("Sync.ConfigureTime_Subsequent.OK", 1);
}

// Regression test for crbug.com/1286204: Reentrant calls to Configure()
// shouldn't crash (or trigger DCHECKs).
TEST_F(SyncDataTypeManagerImplTest, ReentrantConfigure) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  // The DataTypeManagerObserver::OnConfigureStart() call may, in some cases,
  // result in a reentrant call to Configure().
  SetConfigureStartExpectation(base::BindLambdaForTesting(
      [&]() { Configure(ModelTypeSet(PREFERENCES)); }));

  Configure(ModelTypeSet(PREFERENCES, BOOKMARKS));
  // Implicit expectation: No crash here!

  // Eventually, the second (reentrant) Configure() call should win, i.e. here
  // only PREFERENCES gets configured.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest, ProvideDebugInfo) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  // Mark BOOKMARKS as already downloaded.
  sync_pb::ModelTypeState bookmarks_state;
  bookmarks_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  GetController(BOOKMARKS)->model()->SetModelTypeStateForActivationResponse(
      bookmarks_state);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(PREFERENCES, BOOKMARKS));
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Because Bookmarks are already downloaded, configuration finishes as soon
  // as preferences are downloaded.
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, ShouldDoNothingForAlreadyFailedTypes) {
  // Bring the type to FAILED state.
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));

  // Bookmarks is never started due to hitting a model load error.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(),
                       /*datatype_errors=*/ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  // No need to finish the download of BOOKMARKS since it was never started.
  ASSERT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  dtm_->OnSingleDataTypeWillStop(
      BOOKMARKS,
      SyncError(FROM_HERE, SyncError::DATATYPE_ERROR, "Test error", BOOKMARKS));
  EXPECT_FALSE(dtm_->needs_reconfigure_for_test());
}

// Tests that data types which time out are ultimately skipped during
// configuration.
TEST_F(SyncDataTypeManagerImplTest, ShouldFinishConfigureIfSomeTypesTimeout) {
  // Create two controllers, but one with a delayed model load.
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));

  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));

  // BOOKMARKS blocks configuration.
  EXPECT_TRUE(configurer_.connected_types().Empty());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  // Fast-forward to time out.
  task_environment_.FastForwardBy(kSyncLoadModelsTimeoutDuration.Get());

  // BOOKMARKS is ignored and PREFERENCES is connected.
  EXPECT_EQ(configurer_.connected_types(), ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet(BOOKMARKS));
  // BOOKMARKS is skipped and signalled to stop.
  EXPECT_EQ(DataTypeController::STOPPING, GetController(BOOKMARKS)->state());
  // DataTypeManager will be notified for reconfiguration.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());  // control types
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  // DataTypeManager finishes configuration.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

}  // namespace syncer
