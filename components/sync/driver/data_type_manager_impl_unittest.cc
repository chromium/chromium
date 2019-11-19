// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_manager_impl.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Helpers for unioning with control types.
ModelTypeSet AddControlTypesTo(ModelType type) {
  return Union(ControlTypes(), ModelTypeSet(type));
}

ModelTypeSet AddControlTypesTo(ModelTypeSet types) {
  return Union(ControlTypes(), types);
}

ConfigureContext BuildConfigureContext(ConfigureReason reason,
                                       SyncMode sync_mode = SyncMode::kFull) {
  ConfigureContext context;
  context.reason = reason;
  context.sync_mode = sync_mode;
  return context;
}

DataTypeStatusTable BuildStatusTable(ModelTypeSet crypto_errors,
                                     ModelTypeSet association_errors,
                                     ModelTypeSet unready_errors,
                                     ModelTypeSet unrecoverable_errors) {
  DataTypeStatusTable::TypeErrorMap error_map;
  for (ModelType type : crypto_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::CRYPTO_ERROR,
                                "crypto error expected", type);
  }
  for (ModelType type : association_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                "association error expected", type);
  }
  for (ModelType type : unready_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::UNREADY_ERROR,
                                "unready error expected", type);
  }
  for (ModelType type : unrecoverable_errors) {
    error_map[type] = SyncError(FROM_HERE, SyncError::UNRECOVERABLE_ERROR,
                                "unrecoverable error expected", type);
  }
  DataTypeStatusTable status_table;
  status_table.UpdateFailedDataTypes(error_map);
  return status_table;
}

// Fake ModelTypeConfigurer implementation that simply stores away the
// callback passed into ConfigureDataTypes.
class FakeModelTypeConfigurer : public ModelTypeConfigurer {
 public:
  FakeModelTypeConfigurer() {}
  ~FakeModelTypeConfigurer() override {}

  void ConfigureDataTypes(ConfigureParams params) override {
    configure_call_count_++;
    last_params_ = std::move(params);
  }

  void RegisterDirectoryDataType(ModelType type,
                                 ModelSafeGroup group) override {
    registered_directory_types_.Put(type);
  }

  void UnregisterDirectoryDataType(ModelType type) override {
    registered_directory_types_.Remove(type);
  }

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override {
    activated_types_.Put(type);
  }

  void DeactivateDirectoryDataType(ModelType type) override {
    activated_types_.Remove(type);
  }

  void ActivateNonBlockingDataType(ModelType type,
                                   std::unique_ptr<DataTypeActivationResponse>
                                       activation_response) override {
    // TODO(stanisc): crbug.com/515962: Add test coverage.
  }

  void DeactivateNonBlockingDataType(ModelType type) override {
    // TODO(stanisc): crbug.com/515962: Add test coverage.
  }

  const ModelTypeSet registered_directory_types() {
    return registered_directory_types_;
  }

  const ModelTypeSet activated_types() { return activated_types_; }

  int configure_call_count() const { return configure_call_count_; }

  const ConfigureParams& last_params() const { return last_params_; }

 private:
  ModelTypeSet registered_directory_types_;
  ModelTypeSet activated_types_;
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
    EXPECT_TRUE(
        done_expectation_.data_type_status_table.GetFailedTypes().Empty());
  }

  void ExpectStart() { start_expected_ = true; }
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
    for (DataTypeStatusTable::TypeErrorMap::const_iterator iter =
             expected_errors.begin();
         iter != expected_errors.end(); ++iter) {
      ASSERT_TRUE(errors.find(iter->first) != errors.end());
      ASSERT_EQ(iter->second.error_type(),
                errors.find(iter->first)->second.error_type());
    }
    done_expectation_ = DataTypeManager::ConfigureResult();
  }

  void OnConfigureStart() override {
    EXPECT_TRUE(start_expected_);
    start_expected_ = false;
  }

 private:
  bool start_expected_ = true;
  DataTypeManager::ConfigureResult done_expectation_;
};

class FakeDataTypeEncryptionHandler : public DataTypeEncryptionHandler {
 public:
  FakeDataTypeEncryptionHandler();
  ~FakeDataTypeEncryptionHandler() override;

