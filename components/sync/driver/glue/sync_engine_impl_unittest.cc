// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_impl.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/test_unrecoverable_error_handler.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/fake_sync_manager.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sync_engine_host_stub.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/test/callback_counter.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google/cacheinvalidation/include/types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {

namespace {

static const base::FilePath::CharType kTestSyncDir[] =
    FILE_PATH_LITERAL("sync-test");

scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(ModelSafeGroup group) {
  switch (group) {
    case GROUP_PASSIVE:
      return new PassiveModelWorker();
    default:
      return nullptr;
  }
}

class TestSyncEngineHost : public SyncEngineHostStub {
 public:
  explicit TestSyncEngineHost(
      base::Callback<void(ModelTypeSet)> set_engine_types)
      : set_engine_types_(set_engine_types) {}

  void OnEngineInitialized(ModelTypeSet initial_types,
                           const WeakHandle<JsBackend>&,
                           const WeakHandle<DataTypeDebugInfoListener>&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           bool success) override {
    EXPECT_EQ(expect_success_, success);
    set_engine_types_.Run(initial_types);
    std::move(quit_closure_).Run();
  }

  void SetExpectSuccess(bool expect_success) {
    expect_success_ = expect_success;
  }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  base::Callback<void(ModelTypeSet)> set_engine_types_;
  bool expect_success_ = false;
  base::OnceClosure quit_closure_;
};

class FakeSyncManagerFactory : public SyncManagerFactory {
 public:
  explicit FakeSyncManagerFactory(
      FakeSyncManager** fake_manager,
      network::NetworkConnectionTracker* network_connection_tracker)
      : SyncManagerFactory(network_connection_tracker),
        should_fail_on_init_(false),
        fake_manager_(fake_manager) {
    *fake_manager_ = nullptr;
  }
  ~FakeSyncManagerFactory() override {}

  // SyncManagerFactory implementation.  Called on the sync thread.
  std::unique_ptr<SyncManager> CreateSyncManager(
      const std::string& /* name */) override {
    *fake_manager_ =
        new FakeSyncManager(initial_sync_ended_types_, progress_marker_types_,
                            configure_fail_types_, should_fail_on_init_);
    return std::unique_ptr<SyncManager>(*fake_manager_);
  }

  void set_initial_sync_ended_types(ModelTypeSet types) {
    initial_sync_ended_types_ = types;
  }

  void set_progress_marker_types(ModelTypeSet types) {
    progress_marker_types_ = types;
  }

  void set_configure_fail_types(ModelTypeSet types) {
    configure_fail_types_ = types;
  }

  void set_should_fail_on_init(bool should_fail_on_init) {
    should_fail_on_init_ = should_fail_on_init;
  }

 private:
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;
  ModelTypeSet configure_fail_types_;
  bool should_fail_on_init_;
  FakeSyncManager** fake_manager_;
};

class MockInvalidationService : public invalidation::InvalidationService {
 public:
  MockInvalidationService() = default;
  ~MockInvalidationService() override = default;

  MOCK_METHOD1(RegisterInvalidationHandler,
               void(syncer::InvalidationHandler* handler));
  MOCK_METHOD2(UpdateRegisteredInvalidationIds,
               bool(syncer::InvalidationHandler* handler,
                    const syncer::ObjectIdSet& ids));
  MOCK_METHOD1(UnregisterInvalidationHandler,
               void(syncer::InvalidationHandler* handler));
  MOCK_METHOD0(GetInvalidatorStat, syncer::InvalidatorState());
  MOCK_CONST_METHOD0(GetInvalidatorState, syncer::InvalidatorState());
  MOCK_CONST_METHOD0(GetInvalidatorClientId, std::string());
  MOCK_METHOD0(GetInvalidationLogger, invalidation::InvalidationLogger*());
  MOCK_CONST_METHOD1(RequestDetailedStatus,
                     void(base::RepeatingCallback<
                          void(const base::DictionaryValue&)> post_caller));
};

std::unique_ptr<HttpPostProviderFactory> CreateHttpBridgeFactory() {
  return std::make_unique<HttpBridgeFactory>(
      /*user_agent=*/"",
      /*url_loader_factory_info=*/nullptr,
      /*network_time_update_callback=*/base::DoNothing());
}

class SyncEngineImplTest : public testing::Test {
 protected:
  SyncEngineImplTest()
      : sync_thread_("SyncThreadForTest"),
        host_(base::Bind(&SyncEngineImplTest::SetEngineTypes,
                         base::Unretained(this))),
        fake_manager_(nullptr) {}

