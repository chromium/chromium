// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/active_devices_provider.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_sync_manager.h"
#include "components/sync/test/mock_sync_invalidations_service.h"
#include "components/sync/test/sync_engine_host_stub.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace syncer {

namespace {

static const base::FilePath::CharType kTestSyncDir[] =
    FILE_PATH_LITERAL("sync-test");
constexpr char kTestGaiaId[] = "test_gaia_id";

class TestSyncEngineHost : public SyncEngineHostStub {
 public:
  TestSyncEngineHost() = default;

  void OnEngineInitialized(bool success,
                           bool is_first_time_sync_configure) override {
    EXPECT_EQ(expect_success_, success);
    std::move(quit_closure_).Run();
  }

  void SetExpectSuccess(bool expect_success) {
    expect_success_ = expect_success;
  }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  bool expect_success_ = false;
  base::OnceClosure quit_closure_;
};

class FakeSyncManagerFactory : public SyncManagerFactory {
 public:
  explicit FakeSyncManagerFactory(
      FakeSyncManager** fake_manager,
      network::NetworkConnectionTracker* network_connection_tracker)
      : SyncManagerFactory(network_connection_tracker),
        fake_manager_(fake_manager) {
    *fake_manager_ = nullptr;
  }
  ~FakeSyncManagerFactory() override = default;

  // SyncManagerFactory implementation.  Called on the sync thread.
  std::unique_ptr<SyncManager> CreateSyncManager(
      const std::string& /* name */) override {
    *fake_manager_ =
        new FakeSyncManager(initial_sync_ended_types_, progress_marker_types_,
                            configure_fail_types_);
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

 private:
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;
  ModelTypeSet configure_fail_types_;
  raw_ptr<FakeSyncManager*> fake_manager_;
};

class MockInvalidationService : public invalidation::InvalidationService {
 public:
  MockInvalidationService() = default;
  ~MockInvalidationService() override = default;
  MOCK_METHOD(void,
              RegisterInvalidationHandler,
              (invalidation::InvalidationHandler * handler),
              (override));
  MOCK_METHOD(bool,
              UpdateInterestedTopics,
              (invalidation::InvalidationHandler * handler,
               const invalidation::TopicSet& topics),
              (override));
  MOCK_METHOD(void,
              UnsubscribeFromUnregisteredTopics,
              (invalidation::InvalidationHandler * handler),
              (override));
  MOCK_METHOD(void,
              UnregisterInvalidationHandler,
              (invalidation::InvalidationHandler * handler),
              (override));
  MOCK_METHOD(invalidation::InvalidatorState,
              GetInvalidatorState,
              (),
              (const override));
  MOCK_METHOD(std::string, GetInvalidatorClientId, (), (const override));
  MOCK_METHOD(invalidation::InvalidationLogger*,
              GetInvalidationLogger,
              (),
              (override));
  MOCK_METHOD(void,
              RequestDetailedStatus,
              (base::RepeatingCallback<void(base::Value::Dict)> post_caller),
              (const override));
};

class MockActiveDevicesProvider : public ActiveDevicesProvider {
 public:
  MockActiveDevicesProvider() = default;
  ~MockActiveDevicesProvider() override = default;

  MOCK_METHOD(void,
              SetActiveDevicesChangedCallback,
              (ActiveDevicesProvider::ActiveDevicesChangedCallback),
              (override));
  MOCK_METHOD(ActiveDevicesInvalidationInfo,
              CalculateInvalidationInfo,
              (const std::string&),
              (const override));
};

std::unique_ptr<HttpPostProviderFactory> CreateHttpBridgeFactory() {
  return std::make_unique<HttpBridgeFactory>(
      /*user_agent=*/"",
      /*pending_url_loader_factory=*/nullptr);
}

class SyncEngineImplTest : public testing::Test {
 protected:
  SyncEngineImplTest() = default;
  ~SyncEngineImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());

