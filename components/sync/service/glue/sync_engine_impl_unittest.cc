// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/glue/sync_engine_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/service/active_devices_provider.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/test/fake_sync_manager.h"
#include "components/sync/test/mock_sync_invalidations_service.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;

namespace syncer {

namespace {

static const base::FilePath::CharType kTestSyncDir[] =
    FILE_PATH_LITERAL("sync-test");
constexpr char kTestGaiaId[] = "test_gaia_id";
constexpr char kTestCacheGuid[] = "test_cache_guid";
constexpr char kTestBirthday[] = "test_birthday";

class MockSyncEngineHost : public SyncEngineHost {
 public:
  MOCK_METHOD(void,
              OnEngineInitialized,
              (bool success, bool is_first_time_sync_configure),
              (override));
  MOCK_METHOD(void,
              OnSyncCycleCompleted,
              (const SyncCycleSnapshot& snapshot),
              (override));
  MOCK_METHOD(void, OnProtocolEvent, (const ProtocolEvent& event), (override));
  MOCK_METHOD(void,
              OnConnectionStatusChange,
              (ConnectionStatus status),
              (override));
  MOCK_METHOD(void, OnMigrationNeededForTypes, (DataTypeSet types), (override));
  MOCK_METHOD(void,
              OnActionableProtocolError,
              (const SyncProtocolError& error),
              (override));
  MOCK_METHOD(void, OnBackedOffTypesChanged, (), (override));
  MOCK_METHOD(void, OnInvalidationStatusChanged, (), (override));
  MOCK_METHOD(void, OnNewInvalidatedDataTypes, (), (override));
};

class FakeSyncManagerFactory : public SyncManagerFactory {
 public:
  FakeSyncManagerFactory(
      raw_ptr<FakeSyncManager>* fake_manager,
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

  void set_initial_sync_ended_types(DataTypeSet types) {
    initial_sync_ended_types_ = types;
  }

  void set_progress_marker_types(DataTypeSet types) {
    progress_marker_types_ = types;
  }

  void set_configure_fail_types(DataTypeSet types) {
    configure_fail_types_ = types;
  }

 private:
  DataTypeSet initial_sync_ended_types_;
  DataTypeSet progress_marker_types_;
  DataTypeSet configure_fail_types_;
  const raw_ptr<raw_ptr<FakeSyncManager>> fake_manager_;
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
        "fakeDebugName", &mock_sync_invalidations_service_,
        std::move(mock_active_devices_provider),
        std::make_unique<SyncTransportDataPrefs>(
            &pref_service_, signin::GaiaIdHash::FromGaiaId(kTestGaiaId)),
        temp_dir_.GetPath().Append(base::FilePath(kTestSyncDir)),
        std::move(sync_task_runner));

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
    SyncEngine::InitParams params;
    params.host = &mock_host_;
    params.http_factory_getter = base::BindOnce(&CreateHttpBridgeFactory);
    params.authenticated_account_info.gaia = gaia_id;
    params.authenticated_account_info.account_id =
        CoreAccountId::FromGaiaId(gaia_id);
    params.sync_manager_factory = std::move(fake_manager_factory_);

    EXPECT_CALL(mock_host_, OnEngineInitialized(expect_success, _))
        .WillOnce(
            testing::InvokeWithoutArgs(this, &SyncEngineImplTest::QuitRunLoop));
    backend_->Initialize(std::move(params));
    PumpSyncThread();
    // |fake_manager_| is set on the sync thread, but we can rely on the message
    // loop barriers to guarantee that we see the updated value.
    DCHECK(fake_manager_);

    if (expect_success) {
      EXPECT_TRUE(engine_types_.empty());
      engine_types_ = fake_manager_->GetConnectedTypes();
      ON_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes)
          .WillByDefault(Return(engine_types_));
    }
  }

  void ShutdownBackend(ShutdownReason reason) {
    DCHECK(backend_);
    backend_->StopSyncingForShutdown();
    // Reset `fake_manager_` to avoid dangling pointer.
    fake_manager_ = nullptr;
    backend_->Shutdown(reason);
    backend_.reset();
  }

  // Synchronously configures the backend's datatypes.
  DataTypeSet ConfigureDataTypes() {
    return ConfigureDataTypesWithUnready(DataTypeSet());
  }

