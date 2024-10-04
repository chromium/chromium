// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_manager_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_encryption_handler.h"
#include "components/sync/service/data_type_manager_observer.h"
#include "components/sync/service/data_type_status_table.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/mock_data_type_local_data_batch_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::Pair;
using testing::UnorderedElementsAre;

// Helpers for unioning with control types.
DataTypeSet AddControlTypesTo(DataTypeSet types) {
  return Union(ControlTypes(), types);
}

ConfigureContext BuildConfigureContext(ConfigureReason reason,
                                       SyncMode sync_mode = SyncMode::kFull) {
  ConfigureContext context;
  context.reason = reason;
  context.sync_mode = sync_mode;
  context.cache_guid = "test_cache_guid";
  return context;
}

class MockDataTypeManagerObserver : public DataTypeManagerObserver {
 public:
  MockDataTypeManagerObserver() = default;
  ~MockDataTypeManagerObserver() override = default;

  MOCK_METHOD(void,
              OnConfigureDone,
              (const DataTypeManager::ConfigureResult&),
              (override));
  MOCK_METHOD(void, OnConfigureStart, (), (override));
};

MATCHER(ConfigureSucceeded, "") {
  if (arg.status != DataTypeManager::OK) {
    *result_listener << "Status not OK: "
                     << DataTypeManager::ConfigureStatusToString(arg.status);
    return false;
  }
  return true;
}

MATCHER(ConfigureAborted, "") {
  return arg.status == DataTypeManager::ABORTED;
}

// Fake DataTypeConfigurer implementation that allows the test body to control
// when downloads complete and whether failures occurred.
class FakeDataTypeConfigurer : public DataTypeConfigurer {
 public:
  FakeDataTypeConfigurer() = default;
  ~FakeDataTypeConfigurer() override = default;

  void ConfigureDataTypes(ConfigureParams params) override {
    ASSERT_TRUE(last_params_.ready_task.is_null());
    ASSERT_FALSE(params.ready_task.is_null());
    configure_call_count_++;
    last_params_ = std::move(params);
  }

  void ConnectDataType(DataType type,
                       std::unique_ptr<DataTypeActivationResponse>
                           activation_response) override {
    connected_types_.Put(type);
  }

  void DisconnectDataType(DataType type) override {
    connected_types_.Remove(type);
  }

  void ClearNigoriDataForMigration() override { clear_nigori_data_count_++; }

  void RecordNigoriMemoryUsageAndCountsHistograms() override {
    // Not implemented but also not needed for these tests.
  }

  void GetNigoriNodeForDebugging(AllNodesCallback callback) override {
    // Not implemented but also not needed for these tests.
  }

  // Completes any ongoing download request and returns the set of types that
  // was successfully configured, which is all requested except
  // `failed_download_types`.
  DataTypeSet FinishDownloadWithFailedTypes(DataTypeSet failed_download_types) {
    if (!last_params_.ready_task) {
      return DataTypeSet();
    }
    const DataTypeSet to_download = last_params_.to_download;
    // In the odd event that a non-connected type was configured, it cannot
    // successfully configure. This mimics the real SyncEngine's behavior.
    const DataTypeSet succeeded_types =
        Intersection(Difference(to_download, failed_download_types),
                     AddControlTypesTo(connected_types_));
    const DataTypeSet failed_types = Difference(to_download, succeeded_types);
    EXPECT_TRUE(to_download.HasAll(failed_download_types));
    std::move(last_params_.ready_task).Run(succeeded_types, failed_types);
    return succeeded_types;
  }

  DataTypeSet connected_types() const { return connected_types_; }
  int configure_call_count() const { return configure_call_count_; }
  const ConfigureParams& last_params() const { return last_params_; }
  bool has_ongoing_configuration() const {
    return !last_params_.ready_task.is_null();
  }
  int clear_nigori_data_count() const { return clear_nigori_data_count_; }

 private:
  DataTypeSet connected_types_;
  int configure_call_count_ = 0;
  ConfigureParams last_params_;
  int clear_nigori_data_count_ = 0;
};

class FakeDataTypeEncryptionHandler : public DataTypeEncryptionHandler {
 public:
  FakeDataTypeEncryptionHandler() = default;
  ~FakeDataTypeEncryptionHandler() override = default;

  bool HasCryptoError() const override;
  DataTypeSet GetAllEncryptedDataTypes() const override;

  void set_crypto_error(bool crypto_error) { crypto_error_ = crypto_error; }
  void set_encrypted_types(DataTypeSet encrypted_types) {
    encrypted_types_ = encrypted_types;
  }

 private:
  bool crypto_error_ = false;
  DataTypeSet encrypted_types_;
};

bool FakeDataTypeEncryptionHandler::HasCryptoError() const {
  return crypto_error_;
}

DataTypeSet FakeDataTypeEncryptionHandler::GetAllEncryptedDataTypes() const {
  return encrypted_types_;
}

class DataTypeManagerImplTest : public testing::Test {
 protected:
  DataTypeManagerImplTest() = default;
  ~DataTypeManagerImplTest() override = default;

  void InitDataTypeManager(DataTypeSet types_without_transport_mode_support,
                           DataTypeSet types_with_transport_mode_support = {},
                           DataTypeSet types_with_batch_uploader = {}) {
    CHECK(Intersection(types_without_transport_mode_support,
                       types_with_transport_mode_support)
              .empty());
    CHECK(types_with_transport_mode_support.HasAll(types_with_batch_uploader));

    DataTypeController::TypeVector controllers;
    for (DataType type : types_without_transport_mode_support) {
      controllers.push_back(std::make_unique<FakeDataTypeController>(
          type, /*enable_transport_mode=*/false));
    }
    for (DataType type : types_with_transport_mode_support) {
      auto batch_uploader =
          types_with_batch_uploader.Has(type)
              ? std::make_unique<MockDataTypeLocalDataBatchUploader>()
              : nullptr;
      controllers.push_back(std::make_unique<FakeDataTypeController>(
          type, /*enable_transport_mode=*/true, std::move(batch_uploader)));
    }
    InitDataTypeManagerWithControllers(std::move(controllers));
  }