    ON_CALL(invalidator_, UpdateInterestedTopics)
        .WillByDefault(testing::Return(true));
    scoped_refptr<base::SequencedTaskRunner> sync_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    auto mock_active_devices_provider =
        std::make_unique<NiceMock<MockActiveDevicesProvider>>();
    ON_CALL(*mock_active_devices_provider.get(), CalculateInvalidationInfo)
        .WillByDefault(Return(
            ByMove(ActiveDevicesInvalidationInfo::CreateUninitialized())));
    backend_ = std::make_unique<SyncEngineImpl>(
        "dummyDebugName", &invalidator_, GetSyncInvalidationsService(),
        std::move(mock_active_devices_provider),
        std::make_unique<SyncTransportDataPrefs>(&pref_service_),
        temp_dir_.GetPath().Append(base::FilePath(kTestSyncDir)),
        std::move(sync_task_runner), sync_transport_data_cleared_cb_.Get());

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
      ShutdownBackend(ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA);
    }
    // Pump messages posted by the sync thread.
    base::RunLoop().RunUntilIdle();
  }

  // Synchronously initializes the backend.
  void InitializeBackend(bool expect_success = true,
                         const std::string& gaia_id = kTestGaiaId) {
    host_.SetExpectSuccess(expect_success);

    SyncEngine::InitParams params;
    params.host = &host_;
    params.http_factory_getter = base::BindOnce(&CreateHttpBridgeFactory);
    params.authenticated_account_info.gaia = gaia_id;
    params.authenticated_account_info.account_id = CoreAccountId("account_id");
    params.sync_manager_factory = std::move(fake_manager_factory_);

    backend_->Initialize(std::move(params));

    PumpSyncThread();
    // |fake_manager_| is set on the sync thread, but we can rely on the message
    // loop barriers to guarantee that we see the updated value.
    DCHECK(fake_manager_);

    if (expect_success) {
      EXPECT_TRUE(engine_types_.Empty());
      engine_types_ = fake_manager_->GetConnectedTypes();
    }
  }

  void ShutdownBackend(ShutdownReason reason) {
    DCHECK(backend_);
    backend_->StopSyncingForShutdown();
    backend_->Shutdown(reason);
    backend_.reset();
  }

  // Synchronously configures the backend's datatypes.
  ModelTypeSet ConfigureDataTypes() {
    return ConfigureDataTypesWithUnready(ModelTypeSet());
  }

  ModelTypeSet ConfigureDataTypesWithUnready(ModelTypeSet unready_types) {
    ModelTypeConfigurer::ConfigureParams params;
    params.reason = CONFIGURE_REASON_RECONFIGURATION;
    ModelTypeSet enabled_types = Difference(enabled_types_, unready_types);
    params.to_download = Difference(enabled_types, engine_types_);
    if (!params.to_download.Empty()) {
      params.to_download.Put(NIGORI);
    }
    params.to_purge = Difference(engine_types_, enabled_types_);
    params.ready_task = base::BindOnce(&SyncEngineImplTest::DownloadReady,
                                       base::Unretained(this));

    ModelTypeSet ready_types = Difference(enabled_types, params.to_download);
    backend_->ConfigureDataTypes(std::move(params));
    PumpSyncThread();

    return ready_types;
  }

 protected:
  // Used to initialize SyncEngineImpl. Returns nullptr if there is no sync
  // invalidations service enabled.
  virtual SyncInvalidationsService* GetSyncInvalidationsService() {
    return nullptr;
  }

  void DownloadReady(ModelTypeSet succeeded_types, ModelTypeSet failed_types) {
    engine_types_.PutAll(succeeded_types);

    backend_->StartSyncingWithServer();
    std::move(quit_loop_).Run();
  }

  void PumpSyncThread() {
    base::RunLoop run_loop;
    quit_loop_ = run_loop.QuitClosure();
    host_.set_quit_closure(run_loop.QuitClosure());
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
  TestSyncEngineHost host_;
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      sync_transport_data_cleared_cb_;
  std::unique_ptr<SyncEngineImpl> backend_;
  std::unique_ptr<FakeSyncManagerFactory> fake_manager_factory_;
  FakeSyncManager* fake_manager_ = nullptr;
  ModelTypeSet engine_types_;
  ModelTypeSet enabled_types_;
  base::OnceClosure quit_loop_;
  testing::NiceMock<MockInvalidationService> invalidator_;
};