  bool HasCryptoError() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  void set_crypto_error(bool crypto_error) { crypto_error_ = crypto_error; }
  void set_encrypted_types(ModelTypeSet encrypted_types) {
    encrypted_types_ = encrypted_types;
  }

 private:
  bool crypto_error_;
  ModelTypeSet encrypted_types_;
};

FakeDataTypeEncryptionHandler::FakeDataTypeEncryptionHandler()
    : crypto_error_(false) {}
FakeDataTypeEncryptionHandler::~FakeDataTypeEncryptionHandler() {}

bool FakeDataTypeEncryptionHandler::HasCryptoError() const {
  return crypto_error_;
}

ModelTypeSet FakeDataTypeEncryptionHandler::GetEncryptedDataTypes() const {
  return encrypted_types_;
}

}  // namespace

class TestDataTypeManager : public DataTypeManagerImpl {
 public:
  using DataTypeManagerImpl::DataTypeManagerImpl;

  void set_priority_types(const ModelTypeSet& priority_types) {
    custom_priority_types_ = priority_types;
  }

  DataTypeManager::ConfigureResult configure_result() const {
    return configure_result_;
  }

  void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) override {
    configure_result_ = result;
    DataTypeManagerImpl::OnModelAssociationDone(result);
  }

  void set_downloaded_types(ModelTypeSet downloaded_types) {
    downloaded_types_ = downloaded_types;
  }

 protected:
  ModelTypeSet GetPriorityTypes() const override {
    return custom_priority_types_;
  }

 private:
  ModelTypeSet custom_priority_types_ = ControlTypes();
  DataTypeManager::ConfigureResult configure_result_;
};

// The actual test harness class, parametrized on nigori state (i.e., tests are
// run both configuring with nigori, and configuring without).
class SyncDataTypeManagerImplTest : public testing::Test {
 public:
  SyncDataTypeManagerImplTest() {}

  ~SyncDataTypeManagerImplTest() override {}

 protected:
  void SetUp() override { RecreateDataTypeManager(); }

  void RecreateDataTypeManager() {
    dtm_ = std::make_unique<TestDataTypeManager>(
        ModelTypeSet(), WeakHandle<DataTypeDebugInfoListener>(), &controllers_,
        &encryption_handler_, &configurer_, &observer_);
  }

  void SetConfigureStartExpectation() { observer_.ExpectStart(); }

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
    ASSERT_FALSE(last_configure_params().ready_task.is_null());
    last_configure_params().ready_task.Run(
        Difference(types_to_configure, failed_download_types),
        failed_download_types);
  }

  // Adds a fake controller for the given type to |controllers_|.
  // Should be called only before setting up the DTM.
  void AddController(ModelType model_type) {
    controllers_[model_type] =
        std::make_unique<FakeDataTypeController>(model_type);
  }

  // Convenience method to create a controller and set some parameters.
  void AddController(ModelType model_type,
                     bool should_load_model_before_configure,
                     bool should_delay_model_load) {
    AddController(model_type);
    GetController(model_type)
        ->SetShouldLoadModelBeforeConfigure(should_load_model_before_configure);
    if (should_delay_model_load) {
      GetController(model_type)->SetDelayModelLoad();
    }
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
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  DataTypeController::TypeMap controllers_;
  FakeModelTypeConfigurer configurer_;
  FakeDataTypeManagerObserver observer_;
  std::unique_ptr<TestDataTypeManager> dtm_;
  FakeDataTypeEncryptionHandler encryption_handler_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_F(SyncDataTypeManagerImplTest, NoControllers) {
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop(STOP_SYNC);
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
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
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
    EXPECT_EQ(ModelTypeSet(BOOKMARKS),
              configurer_.registered_directory_types());

    dtm_->Stop(STOP_SYNC);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.registered_directory_types().Empty());
  }

  last_configure_params().ready_task.Run(ModelTypeSet(BOOKMARKS),
                                         ModelTypeSet());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_EQ(ModelTypeSet(BOOKMARKS),
              configurer_.registered_directory_types());
    FinishDownload(ModelTypeSet(), ModelTypeSet());
    FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    dtm_->Stop(STOP_SYNC);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.registered_directory_types().Empty());
    dtm_.reset();
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, start the controller's model, but stop the DTM before
// the controller finishes starting up.  It should still be safe to
// finish starting up the controller even after the DTM is stopped and
// destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileAssociating) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_EQ(ModelTypeSet(BOOKMARKS),
              configurer_.registered_directory_types());

    FinishDownload(ModelTypeSet(), ModelTypeSet());
    FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_TRUE(configurer_.activated_types().Empty());

    dtm_->Stop(STOP_SYNC);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.registered_directory_types().Empty());
    dtm_.reset();
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller.  Then:
//
//   1) Configure.
//   2) Finish the download for step 1.
//   3) Finish starting the controller with the NEEDS_CRYPTO status.
//   4) Complete download for the reconfiguration without the controller.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, OneWaitingForCrypto) {
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(PASSWORDS), ModelTypeSet(), ModelTypeSet(),
                       ModelTypeSet()));

  const ModelTypeSet types(PASSWORDS);
  dtm_->set_priority_types(AddControlTypesTo(types));

  // Step 1.
  Configure(types);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FailEncryptionFor(types);
  GetController(PASSWORDS)->FinishStart(DataTypeController::NEEDS_CRYPTO);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller.