  void InitDataTypeManagerWithControllers(
      DataTypeController::TypeVector controllers) {
    dtm_ = std::make_unique<DataTypeManagerImpl>(
        std::move(controllers), &encryption_handler_, &observer_);
    dtm_->SetConfigurer(&configurer_);
  }

  // Configure the given DTM with the given desired types.
  void Configure(DataTypeSet desired_types,
                 ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION) {
    dtm_->Configure(desired_types, BuildConfigureContext(reason));
  }

  void Configure(DataTypeSet desired_types,
                 SyncMode sync_mode,
                 ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION) {
    dtm_->Configure(desired_types, BuildConfigureContext(reason, sync_mode));
  }

  // Completes any ongoing download request and returns the set of types that
  // was successfully configured, which is all requested except
  // `failed_download_types`.
  DataTypeSet FinishDownloadWithFailedTypes(DataTypeSet failed_download_types) {
    return configurer_.FinishDownloadWithFailedTypes(failed_download_types);
  }

  // Completes any ongoing download request and returns the set of types that
  // was successfully configured.
  DataTypeSet FinishDownload() {
    return FinishDownloadWithFailedTypes(DataTypeSet());
  }

  // Completes ongoing download requests until idle, i.e. until there is no
  // download ongoing.
  void FinishAllDownloadsUntilIdle() {
    while (configurer_.has_ongoing_configuration()) {
      FinishDownload();
    }
  }

  // Gets the fake controller for the given type, which should have
  // been previously added via InitDataTypeManager().
  FakeDataTypeController* GetController(DataType data_type) const {
    CHECK(dtm_);
    return static_cast<FakeDataTypeController*>(
        dtm_->GetControllerForTest(data_type));
  }

  // Returns the number of times NIGORI was cleared (aka purged).
  int clear_nigori_data_count() const {
    return configurer_.clear_nigori_data_count();
  }

  // Gets the batch uploader for the given type, which should have
  // been previously added via InitDataTypeManager(). Returns null if the
  // datatype was initialized without a batch uploader.
  MockDataTypeLocalDataBatchUploader* GetBatchUploader(
      DataType data_type) const {
    CHECK(dtm_);
    return static_cast<MockDataTypeLocalDataBatchUploader*>(
        GetController(data_type)->GetLocalDataBatchUploader());
  }

  void FailEncryptionFor(DataTypeSet encrypted_types) {
    encryption_handler_.set_crypto_error(true);
    encryption_handler_.set_encrypted_types(encrypted_types);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  FakeDataTypeConfigurer configurer_;
  testing::NiceMock<MockDataTypeManagerObserver> observer_;
  std::unique_ptr<DataTypeManagerImpl> dtm_;
  FakeDataTypeEncryptionHandler encryption_handler_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_F(DataTypeManagerImplTest, NoControllers) {
  InitDataTypeManager({});

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(_)).Times(0);

  Configure(DataTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnConfigureStart()).Times(0);
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet(), dtm_->GetRegisteredDataTypes());
  EXPECT_EQ(0, clear_nigori_data_count());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_EQ(DataTypeSet(), dtm_->GetRegisteredDataTypes());
  EXPECT_EQ(0, clear_nigori_data_count());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, finish starting the controller, and then stop the DTM.
TEST_F(DataTypeManagerImplTest, ConfigureOne) {
  InitDataTypeManager({BOOKMARKS});

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(_)).Times(0);

  EXPECT_EQ(DataTypeSet{BOOKMARKS}, dtm_->GetRegisteredDataTypes());
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));

  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.

  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnConfigureStart()).Times(0);
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().empty());
  EXPECT_EQ(DataTypeSet{BOOKMARKS}, dtm_->GetRegisteredDataTypes());
  EXPECT_EQ(0, clear_nigori_data_count());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  EXPECT_EQ(DataTypeSet{BOOKMARKS}, dtm_->GetRegisteredDataTypes());
  EXPECT_EQ(0, clear_nigori_data_count());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneThatSkipsEngineConnection) {
  InitDataTypeManager({BOOKMARKS});

  GetController(BOOKMARKS)
      ->model()
      ->EnableSkipEngineConnectionForActivationResponse();

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(_)).Times(0);

  Configure({BOOKMARKS});

  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  ASSERT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnConfigureStart()).Times(0);
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // BOOKMARKS shouldn't download as the engine connection is skipped.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_TRUE(dtm_->GetActiveProxyDataTypes().Has(BOOKMARKS));

  // Even if all APIs above indicate the datatype is active, in reality the
  // configurer (SyncEngine) hasn't been activated/connected.
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with a single controller, configure it, but stop it
// before finishing the download.  It should still be safe to run the
// download callback even after the DTM is stopped and destroyed.
TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileDownloadPending) {
  InitDataTypeManager({BOOKMARKS});

  {
    testing::InSequence seq;
    EXPECT_CALL(observer_, OnConfigureStart());
    EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));

    Configure({BOOKMARKS});
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());
    EXPECT_TRUE(
        dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));

    dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.connected_types().empty());
    EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().empty());
  }

  FinishDownload();
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped.
TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  {
    testing::InSequence seq;
    EXPECT_CALL(observer_, OnConfigureStart());
    EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));

    Configure({BOOKMARKS});
    ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    ASSERT_EQ(DataTypeController::MODEL_STARTING,
              GetController(BOOKMARKS)->state());

    dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    EXPECT_TRUE(configurer_.connected_types().empty());
  }

  EXPECT_EQ(DataTypeController::STOPPING, GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with a single controller.  Then:
//
//   1) Configure.
//   2) Finish the download for step 1.
//   3) The download determines a crypto error.
//   4) Complete download for the reconfiguration without the controller.
//   5) Stop the DTM.
TEST_F(DataTypeManagerImplTest, OneWaitingForCrypto) {
  InitDataTypeManager({PASSWORDS});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1.
  Configure({PASSWORDS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(AddControlTypesTo({PASSWORDS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 3.
  FailEncryptionFor({PASSWORDS});

  // Step 4.
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({PASSWORDS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(PASSWORDS));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(PASSWORDS)->state());

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
TEST_F(DataTypeManagerImplTest, ConfigureOneThenBoth) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}),
            dtm_->GetTypesWithPendingDownloadForInitialSync());

  // Step 2.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeSet({BOOKMARKS}),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet(), dtm_->GetTypesWithPendingDownloadForInitialSync());

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 3.
  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES}),
            configurer_.connected_types());
  EXPECT_EQ(DataTypeSet({PREFERENCES}),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());

  // Step 4.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeSet({PREFERENCES}),
            dtm_->GetTypesWithPendingDownloadForInitialSync());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet(), dtm_->GetTypesWithPendingDownloadForInitialSync());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Configure with second controller.