  DataTypeSet ConfigureDataTypesWithUnready(DataTypeSet unready_types) {
    DataTypeConfigurer::ConfigureParams params;
    params.reason = CONFIGURE_REASON_RECONFIGURATION;
    DataTypeSet enabled_types = Difference(enabled_types_, unready_types);
    params.to_download = Difference(enabled_types, engine_types_);
    if (!params.to_download.empty()) {
      params.to_download.Put(NIGORI);
    }
    params.ready_task = base::BindOnce(&SyncEngineImplTest::DownloadReady,
                                       base::Unretained(this));

    DataTypeSet ready_types = Difference(enabled_types, params.to_download);
    backend_->ConfigureDataTypes(std::move(params));
    PumpSyncThread();

    return ready_types;
  }

 protected:
  void DownloadReady(DataTypeSet succeeded_types, DataTypeSet failed_types) {
    engine_types_.PutAll(succeeded_types);

    backend_->StartSyncingWithServer();
    QuitRunLoop();
  }

  void QuitRunLoop() { std::move(quit_loop_).Run(); }

  void PumpSyncThread() {
    base::RunLoop run_loop;
    quit_loop_ = run_loop.QuitClosure();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SyncEngineImplTest::QuitRunLoop,
                       weak_ptr_factory_.GetWeakPtr()),
        TestTimeouts::action_timeout());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
  NiceMock<MockSyncEngineHost> mock_host_;
  std::unique_ptr<SyncEngineImpl> backend_;
  std::unique_ptr<FakeSyncManagerFactory> fake_manager_factory_;
  raw_ptr<FakeSyncManager> fake_manager_ = nullptr;
  DataTypeSet engine_types_;
  DataTypeSet enabled_types_;
  base::OnceClosure quit_loop_;
  NiceMock<MockSyncInvalidationsService> mock_sync_invalidations_service_;

  base::WeakPtrFactory<SyncEngineImplTest> weak_ptr_factory_{this};
};

// Test basic initialization with no initial types (first time initialization).
// Only the nigori should be configured.
TEST_F(SyncEngineImplTest, InitShutdownWithStopSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  ShutdownBackend(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
}

TEST_F(SyncEngineImplTest, InitShutdownWithDisableSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  ShutdownBackend(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA);
}

// Test first time sync scenario. All types should be properly configured.

TEST_F(SyncEngineImplTest, FirstTimeSync) {
  InitializeBackend();
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());

  DataTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), {NIGORI}), ready_types);
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
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  DataTypeSet ready_types = ConfigureDataTypes();
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

TEST_F(SyncEngineImplTest, DisableTypes) {
  // Simulate first time sync.
  InitializeBackend();
  DataTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), {NIGORI}), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then disable two datatypes.
  DataTypeSet disabled_types = {BOOKMARKS, SEARCH_ENGINES};
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes();

  // Only those datatypes disabled should be cleaned. Nothing should be
  // downloaded.
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().empty());
}

TEST_F(SyncEngineImplTest, AddTypes) {
  // Simulate first time sync.
  InitializeBackend();
  DataTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), {NIGORI}), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then add two datatypes.
  DataTypeSet new_types = {EXTENSIONS, APPS};
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
  DataTypeSet ready_types = ConfigureDataTypes();
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), {NIGORI}), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());

  // Then add two datatypes.
  DataTypeSet disabled_types = {BOOKMARKS, SEARCH_ENGINES};
  DataTypeSet new_types = {EXTENSIONS, APPS};
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
  DataTypeSet old_types = enabled_types_;
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);
  DataTypeSet new_types = {APP_SETTINGS, EXTENSION_SETTINGS};
  enabled_types_.PutAll(new_types);

  // Does nothing.
  InitializeBackend();
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().empty());
  EXPECT_EQ(old_types, fake_manager_->InitialSyncEndedTypes());

  // Downloads and applies the new types (plus nigori).
  DataTypeSet ready_types = ConfigureDataTypes();

  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(old_types, {NIGORI}), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
}