//   4) Configure with both controllers.
//   5) Finish the download for step 4.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenBoth) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 4.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());

  // Step 5.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  // Step 7.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller.
//   4) Configure with second controller.
//   5) Finish the download for step 4.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenSwitch) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 4.
  Configure(ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(PREFERENCES),
            configurer_.registered_directory_types());
  EXPECT_EQ(1, GetController(BOOKMARKS)->clear_metadata_call_count());

  // Step 5.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  // Step 7.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Configure with both controllers.
//   4) Finish starting the first controller.
//   5) Finish the download for step 3.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileOneInFlight) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());

  // Step 7.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with one controller.  Then configure, finish
// downloading, and start the controller with an unrecoverable error.
// The unrecoverable error should cause the DTM to stop.
TEST_F(SyncDataTypeManagerImplTest, OneFailingController) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::UNRECOVERABLE_ERROR,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(), ModelTypeSet(),
                       ModelTypeSet(BOOKMARKS)));

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());

  GetController(BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with both controllers.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller successfully.
//   4) Finish starting the second controller with an unrecoverable error.
//
// The failure from step 4 should cause the DTM to stop.
TEST_F(SyncDataTypeManagerImplTest, SecondControllerFails) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::UNRECOVERABLE_ERROR,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(), ModelTypeSet(),
                       ModelTypeSet(PREFERENCES)));

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(PREFERENCES)
      ->FinishStart(DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with both controllers.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller successfully.
//   4) Finish starting the second controller with an association failure.
//   5) Finish the purge/reconfigure without the failed type.
//   6) Stop the DTM.
//
// The association failure from step 3 should be ignored.
//
// TODO(akalin): Check that the data type that failed association is
// recorded in the CONFIGURE_DONE notification.
TEST_F(SyncDataTypeManagerImplTest, OneControllerFailsAssociation) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(PREFERENCES),
                       ModelTypeSet(), ModelTypeSet()));

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());

  // Step 2.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(PREFERENCES)
      ->FinishStart(DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 6.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
  EXPECT_EQ(0, GetController(PREFERENCES)->clear_metadata_call_count());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1.
//   4) Finish the download for step 2.
//   5) Finish starting both controllers.
//   6) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 2.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());

  // Step 6.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1 with a failed data type.
//   4) Finish the download for step 2 successfully.
//   5) Finish starting both controllers.
//   6) Stop the DTM.
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
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), configurer_.registered_directory_types());

  // Step 2.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            configurer_.registered_directory_types());

  // Step 6.
  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.registered_directory_types().Empty());
}