class SyncEngineImplWithSyncInvalidationsTest : public SyncEngineImplTest {
 public:
  SyncEngineImplWithSyncInvalidationsTest() {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{kSyncSendInterestedDataTypes,
                              kUseSyncInvalidations},
        /*disabled_features=*/{});
  }

 protected:
  SyncInvalidationsService* GetSyncInvalidationsService() override {
    return &mock_sync_invalidations_service_;
  }

  base::test::ScopedFeatureList override_features_;
  NiceMock<MockSyncInvalidationsService> mock_sync_invalidations_service_;
};

class SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest
    : public SyncEngineImplTest {
 public:
  SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest() {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{kSyncSendInterestedDataTypes,
                              kUseSyncInvalidations,
                              kUseSyncInvalidationsForWalletAndOffer},
        /*disabled_features=*/{});
  }

  SyncInvalidationsService* GetSyncInvalidationsService() override {
    return &mock_sync_invalidations_service_;
  }

 protected:
  base::test::ScopedFeatureList override_features_;
  NiceMock<MockSyncInvalidationsService> mock_sync_invalidations_service_;
};

// Test basic initialization with no initial types (first time initialization).
// Only the nigori should be configured.
TEST_F(SyncEngineImplTest, InitShutdownWithStopSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  EXPECT_CALL(sync_transport_data_cleared_cb_, Run()).Times(0);
  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplTest, InitShutdownWithDisableSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  EXPECT_CALL(sync_transport_data_cleared_cb_, Run());
  ShutdownBackend(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA);
}

// Test first time sync scenario. All types should be properly configured.

TEST_F(SyncEngineImplTest, FirstTimeSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      Difference(enabled_types_, ControlTypes())));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

// Test the restart after setting up sync scenario. No enabled types should be
// downloaded.
TEST_F(SyncEngineImplTest, Restart) {
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend();
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  ModelTypeSet ready_types = ConfigureDataTypes();
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

TEST_F(SyncEngineImplTest, DisableTypes) {
  // Simulate first time sync.
  InitializeBackend();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then disable two datatypes.
  ModelTypeSet disabled_types(BOOKMARKS, SEARCH_ENGINES);
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes disabled should be cleaned. Nothing should be
  // downloaded.
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
}

TEST_F(SyncEngineImplTest, AddTypes) {
  // Simulate first time sync.
  InitializeBackend();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then add two datatypes.
  ModelTypeSet new_types(EXTENSIONS, APPS);
  enabled_types_.PutAll(new_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(enabled_types_, new_types), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

// And and disable in the same configuration.
TEST_F(SyncEngineImplTest, AddDisableTypes) {
  // Simulate first time sync.
  InitializeBackend();
  ModelTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then add two datatypes.
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
}

// Test restarting the browser to newly supported datatypes. The new datatypes
// should be downloaded on the configuration after backend initialization.
TEST_F(SyncEngineImplTest, NewlySupportedTypes) {
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  ModelTypeSet old_types = enabled_types_;
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);
  ModelTypeSet new_types(APP_SETTINGS, EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Does nothing.
  InitializeBackend();
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_EQ(old_types, fake_manager_->InitialSyncEndedTypes());

  // Downloads and applies the new types (plus nigori).
  ModelTypeSet ready_types = ConfigureDataTypes();

  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(old_types, ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

// Verify that downloading control types only downloads those types that do
// not have initial sync ended set.
TEST_F(SyncEngineImplTest, DownloadControlTypes) {
  // Set sync manager behavior before passing it down. Experiments and device
  // info are new types without progress markers or initial sync ended, while
  // all other types have been fully downloaded and applied.
  ModelTypeSet new_types(NIGORI);
  ModelTypeSet old_types = Difference(enabled_types_, new_types);
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);

  // Bringing up the backend should download the new types without downloading
  // any old types.
  InitializeBackend();
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
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
  InitializeBackend(/*expect_success=*/false);
}

// Test that local refresh requests are delivered to sync.
TEST_F(SyncEngineImplTest, ForwardLocalRefreshRequest) {
  InitializeBackend();

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
  InitializeBackend();
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            fake_manager_->GetAndResetConfigureReason());
}

// Test that configuration on restart sends the proper GU source.
TEST_F(SyncEngineImplTest, DownloadControlTypesRestart) {
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend();
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            fake_manager_->GetAndResetConfigureReason());
}

// If bookmarks encounter an error that results in disabling without purging
// (such as when the type is unready), and then is explicitly disabled, the
// SyncEngine needs to tell the manager to purge the type, even though
// it's already disabled (crbug.com/386778).
TEST_F(SyncEngineImplTest, DisableThenPurgeType) {
  ModelTypeSet error_types(BOOKMARKS);

  InitializeBackend();

  // First enable the types.
  ModelTypeSet ready_types = ConfigureDataTypes();

  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);

  // Then mark the error types as unready (disables without purging).
  ready_types = ConfigureDataTypesWithUnready(error_types);
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);

  // Lastly explicitly disable the error types, which should result in a purge.
  enabled_types_.RemoveAll(error_types);
  ready_types = ConfigureDataTypes();
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);
}