//   4) Finish the download for step 3.
//   5) Stop the DTM.
TEST_F(DataTypeManagerImplTest, ConfigureOneThenSwitch) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());

  // Step 2.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 3.
  Configure({PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({PREFERENCES}), configurer_.connected_types());
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());

  // Step 4.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

TEST_F(DataTypeManagerImplTest, ConfigureModelLoading) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1: Configure with first controller (model stays loading).
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());

  // Step 2: Configure with both controllers, which gets postponed because
  //         there's an ongoing configuration that cannot complete before the
  //         model loads.
  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  ASSERT_FALSE(dtm_->needs_reconfigure_for_test());
  Configure({BOOKMARKS, PREFERENCES});
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
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS, PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES}),
            configurer_.connected_types());

  // Step 5: Stop the DTM.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with one controller. Then configure and start the controller
// with a datatype error. DTM should proceed without the affected datatype.
TEST_F(DataTypeManagerImplTest, OneFailingController) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  EXPECT_CALL(observer_, OnConfigureStart());

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());

  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ASSERT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  // This should be CONFIGURED but is not properly handled in
  // DataTypeManagerImpl::OnAllDataTypesReadyForConfigure().
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1.
//   4) Finish the download for step 2.
//   5) Stop the DTM.
TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());

  // Step 2.
  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Regular types.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS, PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES}),
            configurer_.connected_types());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
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
TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPendingWithFailure) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Step 1.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS}), configurer_.connected_types());

  // Step 2.
  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES}),
            configurer_.connected_types());

  // Step 5.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Tests a Purge then Configure.  This is similar to the sequence of
// operations that would be invoked by the BackendMigrator.
TEST_F(DataTypeManagerImplTest, MigrateAll) {
  InitDataTypeManager({PRIORITY_PREFERENCES});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Initial setup.
  Configure({PRIORITY_PREFERENCES});
  ASSERT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  ASSERT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());

  // We've now configured priority prefs and (implicitly) the control types.
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Pretend we were told to migrate all types.
  DataTypeSet to_migrate;
  to_migrate.Put(PRIORITY_PREFERENCES);
  to_migrate.PutAll(ControlTypes());

  ASSERT_EQ(0, clear_nigori_data_count());
  ASSERT_EQ(
      0, GetController(PRIORITY_PREFERENCES)->model()->clear_metadata_count());

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  dtm_->PurgeForMigration(to_migrate);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(1, clear_nigori_data_count());
  EXPECT_EQ(
      1, GetController(PRIORITY_PREFERENCES)->model()->clear_metadata_count());

  // The DTM will call ConfigureDataTypes(), even though it is unnecessary.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // no enabled types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Re-enable the migrated types.
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure(to_migrate);
  EXPECT_EQ(ControlTypes(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1, clear_nigori_data_count());
  EXPECT_EQ(
      1, GetController(PRIORITY_PREFERENCES)->model()->clear_metadata_count());
}

// Test receipt of a Configure request while a purge is in flight.
TEST_F(DataTypeManagerImplTest, ConfigureDuringPurge) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  // Initial configure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({BOOKMARKS});
  ASSERT_EQ(DataTypeSet(), FinishDownload());  // Control types.
  ASSERT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1, GetController(PREFERENCES)->model()->clear_metadata_count());
  ASSERT_EQ(0, clear_nigori_data_count());

  // Purge the Nigori type.
  EXPECT_CALL(observer_, OnConfigureStart());
  dtm_->PurgeForMigration(ControlTypes());
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  ASSERT_EQ(1, clear_nigori_data_count());

  // Called during the first call to Configure() and during PurgeForMigration().
  ASSERT_EQ(2, GetController(PREFERENCES)->model()->clear_metadata_count());

  // Before the backend configuration completes, ask for a different
  // set of types.  This request asks for
  // - BOOKMARKS: which is redundant because it was already enabled,
  // - PREFERENCES: which is new and will need to be downloaded, and
  // - NIGORI: (added implicitly because it is a control type) which
  //   the DTM is part-way through purging.
  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Invoke the callback we've been waiting for since we asked to purge NIGORI.
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // regular types

  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Now invoke the callback for the second configure request.
  EXPECT_EQ(ControlTypes(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  // No clears during/after the last Configure().
  EXPECT_EQ(2, GetController(PREFERENCES)->model()->clear_metadata_count());
}

TEST_F(DataTypeManagerImplTest, PrioritizedConfiguration) {
  // The order of priorities is:
  // 1. Control types, i.e. NIGORI - included implicitly.
  // 2. Priority types: includes PRIORITY_PREFERENCES.
  // 3. Regular typesL includes BOOKMARKS.
  // 4. Low-priority types: includes HISTORY.
  InitDataTypeManager({PRIORITY_PREFERENCES, BOOKMARKS, HISTORY});

  // Initial configure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Start the configuration.
  ASSERT_EQ(0, configurer_.configure_call_count());
  Configure({BOOKMARKS, HISTORY, PRIORITY_PREFERENCES});
  // This causes an immediate ConfigureDataTypes() call for control types, i.e.
  // Nigori. It's important that this does *not* ask for any types to be
  // downloaded, see crbug.com/1170318 and crbug.com/1187914.
  ASSERT_NE(0, configurer_.configure_call_count());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Finishing the no-op download of the control types causes the next
  // ConfigureDataTypes() call, for priority types.
  EXPECT_EQ(DataTypeSet(), FinishDownload());

  // BOOKMARKS is downloaded after PRIORITY_PREFERENCES finishes.
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // HISTORY is downloaded after BOOKMARKS finishes.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo({HISTORY}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(DataTypeManagerImplTest, GetDataTypesForTransportOnlyMode) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{BOOKMARKS, PREFERENCES},
      /*types_with_transport_mode_support=*/{PRIORITY_PREFERENCES});

  ASSERT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES, PRIORITY_PREFERENCES}),
            dtm_->GetRegisteredDataTypes());
  // Note that NIGORI is listed, although there is no controller registered in
  // DataTypeManager. This is because it is a control type and it is expected to
  // run in transport mode.
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}),
            dtm_->GetDataTypesForTransportOnlyMode());
}