  ~SyncEngineImplTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
    sync_thread_.StartAndWaitForTesting();
    ON_CALL(invalidator_,
            UpdateRegisteredInvalidationIds(testing::_, testing::_))
        .WillByDefault(testing::Return(true));
    backend_ = std::make_unique<SyncEngineImpl>(
        "dummyDebugName", &invalidator_, sync_prefs_->AsWeakPtr(),
        temp_dir_.GetPath().Append(base::FilePath(kTestSyncDir)));

    fake_manager_factory_ = std::make_unique<FakeSyncManagerFactory>(
        &fake_manager_, network::TestNetworkConnectionTracker::GetInstance());

    // These types are always implicitly enabled.
    enabled_types_.PutAll(ControlTypes());

    // NOTE: We can't include Passwords or Typed URLs due to the Sync Backend
    // Registrar removing them if it can't find their model workers.
    enabled_types_.Put(BOOKMARKS);
    enabled_types_.Put(PREFERENCES);
    enabled_types_.Put(SESSIONS);
    enabled_types_.Put(SEARCH_ENGINES);
    enabled_types_.Put(AUTOFILL);
  }

  void TearDown() override {
    if (backend_) {
      backend_->StopSyncingForShutdown();
      backend_->Shutdown(STOP_SYNC);
    }
    backend_.reset();
    sync_prefs_.reset();
    // Pump messages posted by the sync thread.
    base::RunLoop().RunUntilIdle();
  }

  // Synchronously initializes the backend.
  void InitializeBackend(bool expect_success) {
    host_.SetExpectSuccess(expect_success);

    SyncEngine::InitParams params;
    params.sync_task_runner = sync_thread_.task_runner();
    params.host = &host_;
    params.registrar = std::make_unique<SyncBackendRegistrar>(
        std::string(), base::Bind(&CreateModelWorkerForGroup));
    params.http_factory_getter = base::BindOnce(&CreateHttpBridgeFactory);
    params.authenticated_account_id = CoreAccountId("account_id");
    params.sync_manager_factory = std::move(fake_manager_factory_);
    params.unrecoverable_error_handler =
        MakeWeakHandle(test_unrecoverable_error_handler_.GetWeakPtr()),
    sync_prefs_->GetInvalidationVersions(&params.invalidation_versions);

    backend_->Initialize(std::move(params));

    PumpSyncThread();
    // |fake_manager_factory_|'s fake_manager() is set on the sync
    // thread, but we can rely on the message loop barriers to
    // guarantee that we see the updated value.
    DCHECK(fake_manager_);
  }

  // Synchronously configures the backend's datatypes.
  ModelTypeSet ConfigureDataTypes() {
    return ConfigureDataTypesWithUnready(ModelTypeSet());
  }

  ModelTypeSet ConfigureDataTypesWithUnready(ModelTypeSet unready_types) {
    ModelTypeSet disabled_types =
        Difference(ModelTypeSet::All(), enabled_types_);

    ModelTypeConfigurer::ConfigureParams params;
    params.reason = CONFIGURE_REASON_RECONFIGURATION;
    params.enabled_types = Difference(enabled_types_, unready_types);
    params.disabled_types = Union(disabled_types, unready_types);
    params.to_download = Difference(params.enabled_types, engine_types_);
    if (!params.to_download.Empty()) {
      params.to_download.Put(NIGORI);
    }
    params.to_purge = Intersection(engine_types_, disabled_types);
    params.ready_task =
        base::Bind(&SyncEngineImplTest::DownloadReady, base::Unretained(this));

    ModelTypeSet ready_types =
        Difference(params.enabled_types, params.to_download);
    backend_->ConfigureDataTypes(std::move(params));
    PumpSyncThread();

    return ready_types;
  }

 protected:
  void DownloadReady(ModelTypeSet succeeded_types, ModelTypeSet failed_types) {
    engine_types_.PutAll(succeeded_types);
    std::move(quit_loop_).Run();
  }

  void SetEngineTypes(ModelTypeSet engine_types) {
    EXPECT_TRUE(engine_types_.Empty());
    engine_types_ = engine_types;
  }