// Tests that SyncEngineImpl retains ModelTypeConnector after call to
// StopSyncingForShutdown. This is needed for datatype deactivation during
// DataTypeManager shutdown.
TEST_F(SyncEngineImplTest, ModelTypeConnectorValidDuringShutdown) {
  InitializeBackend();
  backend_->StopSyncingForShutdown();
  // Verify that call to DisconnectDataType doesn't assert.
  backend_->DisconnectDataType(AUTOFILL);
  backend_->Shutdown(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
  backend_.reset();
}

TEST_F(SyncEngineImplTest,
       NoisyDataTypesInvalidationAreDiscardedByDefaultOnAndroid) {
  // Making sure that the noisy types we're interested in are in the
  // |enabled_types_|.
  enabled_types_.Put(SESSIONS);

  ModelTypeSet invalidation_enabled_types(
      Difference(enabled_types_, CommitOnlyTypes()));

#if BUILDFLAG(IS_ANDROID)
  // SESSIONS is a noisy data type whose invalidations aren't enabled by default
  // on Android.
  invalidation_enabled_types.Remove(SESSIONS);
#endif

  InitializeBackend();
  EXPECT_CALL(
      invalidator_,
      UpdateInterestedTopics(
          backend_.get(), ModelTypeSetToTopicSet(invalidation_enabled_types)));
  ConfigureDataTypes();

  // When Sync is stopped, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(), invalidation::TopicSet()));
  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplTest, WhenEnabledTypesStayDisabled) {
  // Tests that noisy types aren't used for registration if they're disabled,
  // hence removing noisy datatypes from |enabled_types_|.
  enabled_types_.Remove(SESSIONS);

  InitializeBackend();
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(),
                                     ModelTypeSetToTopicSet(Difference(
                                         enabled_types_, CommitOnlyTypes()))));
  ConfigureDataTypes();

  // When Sync is stopped, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(), invalidation::TopicSet()));
  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplTest,
       EnabledTypesChangesWhenSetInvalidationsForSessionsCalled) {
  // Making sure that the noisy types we're interested in are in the
  // |enabled_types_|.
  enabled_types_.Put(SESSIONS);

  InitializeBackend();
  ConfigureDataTypes();

  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(),
                                     ModelTypeSetToTopicSet(Difference(
                                         enabled_types_, CommitOnlyTypes()))));
  backend_->SetInvalidationsForSessionsEnabled(true);

  ModelTypeSet enabled_types(enabled_types_);
  enabled_types.Remove(SESSIONS);

  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(),
                                     ModelTypeSetToTopicSet(Difference(
                                         enabled_types, CommitOnlyTypes()))));
  backend_->SetInvalidationsForSessionsEnabled(false);

  // When Sync is stopped, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(), invalidation::TopicSet()));
  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplWithSyncInvalidationsTest,
       ShouldInvalidateDataTypesOnIncomingInvalidation) {
  enabled_types_.PutAll({syncer::BOOKMARKS, syncer::PREFERENCES});

  InitializeBackend(/*expect_success=*/true);
  ConfigureDataTypes();

  sync_pb::SyncInvalidationsPayload payload;
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      bookmarks_invalidation = payload.add_data_type_invalidations();
  bookmarks_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(ModelType::BOOKMARKS));
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      preferences_invalidation = payload.add_data_type_invalidations();
  preferences_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(ModelType::PREFERENCES));

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillOnce(Return(enabled_types_));
  backend_->OnInvalidationReceived(payload.SerializeAsString());

  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(ModelType::BOOKMARKS));
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(ModelType::PREFERENCES));
}