// Tests a Purge then Configure.  This is similar to the sequence of
// operations that would be invoked by the BackendMigrator.
TEST_F(SyncDataTypeManagerImplTest, MigrateAll) {
  AddController(BOOKMARKS);
  dtm_->set_priority_types(AddControlTypesTo(BOOKMARKS));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Initial setup.
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  // We've now configured bookmarks and (implicitly) the control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Pretend we were told to migrate all types.
  ModelTypeSet to_migrate;
  to_migrate.Put(BOOKMARKS);
  to_migrate.PutAll(ControlTypes());

  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->PurgeForMigration(to_migrate);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(1, GetController(BOOKMARKS)->clear_metadata_call_count());

  // The DTM will call ConfigureDataTypes(), even though it is unnecessary.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Re-enable the migrated types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(to_migrate);
  FinishDownload(to_migrate, ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1, GetController(BOOKMARKS)->clear_metadata_call_count());
}

// Test receipt of a Configure request while a purge is in flight.
TEST_F(SyncDataTypeManagerImplTest, ConfigureDuringPurge) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
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
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  observer_.ResetExpectations();

  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Now invoke the callback for the second configure request.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Start the preferences controller.  We don't need to start controller for
  // the NIGORI because it has none.  We don't need to start the controller for
  // the BOOKMARKS because it was never stopped.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
  EXPECT_EQ(0, GetController(PREFERENCES)->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfiguration) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Initially only PREFERENCES is downloaded.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is downloaded after PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationReconfigure) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);
  AddController(APPS);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Reconfigure while associating PREFERENCES and downloading BOOKMARKS.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  // Enable syncing for APPS.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES, APPS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfiguration starts after downloading and association of previous
  // types finish.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);

  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(APPS, NIGORI), last_configure_params().to_download);

  FinishDownload(ModelTypeSet(BOOKMARKS, APPS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Skip calling FinishStart() for PREFENCES because it's already started in
  // first configuration.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(APPS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationStop) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED, DataTypeStatusTable());

  // Initially only PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationDownloadError) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure. Bookmarks will fail to associate due to the download
  // failure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS), ModelTypeSet(),
                       ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Make BOOKMARKS download fail. Preferences is still associating.
  FinishDownload(ModelTypeSet(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());

  // Finish association of PREFERENCES. This will trigger a reconfiguration to
  // disable bookmarks.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, HighPriorityAssociationFailure) {
  AddController(PREFERENCES);  // Will fail.
  AddController(BOOKMARKS);    // Will succeed.

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(PREFERENCES),
                       ModelTypeSet(), ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Make PREFERENCES association fail.
  GetController(PREFERENCES)
      ->FinishStart(DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfigure without PREFERENCES after the BOOKMARKS download completes,
  // then reconfigure with BOOKMARKS.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  FinishDownload(ModelTypeSet(), ModelTypeSet());

  // Reconfigure with BOOKMARKS.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING, GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, LowPriorityAssociationFailure) {
  AddController(PREFERENCES);  // Will succeed.
  AddController(BOOKMARKS);    // Will fail.

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS), ModelTypeSet(),
                       ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo(PREFERENCES),
            last_configure_params().to_download);

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, NIGORI),
            last_configure_params().to_download);

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // BOOKMARKS finishes downloading and PREFERENCES finishes associating.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());

  // Make BOOKMARKS association fail, which triggers reconfigure with only
  // PREFERENCES.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finish configuration with only PREFERENCES.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(ModelTypeSet(), last_configure_params().to_download);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, FilterDesiredTypes) {
  AddController(BOOKMARKS);

  ModelTypeSet types(BOOKMARKS, APPS);
  dtm_->set_priority_types(AddControlTypesTo(types));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(types);
  EXPECT_EQ(AddControlTypesTo(BOOKMARKS), last_configure_params().to_download);
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  dtm_->Stop(STOP_SYNC);
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
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(), ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.activated_types().Size());
  observer_.ResetExpectations();

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  // Should do nothing.
  observer_.ResetExpectations();
  dtm_->DataTypePreconditionChanged(BOOKMARKS);

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(), ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  // Second Configure sets a flag to perform reconfiguration after the first one
  // is done.
  Configure(ModelTypeSet(BOOKMARKS));

  // Reset errors before triggering reconfiguration.
  dtm_->ResetDataTypeErrors();

  // Reconfiguration should update unready errors. Bookmarks shouldn't start.
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.activated_types().Size());
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
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.activated_types().Size());

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  // Set the expectations for the reconfiguration - no unready errors now.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
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
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS, PREFERENCES),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.activated_types().Size());

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

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());

  // Set new expectations for the reconfiguration - no unready errors any more.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());
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
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(BOOKMARKS, PREFERENCES),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS, PREFERENCES));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.activated_types().Size());

  // Bookmarks should start normally now. Preferences should still not start.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  // Set the expectations for the reconfiguration - just prefs are unready now.
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(),
                       /*unready_errors=*/ModelTypeSet(PREFERENCES),
                       ModelTypeSet()));
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
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
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(), ModelTypeSet(BOOKMARKS),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.activated_types().Size());

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
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
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
  GetController(BOOKMARKS)->SetModelLoadError(
      SyncError(FROM_HERE, SyncError::DATATYPE_ERROR, "load error", BOOKMARKS));

  // Bookmarks is never started due to hitting a model load error.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS), ModelTypeSet(),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.activated_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest, ErrorBeforeAssociation) {
  AddController(BOOKMARKS);

  // Bookmarks is never started due to hitting a datatype error while the DTM
  // is still downloading types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS), ModelTypeSet(),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  GetController(BOOKMARKS)->CreateErrorHandler()->OnUnrecoverableError(
      SyncError(FROM_HERE, SyncError::DATATYPE_ERROR, "bookmarks error",
                BOOKMARKS));
  base::RunLoop().RunUntilIdle();
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  FinishDownload(ModelTypeSet(), ModelTypeSet());  // Reconfig for error.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.activated_types().Size());
}