  void PumpSyncThread() {
    base::RunLoop run_loop;
    quit_loop_ = run_loop.QuitClosure();
    host_.set_quit_closure(run_loop.QuitClosure());
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::Thread sync_thread_;
  TestSyncEngineHost host_;
  TestUnrecoverableErrorHandler test_unrecoverable_error_handler_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  std::unique_ptr<SyncEngineImpl> backend_;
  std::unique_ptr<FakeSyncManagerFactory> fake_manager_factory_;
  FakeSyncManager* fake_manager_;
  ModelTypeSet engine_types_;
  ModelTypeSet enabled_types_;
  base::OnceClosure quit_loop_;
  testing::NiceMock<MockInvalidationService> invalidator_;
};

// Test basic initialization with no initial types (first time initialization).
// Only the nigori should be configured.
TEST_F(SyncEngineImplTest, InitShutdown) {
  InitializeBackend(true);
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(ControlTypes())
          .Empty());
}

// Test first time sync scenario. All types should be properly configured.

TEST_F(SyncEngineImplTest, FirstTimeSync) {
  InitializeBackend(true);
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(ControlTypes())
          .Empty());

  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      Difference(enabled_types_, ControlTypes())));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the restart after setting up sync scenario. No enabled types should be
// downloaded or cleaned.
TEST_F(SyncEngineImplTest, Restart) {
  sync_prefs_->SetFirstSetupComplete();
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend(true);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  ModelTypeSet ready_types = ConfigureDataTypes();
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test a sync restart scenario where some types had never finished configuring.
// The partial types should be purged, then reconfigured properly.
TEST_F(SyncEngineImplTest, PartialTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers, but nigori and bookmarks are missing initial sync ended.
  ModelTypeSet partial_types(NIGORI, BOOKMARKS);
  ModelTypeSet full_types = Difference(enabled_types_, partial_types);
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(full_types);

  // Bringing up the backend should purge all partial types, then proceed to
  // download the Nigori.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(NIGORI), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(fake_manager_->GetAndResetPurgedTypes().HasAll(partial_types));
  EXPECT_EQ(Union(full_types, ModelTypeSet(NIGORI)),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Difference(partial_types, ModelTypeSet(NIGORI)),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // Now do the actual configuration, which should download and apply bookmarks.
  ModelTypeSet ready_types = ConfigureDataTypes();
  EXPECT_EQ(full_types, ready_types);
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(partial_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the behavior when we lose the sync db. Although we already have types
// enabled, we should re-download all of them because we lost their data.
TEST_F(SyncEngineImplTest, LostDB) {
  sync_prefs_->SetFirstSetupComplete();
  // Initialization should fetch the Nigori node.  Everything else should be
  // left untouched.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(ControlTypes()),
            fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ModelTypeSet(ControlTypes()),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Difference(enabled_types_, ControlTypes()),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // The database was empty, so any cleaning is entirely optional.  We want to
  // reset this value before running the next part of the test, though.
  fake_manager_->GetAndResetPurgedTypes();

  // The actual configuration should redownload and apply all the enabled types.
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      Difference(enabled_types_, ControlTypes())));
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

TEST_F(SyncEngineImplTest, DisableTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetPurgedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then disable two datatypes.
  ModelTypeSet disabled_types(BOOKMARKS, SEARCH_ENGINES);
  ModelTypeSet old_types = enabled_types_;
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes disabled should be cleaned. Nothing should be
  // downloaded.
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_EQ(disabled_types,
            Intersection(fake_manager_->GetAndResetPurgedTypes(), old_types));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

TEST_F(SyncEngineImplTest, AddTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetPurgedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then add two datatypes.
  ModelTypeSet new_types(EXTENSIONS, APPS);
  enabled_types_.PutAll(new_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(enabled_types_, new_types), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// And and disable in the same configuration.
TEST_F(SyncEngineImplTest, AddDisableTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetPurgedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then add two datatypes.
  ModelTypeSet old_types = enabled_types_;
  ModelTypeSet disabled_types(BOOKMARKS, SEARCH_ENGINES);
  ModelTypeSet new_types(EXTENSIONS, APPS);
  enabled_types_.PutAll(new_types);
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(enabled_types_, new_types), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(disabled_types,
            Intersection(fake_manager_->GetAndResetPurgedTypes(), old_types));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(disabled_types,
            fake_manager_->GetTypesWithEmptyProgressMarkerToken(old_types));
}

// Test restarting the browser to newly supported datatypes. The new datatypes
// should be downloaded on the configuration after backend initialization.
TEST_F(SyncEngineImplTest, NewlySupportedTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  ModelTypeSet old_types = enabled_types_;
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);
  ModelTypeSet new_types(APP_SETTINGS, EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Does nothing.
  InitializeBackend(true);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), old_types).Empty());
  EXPECT_EQ(old_types, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(new_types, fake_manager_->GetTypesWithEmptyProgressMarkerToken(
                           enabled_types_));

  // Downloads and applies the new types (plus nigori).
  ModelTypeSet ready_types = ConfigureDataTypes();

  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(old_types, ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the newly supported types scenario, but with the presence of partial
// types as well. Both partial and newly supported types should be downloaded
// the configuration.
TEST_F(SyncEngineImplTest, NewlySupportedTypesWithPartialTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  ModelTypeSet old_types = enabled_types_;
  ModelTypeSet partial_types(NIGORI, BOOKMARKS);
  ModelTypeSet full_types = Difference(enabled_types_, partial_types);
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(full_types);
  ModelTypeSet new_types(APP_SETTINGS, EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Purge the partial types.  The nigori will be among the purged types, but
  // the syncer will re-download it by the time the initialization is complete.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(NIGORI), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(fake_manager_->GetAndResetPurgedTypes().HasAll(partial_types));
  EXPECT_EQ(Union(full_types, ModelTypeSet(NIGORI)),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Union(new_types, Difference(partial_types, ModelTypeSet(NIGORI))),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // Downloads and applies the new types and partial types (which includes
  // nigori anyways).
  ModelTypeSet ready_types = ConfigureDataTypes();
  EXPECT_EQ(full_types, ready_types);
  EXPECT_EQ(Union(new_types, partial_types),
            fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetPurgedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Verify that downloading control types only downloads those types that do
// not have initial sync ended set.
TEST_F(SyncEngineImplTest, DownloadControlTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. Experiments and device
  // info are new types without progress markers or initial sync ended, while
  // all other types have been fully downloaded and applied.
  ModelTypeSet new_types(NIGORI);
  ModelTypeSet old_types = Difference(enabled_types_, new_types);
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);

  // Bringing up the backend should download the new types without downloading
  // any old types.
  InitializeBackend(true);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(Difference(ModelTypeSet::All(), enabled_types_),
            fake_manager_->GetAndResetPurgedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Fail to download control types.  It's believed that there is a server bug
// which can allow this to happen (crbug.com/164288).  The sync engine should
// detect this condition and fail to initialize the backend.
//
// The failure is "silent" in the sense that the GetUpdates request appears to
// be successful, but it returned no results.  This means that the usual
// download retry logic will not be invoked.
TEST_F(SyncEngineImplTest, SilentlyFailToDownloadControlTypes) {
  fake_manager_factory_->set_configure_fail_types(ModelTypeSet::All());
  InitializeBackend(false);
}

// Test that local refresh requests are delivered to sync.
TEST_F(SyncEngineImplTest, ForwardLocalRefreshRequest) {
  InitializeBackend(true);

  ModelTypeSet set1 = ModelTypeSet::All();
  backend_->TriggerRefresh(set1);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(set1, fake_manager_->GetLastRefreshRequestTypes());

  ModelTypeSet set2 = ModelTypeSet(SESSIONS);
  backend_->TriggerRefresh(set2);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(set2, fake_manager_->GetLastRefreshRequestTypes());
}

// Test that configuration on signin sends the proper GU source.
TEST_F(SyncEngineImplTest, DownloadControlTypesNewClient) {
  InitializeBackend(true);
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            fake_manager_->GetAndResetConfigureReason());
}

// Test that configuration on restart sends the proper GU source.
TEST_F(SyncEngineImplTest, DownloadControlTypesRestart) {
  sync_prefs_->SetFirstSetupComplete();
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend(true);
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            fake_manager_->GetAndResetConfigureReason());
}

// If bookmarks encounter an error that results in disabling without purging
// (such as when the type is unready), and then is explicitly disabled, the
// SyncEngine needs to tell the manager to purge the type, even though
// it's already disabled (crbug.com/386778).
TEST_F(SyncEngineImplTest, DisableThenPurgeType) {
  ModelTypeSet error_types(BOOKMARKS);

  InitializeBackend(true);

  // First enable the types.
  ModelTypeSet ready_types = ConfigureDataTypes();

  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);

  // Then mark the error types as unready (disables without purging).
  ready_types = ConfigureDataTypesWithUnready(error_types);
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(error_types).Empty());

  // Lastly explicitly disable the error types, which should result in a purge.
  enabled_types_.RemoveAll(error_types);
  ready_types = ConfigureDataTypes();
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);
  EXPECT_FALSE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(error_types).Empty());
}

// Tests that SyncEngineImpl retains ModelTypeConnector after call to
// StopSyncingForShutdown. This is needed for datatype deactivation during
// DataTypeManager shutdown.
TEST_F(SyncEngineImplTest, ModelTypeConnectorValidDuringShutdown) {
  InitializeBackend(true);
  backend_->StopSyncingForShutdown();
  // Verify that call to DeactivateNonBlockingDataType doesn't assert.
  backend_->DeactivateNonBlockingDataType(AUTOFILL);
  backend_->Shutdown(STOP_SYNC);
  backend_.reset();
}

TEST_F(SyncEngineImplTest,
       NoisyDataTypesInvalidationAreDiscardedByDefaultOnAndroid) {
  // Making sure that the noisy types we're interested in are in the
  // |enabled_types_|.
  enabled_types_.Put(SESSIONS);
  enabled_types_.Put(FAVICON_IMAGES);
  enabled_types_.Put(FAVICON_TRACKING);

  ModelTypeSet invalidation_enabled_types(enabled_types_);

#if defined(OS_ANDROID)
  // SESSIONS, FAVICON_IMAGES, FAVICON_TRACKING are noisy data types whose
  // invalidations aren't enabled by default on Android.
  invalidation_enabled_types.Remove(SESSIONS);
  invalidation_enabled_types.Remove(FAVICON_IMAGES);
  invalidation_enabled_types.Remove(FAVICON_TRACKING);
#endif

  InitializeBackend(true);
  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(
                  backend_.get(),
                  ModelTypeSetToObjectIdSet(invalidation_enabled_types)));
  ConfigureDataTypes();

  // At shutdown, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(backend_.get(), ObjectIdSet()));
}

TEST_F(SyncEngineImplTest, WhenEnabledTypesStayDisabled) {
  // Testing that noisy types doesn't used for registration, when
  // they're disabled in Sync, hence removing noisy datatypes from
  // |enabled_types_|.
  enabled_types_.Remove(SESSIONS);
  enabled_types_.Remove(FAVICON_IMAGES);
  enabled_types_.Remove(FAVICON_TRACKING);

  InitializeBackend(true);
  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(
                  backend_.get(), ModelTypeSetToObjectIdSet(enabled_types_)));
  ConfigureDataTypes();

  // At shutdown, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(backend_.get(), ObjectIdSet()));
}

TEST_F(SyncEngineImplTest,
       EnabledTypesChangesWhenSetInvalidationsForSessionsCalled) {
  // Making sure that the noisy types we're interested in are in the
  // |enabled_types_|.
  enabled_types_.Put(SESSIONS);
  enabled_types_.Put(FAVICON_IMAGES);
  enabled_types_.Put(FAVICON_TRACKING);

  InitializeBackend(true);
  ConfigureDataTypes();

  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(
                  backend_.get(), ModelTypeSetToObjectIdSet(enabled_types_)));
  backend_->SetInvalidationsForSessionsEnabled(true);

  ModelTypeSet enabled_types(enabled_types_);
  enabled_types.Remove(SESSIONS);
  enabled_types.Remove(FAVICON_IMAGES);
  enabled_types.Remove(FAVICON_TRACKING);

  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(
                  backend_.get(), ModelTypeSetToObjectIdSet(enabled_types)));
  backend_->SetInvalidationsForSessionsEnabled(false);

  // At shutdown, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateRegisteredInvalidationIds(backend_.get(), ObjectIdSet()));
}

// Regression test for crbug.com/1019956.
TEST_F(SyncEngineImplTest, ShouldDestroyAfterInitFailure) {
  base::test::ScopedFeatureList override_features;
  override_features.InitWithFeatures(
      /*enable_feature=*/{switches::kSyncUSSPasswords,
                          switches::kSyncUSSNigori},
      /*disable_feature=*/{});

  fake_manager_factory_->set_should_fail_on_init(true);
  // Sync manager will report initialization failure and gets destroyed during
  // the error handling.
  InitializeBackend(false);

  backend_->StopSyncingForShutdown();
  // This line would post the task causing the crash before the fix, because
  // sync manager was used during the shutdown handling.
  backend_->Shutdown(STOP_SYNC);
  backend_.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace syncer
