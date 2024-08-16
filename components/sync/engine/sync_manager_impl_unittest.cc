// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager_impl.h"

#include <cstddef>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_scheduler.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/data_type_test_util.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/fake_sync_scheduler.h"
#include "components/sync/test/test_engine_components_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"

using testing::_;
using testing::SaveArg;
using testing::Sequence;
using testing::StrictMock;

namespace syncer {

namespace {

class TestHttpPostProvider : public HttpPostProvider {
 public:
  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const GURL& url) override {}
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}
  bool MakeSynchronousPost(int* net_error_code,
                           int* http_status_code) override {
    return false;
  }
  int GetResponseContentLength() const override { return 0; }
  const char* GetResponseContent() const override { return ""; }
  const std::string GetResponseHeaderValue(
      const std::string& name) const override {
    return std::string();
  }
  void Abort() override {}

 private:
  ~TestHttpPostProvider() override = default;
};

class TestHttpPostProviderFactory : public HttpPostProviderFactory {
 public:
  ~TestHttpPostProviderFactory() override = default;
  scoped_refptr<HttpPostProvider> Create() override {
    return new TestHttpPostProvider();
  }
};

class SyncManagerObserverMock : public SyncManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSyncCycleCompleted,
              (const SyncCycleSnapshot&),
              (override));
  MOCK_METHOD(void, OnConnectionStatusChange, (ConnectionStatus), (override));
  MOCK_METHOD(void,
              OnActionableProtocolError,
              (const SyncProtocolError&),
              (override));
  MOCK_METHOD(void, OnMigrationRequested, (DataTypeSet), (override));
  MOCK_METHOD(void, OnProtocolEvent, (const ProtocolEvent&), (override));
  MOCK_METHOD(void, OnSyncStatusChanged, (const SyncStatus&), (override));
};

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD(void,
              OnPassphraseRequired,
              (const KeyDerivationParams&, const sync_pb::EncryptedData&),
              (override));
  MOCK_METHOD(void, OnPassphraseAccepted, (), (override));
  MOCK_METHOD(void, OnTrustedVaultKeyRequired, (), (override));
  MOCK_METHOD(void, OnTrustedVaultKeyAccepted, (), (override));
  MOCK_METHOD(void, OnEncryptedTypesChanged, (DataTypeSet, bool), (override));
  MOCK_METHOD(void,
              OnCryptographerStateChanged,
              (Cryptographer*, bool),
              (override));
  MOCK_METHOD(void,
              OnPassphraseTypeChanged,
              (PassphraseType, base::Time),
              (override));
};

class MockSyncScheduler : public FakeSyncScheduler {
 public:
  MockSyncScheduler() = default;
  ~MockSyncScheduler() override = default;
  MOCK_METHOD(void, Start, (SyncScheduler::Mode, base::Time), (override));
  MOCK_METHOD(void,
              ScheduleConfiguration,
              (sync_pb::SyncEnums::GetUpdatesOrigin origin,
               DataTypeSet types_to_download,
               base::OnceClosure ready_task),
              (override));
  MOCK_METHOD(void, SetHasPendingInvalidations, (DataType, bool), (override));
};

class ComponentsFactory : public TestEngineComponentsFactory {
 public:
  explicit ComponentsFactory(std::unique_ptr<SyncScheduler> scheduler_to_use)
      : scheduler_to_use_(std::move(scheduler_to_use)) {}
  ~ComponentsFactory() override = default;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* stop_handle,
      bool local_sync_backend_enabled) override {
    DCHECK(scheduler_to_use_);
    return std::move(scheduler_to_use_);
  }

 private:
  std::unique_ptr<SyncScheduler> scheduler_to_use_;
};

class SyncManagerImplTest : public testing::Test {
 protected:
  SyncManagerImplTest()
      : sync_manager_("Test sync manager",
                      network::TestNetworkConnectionTracker::GetInstance()) {}

  ~SyncManagerImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    extensions_activity_ = new ExtensionsActivity();