TEST_F(SyncEngineImplWithSyncInvalidationsTest,
       ShouldInvalidateOnlyEnabledDataTypes) {
  enabled_types_.Remove(syncer::BOOKMARKS);
  enabled_types_.Put(syncer::PREFERENCES);

  InitializeBackend(/*expect_success=*/true);
  ConfigureDataTypes();

  sync_pb::SyncInvalidationsPayload payload;
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      bookmarks_invalidation = payload.add_data_type_invalidations();
  bookmarks_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(ModelType::BOOKMARKS));
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      preferences_invalidation = payload.add_data_type_invalidations();
  preferences_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(ModelType::PREFERENCES));

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillOnce(Return(enabled_types_));
  backend_->OnInvalidationReceived(payload.SerializeAsString());

  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(0, fake_manager_->GetInvalidationCount(ModelType::BOOKMARKS));
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(ModelType::PREFERENCES));
}

TEST_F(SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest,
       ShouldStartHandlingInvalidations) {
  ON_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillByDefault(Return(enabled_types_));
  EXPECT_CALL(mock_sync_invalidations_service_, AddListener(backend_.get()));
  backend_->StartHandlingInvalidations();
}

TEST_F(SyncEngineImplWithSyncInvalidationsTest,
       UseOldInvalidationsOnlyForWalletAndOffer) {
  enabled_types_.PutAll({AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillRepeatedly(Return(enabled_types_));
  InitializeBackend(/*expect_success=*/true);
  EXPECT_CALL(
      invalidator_,
      UpdateInterestedTopics(
          backend_.get(), ModelTypeSetToTopicSet(
                              {AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER})));
  ConfigureDataTypes();

  // When Sync is stopped, we clear the registered invalidation ids.
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(backend_.get(), invalidation::TopicSet()));
  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest,
       DoNotUseOldInvalidationsAtAll) {
  enabled_types_.PutAll({AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});

  // Since the old invalidations system is not being used anymore (based on the
  // enabled feature flags), SyncEngine should call the (old) invalidator with
  // an empty TopicSet upon initialization.
  EXPECT_CALL(invalidator_,
              UpdateInterestedTopics(_, invalidation::TopicSet()));
  EXPECT_CALL(invalidator_, UnsubscribeFromUnregisteredTopics);
  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillRepeatedly(Return(enabled_types_));
  InitializeBackend(/*expect_success=*/true);

  EXPECT_CALL(invalidator_, UpdateInterestedTopics).Times(0);
  ConfigureDataTypes();
}

TEST_F(SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest,
       ShouldEnableInvalidationsWhenInitialized) {
  EXPECT_CALL(mock_sync_invalidations_service_, GetFCMRegistrationToken)
      .WillRepeatedly(Return("fcm_token"));
  InitializeBackend(/*expect_success=*/true);
  fake_manager_->WaitForSyncThread();
  EXPECT_TRUE(fake_manager_->IsInvalidatorEnabled());
}

TEST_F(SyncEngineImplWithSyncInvalidationsForWalletAndOfferTest,
       ShouldEnableInvalidationsOnTokenUpdate) {
  EXPECT_CALL(mock_sync_invalidations_service_, GetFCMRegistrationToken)
      .WillRepeatedly(Return(absl::nullopt));
  InitializeBackend(/*expect_success=*/true);
  fake_manager_->WaitForSyncThread();
  EXPECT_FALSE(fake_manager_->IsInvalidatorEnabled());

  EXPECT_CALL(mock_sync_invalidations_service_, GetFCMRegistrationToken)
      .WillRepeatedly(Return("fcm_token"));
  backend_->OnFCMRegistrationTokenChanged();
  fake_manager_->WaitForSyncThread();
  EXPECT_TRUE(fake_manager_->IsInvalidatorEnabled());
}

TEST_F(SyncEngineImplTest, GenerateCacheGUID) {
  const std::string guid1 = SyncEngineImpl::GenerateCacheGUIDForTest();
  const std::string guid2 = SyncEngineImpl::GenerateCacheGUIDForTest();
  EXPECT_EQ(24U, guid1.size());
  EXPECT_EQ(24U, guid2.size());
  EXPECT_NE(guid1, guid2);
}

TEST_F(SyncEngineImplTest, ShouldPopulateAccountIdCachedInPrefs) {
  const std::string kTestCacheGuid = "test_cache_guid";
  const std::string kTestBirthday = "test_birthday";

  SyncTransportDataPrefs transport_data_prefs(&pref_service_);
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);

  InitializeBackend();

  ASSERT_EQ(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_EQ(kTestGaiaId, transport_data_prefs.GetGaiaId());
}

TEST_F(SyncEngineImplTest,
       ShouldNotPopulateAccountIdCachedInPrefsWithLocalSync) {
  const std::string kTestCacheGuid = "test_cache_guid";
  const std::string kTestBirthday = "test_birthday";

  SyncTransportDataPrefs transport_data_prefs(&pref_service_);
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);

  InitializeBackend(/*expect_success=*/true, /*gaia_id=*/std::string());

  ASSERT_EQ(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_TRUE(transport_data_prefs.GetGaiaId().empty());
}

TEST_F(SyncEngineImplTest, ShouldLoadSyncDataUponInitialization) {
  const std::string kTestCacheGuid = "test_cache_guid";
  const std::string kTestBirthday = "test_birthday";

  SyncTransportDataPrefs transport_data_prefs(&pref_service_);
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);
  transport_data_prefs.SetGaiaId(kTestGaiaId);

  EXPECT_CALL(sync_transport_data_cleared_cb_, Run()).Times(0);
  InitializeBackend();

  EXPECT_EQ(kTestGaiaId, transport_data_prefs.GetGaiaId());
  EXPECT_EQ(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_EQ(kTestBirthday, transport_data_prefs.GetBirthday());
}

// Verifies that local sync transport data is thrown away if there is a mismatch
// between the account ID cached in SyncPrefs and the actual one.
TEST_F(SyncEngineImplTest,
       ShouldClearLocalSyncTransportDataDueToAccountIdMismatch) {
  const std::string kTestCacheGuid = "test_cache_guid";
  const std::string kTestBirthday = "test_birthday";

  SyncTransportDataPrefs transport_data_prefs(&pref_service_);
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);
  transport_data_prefs.SetGaiaId("corrupt_gaia_id");

  EXPECT_CALL(sync_transport_data_cleared_cb_, Run());
  InitializeBackend();

  EXPECT_EQ(kTestGaiaId, transport_data_prefs.GetGaiaId());
  EXPECT_NE(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_NE(kTestBirthday, transport_data_prefs.GetBirthday());
}

}  // namespace

}  // namespace syncer