// Checks that DTM handles the case when a controller is already in a FAILED
// state at the time the DTM is created. Regression test for crbug.com/967344.
TEST_F(SyncDataTypeManagerImplTest, ErrorBeforeStartup) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Produce an error (FAILED) state in the BOOKMARKS controller.
  GetController(BOOKMARKS)->SetModelLoadError(SyncError(
      FROM_HERE, SyncError::DATATYPE_ERROR, "bookmarks error", BOOKMARKS));
  SetConfigureStartExpectation();
  Configure({BOOKMARKS});
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // Now create a fresh DTM, simulating a Sync restart.
  RecreateDataTypeManager();

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // Now a configuration attempt for both types should complete successfully,
  // but exclude the failed type.
  SetConfigureStartExpectation();
  Configure({BOOKMARKS, PREFERENCES});

  SetConfigureDoneExpectation(
      DataTypeManager::OK, BuildStatusTable(/*crypto_errors=*/{},
                                            /*association_errors=*/{BOOKMARKS},
                                            /*unready_errore=*/{},
                                            /*unrecoverable_errors=*/{}));
  FinishDownload(/*types_to_configure=*/{},
                 /*failed_download_types=*/{});
  FinishDownload(/*types_to_configure=*/{PREFERENCES},
                 /*failed_download_types=*/{});
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);

  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(PREFERENCES));
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
}

TEST_F(SyncDataTypeManagerImplTest, AssociationNeverCompletes) {
  AddController(BOOKMARKS);

  // Bookmarks times out during association and so it's never started.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(
      DataTypeManager::OK,
      BuildStatusTable(ModelTypeSet(), ModelTypeSet(BOOKMARKS), ModelTypeSet(),
                       ModelTypeSet()));
  Configure(ModelTypeSet(BOOKMARKS));

  GetController(BOOKMARKS)->SetDelayModelLoad();
  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Simulate timeout by firing the timer.
  dtm_->GetModelAssociationManagerForTesting()->GetTimerForTesting()->FireNow();
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.activated_types().Size());
}