TEST_F(DataTypeManagerImplTest, ShouldPrioritizePasswordsOverInvitations) {
  // Passwords must be configured and downloaded before incoming password
  // sharing invitations.
  InitDataTypeManager(/*types_without_transport_mode_support=*/{PASSWORDS},
                      /*types_with_transport_mode_support=*/{
                          INCOMING_PASSWORD_SHARING_INVITATION});

  // Initial configure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Start the configuration.
  ASSERT_EQ(0, configurer_.configure_call_count());
  Configure({PASSWORDS, INCOMING_PASSWORD_SHARING_INVITATION});

  // Finishing the no-op download of the control types causes the next
  // ConfigureDataTypes() call.
  EXPECT_EQ(DataTypeSet(), FinishDownload());

  // INCOMING_PASSWORD_SHARING_INVITATION is downloaded after PASSWORDS.
  EXPECT_EQ(AddControlTypesTo({PASSWORDS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(AddControlTypesTo({INCOMING_PASSWORD_SHARING_INVITATION}),
            FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(DataTypeManagerImplTest, PrioritizedConfigurationReconfigure) {
  InitDataTypeManager({PRIORITY_PREFERENCES, BOOKMARKS, APPS});

  // Initial configure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Start a configuration with BOOKMARKS and PRIORITY_PREFERENCES, and finish
  // the download of PRIORITY_PREFERENCES.
  Configure({BOOKMARKS, PRIORITY_PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}),
            configurer_.last_params().to_download);

  // Enable syncing for APPS while the download of BOOKMARKS is still pending.
  Configure({BOOKMARKS, PRIORITY_PREFERENCES, APPS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfiguration starts after downloading of previous types finishes.
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // Priority types: Nothing to download, since PRIORITY_PREFERENCES was
  // downloaded before.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Regular types: Only the newly-enabled APPS still needs to be downloaded.
  EXPECT_EQ(AddControlTypesTo({APPS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(DataTypeManagerImplTest, PrioritizedConfigurationStop) {
  InitDataTypeManager({PRIORITY_PREFERENCES, BOOKMARKS});

  // Initial configure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));

  // Initially only PRIORITY_PREFERENCES is configured.
  Configure({BOOKMARKS, PRIORITY_PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet(), FinishDownload());

  // BOOKMARKS is configured after download of PRIORITY_PREFERENCES finishes.
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}),
            configurer_.last_params().to_download);

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

TEST_F(DataTypeManagerImplTest, PrioritizedConfigurationDownloadError) {
  InitDataTypeManager({PRIORITY_PREFERENCES, BOOKMARKS});

  // Initial configure. Bookmarks will fail to associate due to the download
  // failure.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Initially only PRIORITY_PREFERENCES is configured.
  Configure({BOOKMARKS, PRIORITY_PREFERENCES});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeSet(), FinishDownload());

  // BOOKMARKS is configured after download of PRIORITY_PREFERENCES finishes.
  EXPECT_EQ(AddControlTypesTo({PRIORITY_PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}),
            configurer_.last_params().to_download);

  // PRIORITY_PREFERENCES controller is running while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Make BOOKMARKS download fail. PRIORITY_PREFERENCES is still running.
  EXPECT_EQ(ControlTypes(), FinishDownloadWithFailedTypes({BOOKMARKS}));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());

  // Finish pending downloads, which will trigger a reconfiguration to disable
  // bookmarks.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeSet(), configurer_.last_params().to_download);
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(DataTypeManagerImplTest, FilterDesiredTypes) {
  InitDataTypeManager({BOOKMARKS});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, APPS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

TEST_F(DataTypeManagerImplTest, FailingPreconditionKeepData) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(0U, configurer_.connected_types().size());

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());

  // Should do nothing.
  dtm_->DataTypePreconditionChanged(BOOKMARKS);

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());

  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(DataTypeManagerImplTest, FailingPreconditionClearData) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(0U, configurer_.connected_types().size());

  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

// Tests that unready types are not started after ResetDataTypeErrors and
// reconfiguration.
TEST_F(DataTypeManagerImplTest, UnreadyTypeResetReconfigure) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  // Second Configure sets a flag to perform reconfiguration after the first one
  // is done.
  Configure({BOOKMARKS});

  // Reset errors before triggering reconfiguration.
  dtm_->ResetDataTypeErrors();

  // Reconfiguration should update unready errors. Bookmarks shouldn't start.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeSet(), FinishDownload());  // regular types
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(0U, configurer_.connected_types().size());
}

TEST_F(DataTypeManagerImplTest, UnreadyTypeLaterReady) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(0U, configurer_.connected_types().size());

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  // Set the expectations for the reconfiguration - no unready errors now.
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());
}

TEST_F(DataTypeManagerImplTest, MultipleUnreadyTypesLaterReadyAtTheSameTime) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);
  GetController(PREFERENCES)
      ->SetPreconditionState(
          DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Both types are never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  ASSERT_FALSE(dtm_->GetActiveDataTypes().Has(PREFERENCES));
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(0U, configurer_.connected_types().size());

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
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS, PREFERENCES}), FinishDownload());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.connected_types().size());
}

