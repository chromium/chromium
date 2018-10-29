// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/abstract_profile_sync_service_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/browser_sync/test_http_bridge_factory.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/sync/driver/glue/sync_backend_host_core.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/engine/sync_manager_factory_for_profile_sync_test.h"
#include "components/sync/engine/test_engine_components_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/test_user_share.h"
#include "services/network/test/test_network_connection_tracker.h"

using syncer::SyncBackendHostImpl;
using syncer::ModelType;
using testing::_;
using testing::ByMove;
using testing::Return;

namespace browser_sync {

namespace {

std::unique_ptr<syncer::HttpPostProviderFactory> GetHttpPostProviderFactory(
    syncer::CancelationSignal* signal) {
  return std::make_unique<TestHttpBridgeFactory>();
}

class SyncEngineForProfileSyncTest : public SyncBackendHostImpl {
 public:
  SyncEngineForProfileSyncTest(
      const base::FilePath& temp_dir,
      syncer::SyncClient* sync_client,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
      base::OnceClosure callback);
  ~SyncEngineForProfileSyncTest() override;

  void Initialize(InitParams params) override;

  void ConfigureDataTypes(ConfigureParams params) override;

 private:
  // Invoked at the start of HandleSyncManagerInitializationOnFrontendLoop.
  // Allows extra initialization work to be performed before the engine comes
  // up.
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineForProfileSyncTest);
};

SyncEngineForProfileSyncTest::SyncEngineForProfileSyncTest(
    const base::FilePath& temp_dir,
    syncer::SyncClient* sync_client,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
    base::OnceClosure callback)
    : SyncBackendHostImpl(
          "dummy_debug_name",
          sync_client,
          invalidator,
          sync_prefs,
          temp_dir.Append(base::FilePath(FILE_PATH_LITERAL("test")))),
      callback_(std::move(callback)) {}

SyncEngineForProfileSyncTest::~SyncEngineForProfileSyncTest() {}

void SyncEngineForProfileSyncTest::Initialize(InitParams params) {
  params.http_factory_getter = base::Bind(&GetHttpPostProviderFactory);
  params.sync_manager_factory =
      std::make_unique<syncer::SyncManagerFactoryForProfileSyncTest>(
          std::move(callback_),
          network::TestNetworkConnectionTracker::GetInstance());
  params.credentials.email = "testuser@gmail.com";
  params.credentials.sync_token = "token";
  params.restored_key_for_bootstrapping.clear();

  // It'd be nice if we avoided creating the EngineComponentsFactory in the
  // first place, but SyncEngine will have created one by now so we must free
  // it. Grab the switches to pass on first.
  syncer::EngineComponentsFactory::Switches factory_switches =
      params.engine_components_factory->GetSwitches();
  params.engine_components_factory =
      std::make_unique<syncer::TestEngineComponentsFactory>(
          factory_switches, syncer::EngineComponentsFactory::STORAGE_IN_MEMORY,
          nullptr);

  SyncBackendHostImpl::Initialize(std::move(params));
}

void SyncEngineForProfileSyncTest::ConfigureDataTypes(ConfigureParams params) {
  // The first parameter there should be the set of enabled types.  That's not
  // something we have access to from this strange test harness.  We'll just
  // send back the list of newly configured types instead and hope it doesn't
  // break anything.
  // Posted to avoid re-entrancy issues.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncEngineForProfileSyncTest::FinishConfigureDataTypesOnFrontendLoop,
          base::Unretained(this), params.to_download, params.to_download,
          syncer::ModelTypeSet(), params.ready_task));
}

// Helper function for return-type-upcasting of the callback.
syncer::SyncService* GetSyncService(
    base::Callback<TestProfileSyncService*(void)> get_sync_service_callback) {
  return get_sync_service_callback.Run();
}

}  // namespace

/* static */
syncer::ImmutableChangeRecordList
ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
    int64_t node_id,
    syncer::ChangeRecord::Action action) {
  syncer::ChangeRecord record;
  record.action = action;
  record.id = node_id;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

/* static */
syncer::ImmutableChangeRecordList
ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
    int64_t node_id,
    const sync_pb::EntitySpecifics& specifics) {
  syncer::ChangeRecord record;
  record.action = syncer::ChangeRecord::ACTION_DELETE;
  record.id = node_id;
  record.specifics = specifics;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

AbstractProfileSyncServiceTest::AbstractProfileSyncServiceTest()
    : data_type_thread_("Extra thread") {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

AbstractProfileSyncServiceTest::~AbstractProfileSyncServiceTest() {
  sync_service_->Shutdown();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return syncer::TestUserShare::CreateRoot(model_type,
                                           sync_service_->GetUserShare());
}

void AbstractProfileSyncServiceTest::CreateSyncService(
    std::unique_ptr<syncer::SyncClient> sync_client,
    base::OnceClosure initialization_success_callback) {
  ASSERT_TRUE(sync_client);
  ProfileSyncService::InitParams init_params =
      profile_sync_service_bundle_.CreateBasicInitParams(
          ProfileSyncService::AUTO_START, std::move(sync_client));
  sync_service_ =
      std::make_unique<TestProfileSyncService>(std::move(init_params));

  syncer::SyncApiComponentFactoryMock* components =
      profile_sync_service_bundle_.component_factory();
  auto engine = std::make_unique<SyncEngineForProfileSyncTest>(
      temp_dir_.GetPath(), sync_service_->GetSyncClientForTest(),
      profile_sync_service_bundle_.fake_invalidation_service(),
      sync_service_->sync_prefs()->AsWeakPtr(),
      std::move(initialization_success_callback));
  EXPECT_CALL(*components, CreateSyncEngine(_, _, _, _))
      .WillOnce(Return(ByMove(std::move(engine))));

  sync_service_->SetFirstSetupComplete();
}

base::Callback<syncer::SyncService*(void)>
AbstractProfileSyncServiceTest::GetSyncServiceCallback() {
  return base::Bind(GetSyncService,
                    base::Bind(&AbstractProfileSyncServiceTest::sync_service,
                               base::Unretained(this)));
}

CreateRootHelper::CreateRootHelper(AbstractProfileSyncServiceTest* test,
                                   ModelType model_type)
    : callback_(base::Bind(&CreateRootHelper::CreateRootCallback,
                           base::Unretained(this))),
      test_(test),
      model_type_(model_type),
      success_(false) {}

CreateRootHelper::~CreateRootHelper() {}

const base::Closure& CreateRootHelper::callback() const {
  return callback_;
}

bool CreateRootHelper::success() {
  return success_;
}

void CreateRootHelper::CreateRootCallback() {
  success_ = test_->CreateRoot(model_type_);
}

}  // namespace browser_sync