// Verify that downloading control types only downloads those types that do
// not have initial sync ended set.
TEST_F(SyncEngineImplTest, DownloadControlTypes) {
  // Set sync manager behavior before passing it down. Experiments and device
  // info are new types without progress markers or initial sync ended, while
  // all other types have been fully downloaded and applied.
  DataTypeSet new_types = {NIGORI};
  DataTypeSet old_types = Difference(enabled_types_, new_types);
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
  fake_manager_factory_->set_configure_fail_types(DataTypeSet::All());
  InitializeBackend(/*expect_success=*/false);
}

// Test that local refresh requests are delivered to sync.
TEST_F(SyncEngineImplTest, ForwardLocalRefreshRequest) {
  InitializeBackend();

  const DataTypeSet set1 = DataTypeSet::All();
  backend_->TriggerRefresh(set1);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(set1, fake_manager_->GetLastRefreshRequestTypes());

  const DataTypeSet set2 = {SESSIONS};
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

// Tests that SyncEngineImpl retains DataTypeConnector after call to
// StopSyncingForShutdown. This is needed for datatype deactivation during
// DataTypeManager shutdown.
TEST_F(SyncEngineImplTest, DataTypeConnectorValidDuringShutdown) {
  InitializeBackend();
  backend_->StopSyncingForShutdown();
  // Verify that call to DisconnectDataType doesn't assert.
  backend_->DisconnectDataType(AUTOFILL);
  // Reset `fake_manager_` to avoid dangling pointer.
  fake_manager_ = nullptr;
  backend_->Shutdown(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
  backend_.reset();
}

TEST_F(SyncEngineImplTest, ShouldInvalidateDataTypesOnIncomingInvalidation) {
  enabled_types_.PutAll({syncer::BOOKMARKS, syncer::PREFERENCES});

  InitializeBackend(/*expect_success=*/true);
  ConfigureDataTypes();

  sync_pb::SyncInvalidationsPayload payload;
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      bookmarks_invalidation = payload.add_data_type_invalidations();
  bookmarks_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(DataType::BOOKMARKS));
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      preferences_invalidation = payload.add_data_type_invalidations();
  preferences_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(DataType::PREFERENCES));

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillOnce(Return(enabled_types_));
  backend_->OnInvalidationReceived(payload.SerializeAsString());

  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(DataType::BOOKMARKS));
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(DataType::PREFERENCES));
}

TEST_F(SyncEngineImplTest, ShouldInvalidateOnlyEnabledDataTypes) {
  enabled_types_.Remove(syncer::BOOKMARKS);
  enabled_types_.Put(syncer::PREFERENCES);

  InitializeBackend(/*expect_success=*/true);
  ConfigureDataTypes();

  sync_pb::SyncInvalidationsPayload payload;
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      bookmarks_invalidation = payload.add_data_type_invalidations();
  bookmarks_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(DataType::BOOKMARKS));
  sync_pb::SyncInvalidationsPayload::DataTypeInvalidation*
      preferences_invalidation = payload.add_data_type_invalidations();
  preferences_invalidation->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(DataType::PREFERENCES));

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillOnce(Return(enabled_types_));
  backend_->OnInvalidationReceived(payload.SerializeAsString());

  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(0, fake_manager_->GetInvalidationCount(DataType::BOOKMARKS));
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount(DataType::PREFERENCES));
}

TEST_F(SyncEngineImplTest, ShouldStartHandlingInvalidations) {
  InitializeBackend(/*expect_success=*/true);

  EXPECT_CALL(mock_sync_invalidations_service_, AddListener(backend_.get()));
  backend_->StartHandlingInvalidations();
}

TEST_F(SyncEngineImplTest, DoNotUseOldInvalidationsAtAll) {
  enabled_types_.PutAll({AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});

  EXPECT_CALL(mock_sync_invalidations_service_, GetInterestedDataTypes())
      .WillRepeatedly(Return(enabled_types_));
  InitializeBackend(/*expect_success=*/true);

  ConfigureDataTypes();
}

TEST_F(SyncEngineImplTest, ShouldEnableInvalidationsWhenStartedHandling) {
  EXPECT_CALL(mock_sync_invalidations_service_, HasListener)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_sync_invalidations_service_, GetFCMRegistrationToken)
      .WillRepeatedly(Return("fcm_token"));
  InitializeBackend(/*expect_success=*/true);
  backend_->StartHandlingInvalidations();
  fake_manager_->WaitForSyncThread();
  EXPECT_TRUE(fake_manager_->IsInvalidatorEnabled());
}