// Test that sync configures properly if all low priority types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllLowPriorityTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  dtm_->set_downloaded_types(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());

  // Association of Bookmarks can't happen until higher priority types are
  // finished.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks are a ready type, once Preference finishes, Bookmarks
  // can start associating immediately (even before the ModelTypeConfigurer
  // calls back).
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::ASSOCIATING, GetController(BOOKMARKS)->state());

  // Once the association finishes, the DTM should still be waiting for the
  // Sync configurer to call back.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the download should complete the configuration.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that sync configures properly if all high priority types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllHighPriorityTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  dtm_->set_downloaded_types(ModelTypeSet(PREFERENCES));
  Configure(ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Association of Bookmarks can't happen until higher priority types are
  // finished, but Preferences should start associating immediately.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // When Prefs finish associating, configuration should still be waiting for
  // the high priority download to finish.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks aren't a ready type, they'll need to wait until the
  // low priority download also finishes.
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the Bookmarks association ends the configuration.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that sync configures properly if all types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(AddControlTypesTo(PREFERENCES));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  dtm_->set_downloaded_types(ModelTypeSet(PREFERENCES));
  Configure(ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Association of Bookmarks can't happen until higher priority types are
  // finished, but Preferences should start associating immediately.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // When Prefs finish associating, configuration should still be waiting for
  // the high priority download to finish.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks are a ready type, it can start associating immediately
  // after the high priority types finish downloading.
  dtm_->set_downloaded_types(ModelTypeSet(BOOKMARKS));
  FinishDownload(ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING, GetController(BOOKMARKS)->state());

  // Finishing the Bookmarks association leaves the DTM waiting for the low
  // priority download to finish.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the low priority download ends the configuration.
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that DataTypeManagerImpl delays configuration until all datatypes for
// which ShouldLoadModelBeforeConfigure() returns true loaded their models.
TEST_F(SyncDataTypeManagerImplTest, DelayConfigureForUSSTypes) {
  AddController(BOOKMARKS, true, true);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Bookmarks model isn't loaded yet and it is required to complete before
  // call to configure. Ensure that configure wasn't called.
  EXPECT_EQ(0, configurer_.configure_call_count());
  EXPECT_EQ(0, GetController(BOOKMARKS)->register_with_backend_call_count());

  // Finishing model load should trigger configure.
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();
  EXPECT_EQ(1, configurer_.configure_call_count());
  EXPECT_EQ(1, GetController(BOOKMARKS)->register_with_backend_call_count());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
}

// Test that when encryption fails for a given type, the corresponding
// controller is not told to register with its backend.
TEST_F(SyncDataTypeManagerImplTest, RegisterWithBackendOnEncryptionError) {
  AddController(BOOKMARKS, true, true);
  AddController(PASSWORDS, true, true);
  SetConfigureStartExpectation();

  FailEncryptionFor(ModelTypeSet(BOOKMARKS));
  Configure(ModelTypeSet(BOOKMARKS, PASSWORDS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(PASSWORDS)->state());
  EXPECT_EQ(0, GetController(BOOKMARKS)->register_with_backend_call_count());
  EXPECT_EQ(0, GetController(PASSWORDS)->register_with_backend_call_count());

  GetController(PASSWORDS)->SimulateModelLoadFinishing();
  EXPECT_EQ(0, GetController(BOOKMARKS)->register_with_backend_call_count());
  EXPECT_EQ(1, GetController(PASSWORDS)->register_with_backend_call_count());
}

// Test that RegisterWithBackend is not called for datatypes that failed
// LoadModels().
TEST_F(SyncDataTypeManagerImplTest, RegisterWithBackendAfterLoadModelsError) {
  // Initiate configuration for two datatypes but block them at LoadModels.
  AddController(BOOKMARKS, true, true);
  AddController(PASSWORDS, true, true);
  SetConfigureStartExpectation();
  Configure(ModelTypeSet(BOOKMARKS, PASSWORDS));
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(PASSWORDS)->state());

  // Make bookmarks fail LoadModels. Passwords load normally.
  GetController(BOOKMARKS)->SetModelLoadError(
      SyncError(FROM_HERE, SyncError::DATATYPE_ERROR, "load error", BOOKMARKS));
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();
  GetController(PASSWORDS)->SimulateModelLoadFinishing();

  // RegisterWithBackend should be called for passwords, but not bookmarks.
  EXPECT_EQ(0, GetController(BOOKMARKS)->register_with_backend_call_count());
  EXPECT_EQ(1, GetController(PASSWORDS)->register_with_backend_call_count());
}

// Test that Stop with DISABLE_SYNC calls DTC Stop with CLEAR_METADATA for
// active data types.
TEST_F(SyncDataTypeManagerImplTest, StopWithDisableSync) {
  // Initiate configuration for a datatype but block it at LoadModels.
  AddController(BOOKMARKS, true, true);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  dtm_->Stop(DISABLE_SYNC);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
  EXPECT_EQ(1, GetController(BOOKMARKS)->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PurgeDataOnStartingPersistent) {
  AddController(BOOKMARKS);
  AddController(AUTOFILL_WALLET_DATA);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.activated_types().Size());

  // The user temporarily turns off Sync.
  dtm_->Stop(STOP_SYNC);
  ASSERT_EQ(DataTypeManager::STOPPED, dtm_->state());
  ASSERT_TRUE(configurer_.activated_types().Empty());
  ASSERT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());

  // Now we restart with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.activated_types().Size());

  // This should have purged the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Has(BOOKMARKS));
  // Stop(CLEAR_METADATA) has *not* been called on the controller though; that
  // happens only when stopping or reconfiguring, not when (re)starting without
  // the type.
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, DontPurgeDataOnStartingEphemeral) {
  AddController(BOOKMARKS);
  AddController(AUTOFILL_WALLET_DATA);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.activated_types().Size());

  // The user temporarily turns off Sync.
  dtm_->Stop(STOP_SYNC);
  ASSERT_EQ(DataTypeManager::STOPPED, dtm_->state());
  ASSERT_TRUE(configurer_.activated_types().Empty());
  ASSERT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());

  // Now we restart in ephemeral mode, with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kTransportOnly);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.activated_types().Size());

  // This should *not* have purged the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, PurgeDataOnReconfiguringPersistent) {
  AddController(BOOKMARKS);
  AddController(AUTOFILL_WALLET_DATA);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.activated_types().Size());

  // Now we reconfigure with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.activated_types().Size());

  // This should have purged the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Has(BOOKMARKS));
  // Also Stop(CLEAR_METADATA) has been called on the controller since the type
  // is no longer enabled.
  EXPECT_EQ(1, GetController(BOOKMARKS)->clear_metadata_call_count());
}