TEST_F(DataTypeManagerImplTest, MultipleUnreadyTypesLaterOneOfThemReady) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);
  GetController(PREFERENCES)
      ->SetPreconditionState(
          DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Both types are never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().size());

  // Bookmarks should start normally now. Preferences should still not start.
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_NE(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());

  // Set the expectations for the reconfiguration - just prefs are unready now.
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());
}

TEST_F(DataTypeManagerImplTest,
       NoOpDataTypePreconditionChangedWhileStillUnready) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(0U, configurer_.connected_types().size());

  // Bookmarks is still unready so DataTypePreconditionChanged() should be
  // ignored.
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(DataTypeManagerImplTest,
       NoOpDataTypePreconditionChangedWhileStillReady) {
  InitDataTypeManager({BOOKMARKS});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Bookmarks is still ready so DataTypePreconditionChanged() should be
  // ignored.
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(DataTypeManagerImplTest, ModelLoadError) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));

  // Bookmarks is never started due to hitting a model load error.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // No need to finish the download of BOOKMARKS since it was never started.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.connected_types().size());
}

// Checks that DTM handles the case when a controller is already in a FAILED
// state at the time the DTM is created. Regression test for crbug.com/967344.
TEST_F(DataTypeManagerImplTest, ErrorBeforeStartup) {
  DataTypeController::TypeVector controllers;
  for (DataType type : {BOOKMARKS, PREFERENCES}) {
    controllers.push_back(std::make_unique<FakeDataTypeController>(
        type, /*enable_transport_mode=*/false));
  }

  // Produce an error (FAILED) state in the BOOKMARKS controller.
  static_cast<FakeDataTypeController*>(controllers[0].get())
      ->SimulateControllerError(FROM_HERE);

  InitDataTypeManagerWithControllers(std::move(controllers));
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  // Now a configuration attempt for both types should complete successfully,
  // but exclude the failed type.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  Configure({BOOKMARKS, PREFERENCES});

  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  EXPECT_TRUE(dtm_->GetActiveDataTypes().Has(PREFERENCES));
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_FALSE(
      dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));
}

// Test that sync configures properly if all types are already downloaded.
TEST_F(DataTypeManagerImplTest, AllTypesReady) {
  InitDataTypeManager({PRIORITY_PREFERENCES, BOOKMARKS});

  // Mark both types as already downloaded.
  sync_pb::DataTypeState already_downloaded;
  already_downloaded.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  GetController(PRIORITY_PREFERENCES)
      ->model()
      ->SetDataTypeStateForActivationResponse(already_downloaded);
  GetController(BOOKMARKS)->model()->SetDataTypeStateForActivationResponse(
      already_downloaded);

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({PRIORITY_PREFERENCES, BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Both types were downloaded already, so they aren't downloading initial
  // data even during the CONFIGURING state.
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().empty());

  // This started the configuration of control types, which aren't tracked by
  // DataTypeManagerImpl, so always considered already downloaded.
  ASSERT_EQ(1, configurer_.configure_call_count());
  EXPECT_TRUE(configurer_.last_params().to_download.empty());

  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PRIORITY_PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Finish downloading (configuring, really) control types.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // This started the configuration of priority types, i.e.
  // PRIORITY_PREFERENCES, which is already downloaded.
  ASSERT_EQ(2, configurer_.configure_call_count());
  EXPECT_TRUE(configurer_.last_params().to_download.empty());

  // Finish downloading (configuring, really) priority types.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // This started the configuration of regular types, i.e. BOOKMARKS, which is
  // already downloaded.
  ASSERT_EQ(3, configurer_.configure_call_count());
  EXPECT_TRUE(configurer_.last_params().to_download.empty());

  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finish downloading (configuring, really) regular types. This finishes the
  // configuration.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(3, configurer_.configure_call_count());  // Not increased.

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.connected_types().size());
  EXPECT_TRUE(dtm_->GetActiveProxyDataTypes().empty());
  EXPECT_TRUE(dtm_->GetTypesWithPendingDownloadForInitialSync().empty());

  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
}

// Test that DataTypeManagerImpl delays configuration until all data types
// loaded their models.
TEST_F(DataTypeManagerImplTest, DelayConfigureForUSSTypes) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Bookmarks model isn't loaded yet and it is required to complete before
  // call to configure. Ensure that configure wasn't called.
  EXPECT_EQ(0, configurer_.configure_call_count());
  EXPECT_EQ(0, GetController(BOOKMARKS)->activate_call_count());

  // Finishing model load should trigger configure.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(1, configurer_.configure_call_count());
  EXPECT_EQ(1, GetController(BOOKMARKS)->activate_call_count());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());
}

