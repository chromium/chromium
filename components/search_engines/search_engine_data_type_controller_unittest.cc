// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_generic_change_processor.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/model/fake_syncable_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgPointee;

namespace browser_sync {
namespace {

class SyncSearchEngineDataTypeControllerTest : public testing::Test,
                                               public syncer::FakeSyncClient {
 public:
  SyncSearchEngineDataTypeControllerTest()
      : syncer::FakeSyncClient(&profile_sync_factory_),
        template_url_service_(nullptr, 0),
        search_engine_dtc_(base::Closure(), this, &template_url_service_) {
    // Disallow the TemplateURLService from loading until
    // PreloadTemplateURLService() is called .
    template_url_service_.set_disable_load(true);
  }

  // FakeSyncClient overrides.
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override {
    return syncable_service_.AsWeakPtr();
  }

  void TearDown() override {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncer::SEARCH_ENGINES);
  }

 protected:
  void PreloadTemplateURLService() {
    template_url_service_.set_disable_load(false);
    template_url_service_.Load();
  }

  void SetStartExpectations() {
    search_engine_dtc_.SetGenericChangeProcessorFactoryForTest(
        base::WrapUnique<syncer::GenericChangeProcessorFactory>(
            new syncer::FakeGenericChangeProcessorFactory(
                std::make_unique<syncer::FakeGenericChangeProcessor>(
                    syncer::SEARCH_ENGINES, this))));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void Start() {
    search_engine_dtc_.LoadModels(
        syncer::ConfigureContext(),
        base::Bind(&syncer::ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    search_engine_dtc_.StartAssociating(base::Bind(
        &syncer::StartCallbackMock::Run, base::Unretained(&start_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  TemplateURLService template_url_service_;
  SearchEngineDataTypeController search_engine_dtc_;
  syncer::SyncApiComponentFactoryMock profile_sync_factory_;
  syncer::FakeSyncableService syncable_service_;
  syncer::StartCallbackMock start_callback_;
  syncer::ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceReady) {
  SetStartExpectations();
  // We want to start ready.
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(syncer::DataTypeController::OK, _, _));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, search_engine_dtc_.state());
  EXPECT_TRUE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceNotReady) {
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_.LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&syncer::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_.GetSubscriptionForTesting());
  EXPECT_EQ(syncer::DataTypeController::MODEL_STARTING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());

  // Send the notification that the TemplateURLService has started.
  PreloadTemplateURLService();
  EXPECT_EQ(nullptr, search_engine_dtc_.GetSubscriptionForTesting());
  EXPECT_EQ(syncer::DataTypeController::MODEL_LOADED,
            search_engine_dtc_.state());

  // Wait until WebDB is loaded before we shut it down.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::ASSOCIATION_FAILED, _, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      syncer::SyncError(FROM_HERE,
                        syncer::SyncError::DATATYPE_ERROR,
                        "Error",
                        syncer::SEARCH_ENGINES));

  Start();
  EXPECT_EQ(syncer::DataTypeController::FAILED, search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_.Stop(syncer::STOP_SYNC);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, Stop) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(syncer::DataTypeController::OK, _, _));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, search_engine_dtc_.state());
  EXPECT_TRUE(syncable_service_.syncing());
  search_engine_dtc_.Stop(syncer::STOP_SYNC);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            search_engine_dtc_.state());
  // AsyncDirectoryTypeController::Stop posts call to StopLocalService to model
  // thread. We run message loop for this call to take effect.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StopBeforeLoaded) {
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_.LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&syncer::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_.GetSubscriptionForTesting());
  EXPECT_EQ(syncer::DataTypeController::MODEL_STARTING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_.Stop(syncer::STOP_SYNC);
  EXPECT_EQ(nullptr, search_engine_dtc_.GetSubscriptionForTesting());
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            search_engine_dtc_.state());
  EXPECT_FALSE(syncable_service_.syncing());
}

}  // namespace
}  // namespace browser_sync