    sync_manager_.AddObserver(&manager_observer_);

    // Save raw pointers to the objects that won't be owned by the fixture.
    auto encryption_observer =
        std::make_unique<StrictMock<SyncEncryptionHandlerObserverMock>>();
    encryption_observer_ = encryption_observer.get();
    auto scheduler = std::make_unique<MockSyncScheduler>();
    scheduler_ = scheduler.get();

    // This should be the only method called by the Init() in the observer.
    EXPECT_CALL(manager_observer_, OnSyncStatusChanged).Times(2);

    SyncManager::InitArgs args;
    args.service_url = GURL("https://example.com/");
    args.post_factory = std::make_unique<TestHttpPostProviderFactory>();
    args.encryption_observer_proxy = std::move(encryption_observer);
    args.extensions_activity = extensions_activity_.get();
    args.cache_guid = "fake_cache_guid";
    args.enable_local_sync_backend = false;
    args.local_sync_backend_folder = temp_dir_.GetPath();
    args.engine_components_factory =
        std::make_unique<ComponentsFactory>(std::move(scheduler));
    args.encryption_handler = &encryption_handler_;
    args.cancelation_signal = &cancelation_signal_;
    args.poll_interval = base::Minutes(60);
    sync_manager_.Init(&args);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    sync_manager_.RemoveObserver(&manager_observer_);
    sync_manager_.ShutdownOnSyncThread();
    base::RunLoop().RunUntilIdle();
  }

  SyncManagerImpl* sync_manager() { return &sync_manager_; }
  MockSyncScheduler* scheduler() { return scheduler_; }
  SyncManagerObserverMock* manager_observer() { return &manager_observer_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;

  FakeSyncEncryptionHandler encryption_handler_;
  SyncManagerImpl sync_manager_;
  CancelationSignal cancelation_signal_;
  StrictMock<SyncManagerObserverMock> manager_observer_;
  // Owned by |sync_manager_|.
  raw_ptr<StrictMock<SyncEncryptionHandlerObserverMock>> encryption_observer_ =
      nullptr;
  raw_ptr<MockSyncScheduler, DanglingUntriaged> scheduler_ = nullptr;
};

// Test that the configuration params are properly created and sent to
// ScheduleConfigure. No callback should be invoked.
TEST_F(SyncManagerImplTest, BasicConfiguration) {
  const DataTypeSet types_to_download = {BOOKMARKS, PREFERENCES};
  base::MockOnceClosure ready_task;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE, _));
  EXPECT_CALL(*scheduler(),
              ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                    types_to_download, _));
  EXPECT_CALL(ready_task, Run).Times(0);

  sync_manager()->ConfigureSyncer(
      CONFIGURE_REASON_RECONFIGURATION, types_to_download,
      SyncManager::SyncFeatureState::ON, ready_task.Get());
}

TEST_F(SyncManagerImplTest, ShouldSetHasPendingInvalidations) {
  Sequence s1;
  EXPECT_CALL(*scheduler(),
              SetHasPendingInvalidations(BOOKMARKS, /*has_invalidation=*/true))
      .InSequence(s1);
  EXPECT_CALL(*scheduler(),
              SetHasPendingInvalidations(BOOKMARKS, /*has_invalidation=*/false))
      .InSequence(s1);
  SyncStatus status;
  EXPECT_CALL(*manager_observer(), OnSyncStatusChanged)
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&status));

  sync_manager()->SetHasPendingInvalidations(
      BOOKMARKS, /*has_pending_invalidations=*/true);
  EXPECT_EQ(status.invalidated_data_types.size(), 1u);
  EXPECT_TRUE(status.invalidated_data_types.Has(BOOKMARKS));

  sync_manager()->SetHasPendingInvalidations(
      BOOKMARKS, /*has_pending_invalidations=*/false);
  EXPECT_TRUE(status.invalidated_data_types.empty());
}

}  // namespace

}  // namespace syncer