// Test that when encryption fails for a given type, the corresponding
// data type is not activated.
TEST_F(DataTypeManagerImplTest, ConnectDataTypeOnEncryptionError) {
  InitDataTypeManager({BOOKMARKS, PASSWORDS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(PASSWORDS)->model()->EnableManualModelStart();
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());

  FailEncryptionFor({BOOKMARKS});
  Configure({BOOKMARKS, PASSWORDS});
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
TEST_F(DataTypeManagerImplTest, ConnectDataTypeAfterLoadModelsError) {
  // Initiate configuration for two datatypes but block them at LoadModels.
  InitDataTypeManager({BOOKMARKS, PASSWORDS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(PASSWORDS)->model()->EnableManualModelStart();
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  Configure({BOOKMARKS, PASSWORDS});
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
TEST_F(DataTypeManagerImplTest, StopWithDisableSync) {
  InitDataTypeManager({BOOKMARKS});
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));

  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  dtm_->Stop(SyncStopMetadataFate::CLEAR_METADATA);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.connected_types().empty());
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
  // Clearing of Nigori in this case happens outside of DataTypeManager, namely
  // via SyncEngine::Shutdown().
  EXPECT_EQ(0, clear_nigori_data_count());
}

TEST_F(DataTypeManagerImplTest, PurgeDataOnStarting) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{BOOKMARKS},
      /*types_with_transport_mode_support=*/{AUTOFILL_WALLET_DATA});

  // Configure as usual.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, AUTOFILL_WALLET_DATA}, SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  ASSERT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(AddControlTypesTo({BOOKMARKS, AUTOFILL_WALLET_DATA}),
            FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.connected_types().size());

  // The user temporarily turns off Sync.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(DataTypeManager::STOPPED, dtm_->state());
  ASSERT_TRUE(configurer_.connected_types().empty());
  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());

  // Now we restart with a reduced set of data types.
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({AUTOFILL_WALLET_DATA}, SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({AUTOFILL_WALLET_DATA}), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.connected_types().size());

  // Stop(CLEAR_METADATA) is called if (re)started without the type.
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(DataTypeManagerImplTest, PurgeDataOnReconfiguring) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{BOOKMARKS},
      /*types_with_transport_mode_support=*/{AUTOFILL_WALLET_DATA});

  // Configure as usual.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, AUTOFILL_WALLET_DATA}, SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  ASSERT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(AddControlTypesTo({BOOKMARKS, AUTOFILL_WALLET_DATA}),
            FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(2U, configurer_.connected_types().size());

  // Now we reconfigure with a reduced set of data types.
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({AUTOFILL_WALLET_DATA}, SyncMode::kFull);
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_EQ(1U, configurer_.connected_types().size());
  ASSERT_EQ(0, clear_nigori_data_count());

  // Also Stop(CLEAR_METADATA) has been called on the controller since the type
  // is no longer enabled.
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(DataTypeManagerImplTest, ShouldRecordInitialConfigureTimeHistogram) {
  base::HistogramTester histogram_tester;
  InitDataTypeManager({BOOKMARKS});

  // Configure as first sync.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS}, SyncMode::kFull, CONFIGURE_REASON_NEW_CLIENT);

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  histogram_tester.ExpectTotalCount("Sync.ConfigureTime_Initial.OK", 1);
}

TEST_F(DataTypeManagerImplTest, ShouldRecordSubsequentConfigureTimeHistogram) {
  base::HistogramTester histogram_tester;
  InitDataTypeManager({BOOKMARKS});
  // Configure as subsequent sync.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS}, SyncMode::kFull, CONFIGURE_REASON_RECONFIGURATION);

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  histogram_tester.ExpectTotalCount("Sync.ConfigureTime_Subsequent.OK", 1);
}

// Regression test for crbug.com/1286204: Reentrant calls to Configure()
// shouldn't crash (or trigger DCHECKs).
TEST_F(DataTypeManagerImplTest, ReentrantConfigure) {
  InitDataTypeManager({PREFERENCES, BOOKMARKS});

  // The DataTypeManagerObserver::OnConfigureStart() call may, in some cases,
  // result in a reentrant call to Configure().
  EXPECT_CALL(observer_, OnConfigureStart()).WillOnce([&]() {
    Configure({PREFERENCES});
  });

  Configure({PREFERENCES, BOOKMARKS});
  // Implicit expectation: No crash here!

  // Eventually, the second (reentrant) Configure() call should win, i.e. here
  // only PREFERENCES gets configured.
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.connected_types().size());
}

TEST_F(DataTypeManagerImplTest, ProvideDebugInfo) {
  InitDataTypeManager({PREFERENCES, BOOKMARKS});

  // Mark BOOKMARKS as already downloaded.
  sync_pb::DataTypeState bookmarks_state;
  bookmarks_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  GetController(BOOKMARKS)->model()->SetDataTypeStateForActivationResponse(
      bookmarks_state);

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({PREFERENCES, BOOKMARKS});
  ASSERT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Because Bookmarks are already downloaded, configuration finishes as soon
  // as preferences are downloaded.
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(DataTypeManagerImplTest, ShouldDoNothingForAlreadyStoppedTypes) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);

  // Bookmarks is never started due to failing preconditions.
  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS});
  ASSERT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // No need to finish the download of BOOKMARKS since it was never started.
  ASSERT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_FALSE(dtm_->needs_reconfigure_for_test());
}

TEST_F(DataTypeManagerImplTest, ShouldDoNothingForAlreadyFailedTypes) {
  InitDataTypeManager({BOOKMARKS});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_TRUE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));

  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));
  ASSERT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());

  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // Data type error should cause re-configuration.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));

  // Another error should not trigger re-configuration. This is verified by
  // `observer_` which checks for OnConfigurationDone() and should fails when
  // it's called unexpectedly.
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "test error"));
  task_environment_.RunUntilIdle();
}

// Tests that data types which time out are ultimately skipped during
// configuration.
TEST_F(DataTypeManagerImplTest, ShouldFinishConfigureIfSomeTypesTimeout) {
  // Create two controllers, but one with a delayed model load.
  InitDataTypeManager({BOOKMARKS, PREFERENCES});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  Configure({BOOKMARKS, PREFERENCES});

  // BOOKMARKS blocks configuration.
  EXPECT_TRUE(configurer_.connected_types().empty());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  // Fast-forward to time out.
  task_environment_.FastForwardBy(kSyncLoadModelsTimeoutDuration);

  // BOOKMARKS is ignored and PREFERENCES is connected.
  EXPECT_EQ(configurer_.connected_types(), DataTypeSet({PREFERENCES}));
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({PREFERENCES}),
            FinishDownloadWithFailedTypes({BOOKMARKS}));
  // BOOKMARKS is skipped and signalled to stop.
  EXPECT_EQ(DataTypeController::STOPPING, GetController(BOOKMARKS)->state());
  // DataTypeManager will be notified for reconfiguration.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // DataTypeManager finishes configuration.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