TEST_F(SyncEngineImplTest, ShouldEnableInvalidationsOnTokenUpdate) {
  EXPECT_CALL(mock_sync_invalidations_service_, GetFCMRegistrationToken)
      .WillRepeatedly(Return(std::nullopt));
  InitializeBackend(/*expect_success=*/true);
  fake_manager_->WaitForSyncThread();

  // Simulate listening for invalidations but since an FCM token hasn't been
  // obtained, the invalidator is still disabled.
  ON_CALL(mock_sync_invalidations_service_, HasListener)
      .WillByDefault(Return(true));
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

TEST_F(SyncEngineImplTest, ShouldLoadSyncDataUponInitialization) {
  SyncTransportDataPrefs transport_data_prefs(
      &pref_service_, signin::GaiaIdHash::FromGaiaId(kTestGaiaId));
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);
  transport_data_prefs.SetCurrentSyncingGaiaId(kTestGaiaId);

  InitializeBackend();

  EXPECT_EQ(kTestGaiaId, transport_data_prefs.GetCurrentSyncingGaiaId());
  EXPECT_EQ(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_EQ(kTestBirthday, transport_data_prefs.GetBirthday());
}

TEST_F(SyncEngineImplTest, ShouldNotifyOnNewInvalidatedDataTypes) {
  InitializeBackend();
  ConfigureDataTypes();

  // Use OnInvalidationStatusChanged() to verify that
  // OnNewInvalidatedDataTypes() is caled only once in the beginning, and all
  // the next invalidated data types updates should not notify.
  Sequence seq;
  EXPECT_CALL(mock_host_, OnNewInvalidatedDataTypes).InSequence(seq);
  EXPECT_CALL(mock_host_, OnInvalidationStatusChanged).InSequence(seq);

  SyncStatus sync_status;
  sync_status.invalidated_data_types.Put(BOOKMARKS);
  fake_manager_->NotifySyncStatusChanged(sync_status);
  fake_manager_->WaitForSyncThread();

  // Turn on notifications to trigger OnInvalidationStatusChanged().
  sync_status.notifications_enabled = true;
  fake_manager_->NotifySyncStatusChanged(sync_status);
  fake_manager_->WaitForSyncThread();

  // Removing an invalidated data type shouldn't invoke
  // OnNewInvalidatedDataTypes().
  sync_status.invalidated_data_types.Remove(BOOKMARKS);
  fake_manager_->NotifySyncStatusChanged(sync_status);
  fake_manager_->WaitForSyncThread();
}

TEST_F(SyncEngineImplTest, ShouldReturnWhetherNextPollTimePassed) {
  SyncTransportDataPrefs transport_data_prefs(
      &pref_service_, signin::GaiaIdHash::FromGaiaId(kTestGaiaId));
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(kTestBirthday);
  transport_data_prefs.SetCurrentSyncingGaiaId(kTestGaiaId);

  transport_data_prefs.SetLastPollTime(base::Time::Now() - base::Hours(5));
  transport_data_prefs.SetPollInterval(base::Hours(4));

  InitializeBackend();
  ConfigureDataTypes();

  EXPECT_TRUE(backend_->IsNextPollTimeInThePast());

  // Mimic a finished PERIODIC sync cycle.
  SyncCycleSnapshot snapshot(
      /*birthday=*/std::string(),
      /*bag_of_chips=*/std::string(), ModelNeutralState(), ProgressMarkerMap(),
      /*is_silenced=*/false,
      /*num_server_conflicts=*/0,
      /*notifications_enabled=*/true,
      /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      /*get_updates_origin=*/sync_pb::SyncEnums::PERIODIC,
      /*poll_interval=*/base::Hours(4),
      /*has_remaining_local_changes=*/false);
  fake_manager_->NotifySyncCycleCompleted(snapshot);
  fake_manager_->WaitForSyncThread();

  EXPECT_FALSE(backend_->IsNextPollTimeInThePast());
}

}  // namespace

}  // namespace syncer
