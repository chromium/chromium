// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_generic_change_processor.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/fake_syncable_service.h"
#include "components/sync/model/sync_error.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillWebDataService;

namespace browser_sync {

namespace {

// Fake WebDataService implementation that stubs out the database loading.
class FakeWebDataService : public AutofillWebDataService {
 public:
  FakeWebDataService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner)
      : AutofillWebDataService(ui_task_runner, db_task_runner),
        is_database_loaded_(false),
        db_loaded_callback_(base::Callback<void(void)>()) {}

  // Mark the database as loaded and send out the appropriate notification.
  void LoadDatabase() {
    is_database_loaded_ = true;

    if (!db_loaded_callback_.is_null()) {
      db_loaded_callback_.Run();
      // Clear the callback here or the WDS and DTC will have refs to each other
      // and create a memory leak.
      db_loaded_callback_ = base::Callback<void(void)>();
    }
  }

  bool IsDatabaseLoaded() override { return is_database_loaded_; }

  void RegisterDBLoadedCallback(
      const base::Callback<void(void)>& callback) override {
    db_loaded_callback_ = callback;
  }

 private:
  ~FakeWebDataService() override {}

  bool is_database_loaded_;
  base::Callback<void(void)> db_loaded_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebDataService);
};

class AutofillWalletDataTypeControllerTest : public testing::Test,
                                             public syncer::FakeSyncClient {
 public:
  AutofillWalletDataTypeControllerTest()
      : syncer::FakeSyncClient(&profile_sync_factory_),
        last_type_(syncer::UNSPECIFIED) {}
  ~AutofillWalletDataTypeControllerTest() override {}

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportEnabled, true);
    prefs_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillCreditCardEnabled, true);

    web_data_service_ = base::MakeRefCounted<FakeWebDataService>(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    autofill_wallet_dtc_ = std::make_unique<AutofillWalletDataTypeController>(
        syncer::AUTOFILL_WALLET_DATA, base::ThreadTaskRunnerHandle::Get(),
        base::DoNothing(), this, web_data_service_);

    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  void TearDown() override {
    // Make sure WebDataService is shutdown properly on DB thread before we
    // destroy it.
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncer::AUTOFILL_WALLET_DATA);
  }

  // FakeSyncClient overrides.
  PrefService* GetPrefService() override { return &prefs_; }

  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override {
    return syncable_service_.AsWeakPtr();
  }

 protected:
  void SetStartExpectations() {
    autofill_wallet_dtc_->SetGenericChangeProcessorFactoryForTest(
        std::make_unique<syncer::FakeGenericChangeProcessorFactory>(
            std::make_unique<syncer::FakeGenericChangeProcessor>(
                syncer::AUTOFILL_WALLET_DATA, this)));
  }

  void Start() {
    autofill_wallet_dtc_->LoadModels(
        syncer::ConfigureContext(),
        base::Bind(&AutofillWalletDataTypeControllerTest::OnLoadFinished,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    if (autofill_wallet_dtc_->state() !=
        syncer::DataTypeController::MODEL_LOADED)
      return;
    autofill_wallet_dtc_->StartAssociating(base::Bind(
        &syncer::StartCallbackMock::Run, base::Unretained(&start_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  void OnLoadFinished(syncer::ModelType type, const syncer::SyncError& error) {
    last_type_ = type;
    last_error_ = error;
  }

  base::MessageLoop message_loop_;
  TestingPrefServiceSimple prefs_;
  syncer::StartCallbackMock start_callback_;
  syncer::SyncApiComponentFactoryMock profile_sync_factory_;
  syncer::FakeSyncableService syncable_service_;
  std::unique_ptr<AutofillWalletDataTypeController> autofill_wallet_dtc_;
  scoped_refptr<FakeWebDataService> web_data_service_;

  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
};

TEST_F(AutofillWalletDataTypeControllerTest, StartDatatypeEnabled) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_DATA, last_type_);
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByWalletImportWhileRunning) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_DATA, last_type_);
  autofill::prefs::SetPaymentsIntegrationEnabled(GetPrefService(), false);
  autofill::prefs::SetCreditCardAutofillEnabled(GetPrefService(), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_error_.IsSet());
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByCreditCardsWhileRunning) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_DATA, last_type_);
  autofill::prefs::SetPaymentsIntegrationEnabled(GetPrefService(), true);
  autofill::prefs::SetCreditCardAutofillEnabled(GetPrefService(), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_error_.IsSet());
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByWalletImportAtStartup) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  autofill::prefs::SetPaymentsIntegrationEnabled(GetPrefService(), false);
  autofill::prefs::SetCreditCardAutofillEnabled(GetPrefService(), true);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_error_.IsSet());
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByCreditCardsAtStartup) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  autofill::prefs::SetPaymentsIntegrationEnabled(GetPrefService(), true);
  autofill::prefs::SetCreditCardAutofillEnabled(GetPrefService(), false);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_error_.IsSet());
}

}  // namespace

}  // namespace browser_sync