// Tests that if the load-models timeout triggers after a Stop(), this doesn't
// have any adverse effects.
// Regression test for crbug.com/333865298.
TEST_F(DataTypeManagerImplTest, TimeoutAfterStop) {
  // Create a controller with a delayed model load.
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  Configure({BOOKMARKS});

  // BOOKMARKS blocks configuration.
  EXPECT_TRUE(configurer_.connected_types().empty());
  EXPECT_EQ(DataTypeController::MODEL_STARTING,
            GetController(BOOKMARKS)->state());

  // Before configuration finishes (or times out), the DataTypeManager gets
  // stopped again, and the configurer gets destroyed.
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  dtm_->SetConfigurer(nullptr);

  // Fast-forward to trigger the load-models timeout. This shouldn't do
  // anything (in particular, not crash).
  task_environment_.FastForwardBy(kSyncLoadModelsTimeoutDuration);
}

TEST_F(DataTypeManagerImplTest, ShouldUpdateDataTypeStatusWhileStopped) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);

  EXPECT_FALSE(dtm_->needs_reconfigure_for_test());
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(dtm_->GetDataTypesWithPermanentErrors().Has(BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, ShouldReconfigureOnPreconditionChanged) {
  InitDataTypeManager({BOOKMARKS});

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  EXPECT_EQ(AddControlTypesTo({BOOKMARKS}), FinishDownload());

  ASSERT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  ASSERT_FALSE(dtm_->needs_reconfigure_for_test());

  GetController(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);
  dtm_->DataTypePreconditionChanged(BOOKMARKS);
  EXPECT_TRUE(dtm_->needs_reconfigure_for_test());
}

// Tests that failure of data type in STOPPING state is handled during
// configuration.
// Regression test for crbug.com/1477324.
TEST_F(DataTypeManagerImplTest, ShouldHandleStoppingTypesFailure) {
  InitDataTypeManager({BOOKMARKS});
  GetController(BOOKMARKS)->model()->EnableManualModelStart();

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureAborted()));
  Configure({BOOKMARKS});
  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  // Bring BOOKMARKS to a STOPPING state.
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::STOPPING);
  ASSERT_EQ(DataTypeManager::STOPPED, dtm_->state());

  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // BOOKMARKS should not be started since it is in a FAILED state.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // No need to finish the download of BOOKMARKS since it was never started.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_FALSE(
      dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));
}

// Tests that failure of data type in NOT_RUNNING state is handled during
// configuration.
// Regression test for crbug.com/1477324.
TEST_F(DataTypeManagerImplTest, ShouldHandleStoppedTypesFailure) {
  InitDataTypeManager({BOOKMARKS});

  // Start and stop BOOKMARKS. This is needed to allow FakeDataTypeController
  // to get into FAILED state from NOT_RUNNING.
  Configure({BOOKMARKS});
  ASSERT_EQ(DataTypeSet(), FinishDownload());
  ASSERT_EQ(AddControlTypesTo({BOOKMARKS}),
            FinishDownload());  // priority types
  dtm_->Stop(SyncStopMetadataFate::KEEP_METADATA);
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::FAILED);

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnConfigureStart());
  EXPECT_CALL(observer_, OnConfigureDone(ConfigureSucceeded()));

  // BOOKMARKS should not be started since it is in a FAILED state.
  Configure({BOOKMARKS});
  EXPECT_EQ(DataTypeSet(), FinishDownload());
  // No need to finish the download of BOOKMARKS since it was never started.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_FALSE(dtm_->GetActiveDataTypes().Has(BOOKMARKS));
  EXPECT_FALSE(
      dtm_->GetTypesWithPendingDownloadForInitialSync().Has(BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, ClearMetadataWhileStoppedExceptFor) {
  InitDataTypeManager({BOOKMARKS, PREFERENCES});

  ASSERT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  ASSERT_EQ(0, GetController(PREFERENCES)->model()->clear_metadata_count());

  dtm_->ClearMetadataWhileStoppedExceptFor({BOOKMARKS});

  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_count());
  EXPECT_EQ(1, GetController(PREFERENCES)->model()->clear_metadata_count());
}

TEST_F(DataTypeManagerImplTest, ShouldGetLocalDataDescriptionsForOneType) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{},
      /*types_with_transport_mode_support=*/{BOOKMARKS, READING_LIST},
      /*types_with_batch_uploader=*/{BOOKMARKS, READING_LIST});
  Configure({BOOKMARKS, READING_LIST});
  FinishAllDownloadsUntilIdle();
  ASSERT_EQ(dtm_->GetActiveDataTypes(),
            AddControlTypesTo({BOOKMARKS, READING_LIST}));

  base::OnceCallback<void(LocalDataDescription)> bookmarks_upload_callback;

  // Only the controller for bookmarks should be exercised.
  EXPECT_CALL(*GetBatchUploader(READING_LIST), GetLocalDataDescription)
      .Times(0);
  EXPECT_CALL(*GetBatchUploader(BOOKMARKS), GetLocalDataDescription)
      .WillOnce([&](base::OnceCallback<void(LocalDataDescription)> callback) {
        bookmarks_upload_callback = std::move(callback);
      });

  base::MockCallback<
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>>
      mock_completion_callback;

  dtm_->GetLocalDataDescriptions({BOOKMARKS}, mock_completion_callback.Get());

  ASSERT_TRUE(bookmarks_upload_callback);

  // When bookmarks complete, the caller should also be notified about
  // completion.
  LocalDataDescription bookmarks_description;
  bookmarks_description.item_count = 42;
  EXPECT_CALL(mock_completion_callback,
              Run(ElementsAre(Pair(BOOKMARKS, bookmarks_description))));
  std::move(bookmarks_upload_callback).Run(bookmarks_description);
}