TEST_F(SyncDataTypeManagerImplTest, DontPurgeDataOnReconfiguringEphemeral) {
  AddController(BOOKMARKS);
  AddController(AUTOFILL_WALLET_DATA);

  // Configure as usual.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS, AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.activated_types().Size());

  // Now we reconfigure into ephemeral mode, with a reduced set of data types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(ModelTypeSet(AUTOFILL_WALLET_DATA), SyncMode::kTransportOnly);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(AUTOFILL_WALLET_DATA), ModelTypeSet());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Since the storage option has changed, the controller has to re-associate
  // even though we didn't actually stop. So we have to call FinishStart again.
  GetController(AUTOFILL_WALLET_DATA)->FinishStart(DataTypeController::OK);
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.activated_types().Size());

  // This should *not* have cleared the data for the excluded type.
  EXPECT_TRUE(last_configure_params().to_purge.Empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->clear_metadata_call_count());
}
TEST_F(SyncDataTypeManagerImplTest, ShouldRecordInitialConfigureTimeHistogram) {
  base::HistogramTester histogram_tester;
  AddController(BOOKMARKS);

  // Configure as first sync.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(ModelTypeSet(BOOKMARKS), SyncMode::kFull,
            CONFIGURE_REASON_NEW_CLIENT);

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
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

  FinishDownload(ModelTypeSet(), ModelTypeSet());
  FinishDownload(ModelTypeSet(BOOKMARKS), ModelTypeSet());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  histogram_tester.ExpectTotalCount("Sync.ConfigureTime_Subsequent.OK", 1);
}

}  // namespace syncer