TEST_F(DataTypeManagerImplTest,
       ShouldGetLocalDataDescriptionsForMultipleTypes) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{},
      /*types_with_transport_mode_support=*/{BOOKMARKS, READING_LIST},
      /*types_with_batch_uploader=*/{BOOKMARKS, READING_LIST});
  Configure({BOOKMARKS, READING_LIST});
  FinishAllDownloadsUntilIdle();
  ASSERT_EQ(dtm_->GetActiveDataTypes(),
            AddControlTypesTo({BOOKMARKS, READING_LIST}));

  base::OnceCallback<void(LocalDataDescription)> bookmarks_upload_callback;
  base::OnceCallback<void(LocalDataDescription)> reading_list_upload_callback;

  // Both controllers should be exercised.
  EXPECT_CALL(*GetBatchUploader(BOOKMARKS), GetLocalDataDescription)
      .WillOnce([&](base::OnceCallback<void(LocalDataDescription)> callback) {
        bookmarks_upload_callback = std::move(callback);
      });
  EXPECT_CALL(*GetBatchUploader(READING_LIST), GetLocalDataDescription)
      .WillOnce([&](base::OnceCallback<void(LocalDataDescription)> callback) {
        reading_list_upload_callback = std::move(callback);
      });

  base::MockCallback<
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>>
      mock_completion_callback;
  EXPECT_CALL(mock_completion_callback, Run).Times(0);

  dtm_->GetLocalDataDescriptions({BOOKMARKS, READING_LIST},
                                 mock_completion_callback.Get());

  ASSERT_TRUE(bookmarks_upload_callback);
  ASSERT_TRUE(reading_list_upload_callback);

  // When bookmarks complete, nothing happens because reading list is still
  // ongoing.
  LocalDataDescription bookmarks_description;
  bookmarks_description.item_count = 42;
  std::move(bookmarks_upload_callback).Run(bookmarks_description);

  // When both types complete, the caller should also be notified about
  // completion.
  LocalDataDescription reading_list_description;
  reading_list_description.item_count = 31;
  EXPECT_CALL(
      mock_completion_callback,
      Run(UnorderedElementsAre(Pair(BOOKMARKS, bookmarks_description),
                               Pair(READING_LIST, reading_list_description))));
  std::move(reading_list_upload_callback).Run(reading_list_description);
}

TEST_F(DataTypeManagerImplTest,
       ShouldOnlyGetLocalDataDescriptionsFromActiveTypes) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{},
      /*types_with_transport_mode_support=*/{BOOKMARKS, READING_LIST},
      /*types_with_batch_uploader=*/{BOOKMARKS, READING_LIST});
  Configure({BOOKMARKS});
  FinishAllDownloadsUntilIdle();
  ASSERT_EQ(dtm_->GetActiveDataTypes(), AddControlTypesTo({BOOKMARKS}));

  base::OnceCallback<void(LocalDataDescription)> bookmarks_upload_callback;

  // Only the controller for bookmarks should be exercised, because reading list
  // is not active.
  EXPECT_CALL(*GetBatchUploader(READING_LIST), GetLocalDataDescription)
      .Times(0);
  EXPECT_CALL(*GetBatchUploader(BOOKMARKS), GetLocalDataDescription)
      .WillOnce([&](base::OnceCallback<void(LocalDataDescription)> callback) {
        bookmarks_upload_callback = std::move(callback);
      });

  base::MockCallback<
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>>
      mock_completion_callback;
  EXPECT_CALL(mock_completion_callback, Run).Times(0);

  dtm_->GetLocalDataDescriptions({BOOKMARKS, READING_LIST},
                                 mock_completion_callback.Get());

  // When bookmarks complete, the caller should also be notified about
  // completion.
  EXPECT_CALL(mock_completion_callback, Run(ElementsAre(Pair(BOOKMARKS, _))));
  std::move(bookmarks_upload_callback).Run(LocalDataDescription());
}

TEST_F(DataTypeManagerImplTest,
       ShouldReturnEmptyGetLocalDataDescriptionsIfNoActiveTypes) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{},
      /*types_with_transport_mode_support=*/{BOOKMARKS, READING_LIST},
      /*types_with_batch_uploader=*/{BOOKMARKS, READING_LIST});
  Configure({});
  FinishAllDownloadsUntilIdle();
  ASSERT_EQ(dtm_->GetActiveDataTypes(), ControlTypes());

  // The types aren't active so the batch uploaders should not be exercised.
  EXPECT_CALL(*GetBatchUploader(READING_LIST), GetLocalDataDescription)
      .Times(0);
  EXPECT_CALL(*GetBatchUploader(BOOKMARKS), GetLocalDataDescription).Times(0);

  base::MockCallback<
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>>
      mock_completion_callback;
  EXPECT_CALL(mock_completion_callback, Run);
  dtm_->GetLocalDataDescriptions({BOOKMARKS, READING_LIST},
                                 mock_completion_callback.Get());
}

TEST_F(DataTypeManagerImplTest,
       ShouldOnlyMigrateActiveTypesUponTriggerLocalDataMigration) {
  InitDataTypeManager(
      /*types_without_transport_mode_support=*/{},
      /*types_with_transport_mode_support=*/{BOOKMARKS, READING_LIST},
      /*types_with_batch_uploader=*/{BOOKMARKS, READING_LIST});
  Configure({BOOKMARKS});
  FinishAllDownloadsUntilIdle();
  ASSERT_EQ(dtm_->GetActiveDataTypes(), AddControlTypesTo({BOOKMARKS}));

  // Only the controller for bookmarks should be exercised, because reading list
  // is not active.
  EXPECT_CALL(*GetBatchUploader(READING_LIST), TriggerLocalDataMigration)
      .Times(0);
  EXPECT_CALL(*GetBatchUploader(BOOKMARKS), TriggerLocalDataMigration);

  dtm_->TriggerLocalDataMigration({BOOKMARKS, READING_LIST});
}

}  // namespace

}  // namespace syncer
