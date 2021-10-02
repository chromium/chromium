// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/sync/engine/entity_data.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/model_type_store_test_util.h"
#include "components/sync/test/model/test_matchers.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using BrowserAppTab =
    sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_BrowserAppTab;
using BrowserAppWindow = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow;
using ChromeApp = sync_pb::WorkspaceDeskSpecifics_ChromeApp;
using Desk = sync_pb::WorkspaceDeskSpecifics_Desk;
using ProgressiveWebApp = sync_pb::WorkspaceDeskSpecifics_ProgressiveWebApp;
using WindowBound = sync_pb::WorkspaceDeskSpecifics_WindowBound;
using WindowState = sync_pb::WorkspaceDeskSpecifics_WindowState;
using WorkspaceDeskSpecifics_App = sync_pb::WorkspaceDeskSpecifics_App;

namespace {

using ash::DeskTemplate;
using sync_pb::ModelTypeState;
using sync_pb::WorkspaceDeskSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::HasEncryptionKeyName;
using syncer::InMemoryMetadataChangeList;
using syncer::MetadataBatchContains;
using syncer::MetadataChangeList;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelError;
using syncer::ModelTypeStore;
using syncer::ModelTypeStoreTestUtil;
using testing::_;
using testing::Return;
using testing::SizeIs;
using testing::StrEq;

constexpr char kTestPwaAppId[] = "test_pwa_app_id";
constexpr char kTestChromeAppId[] = "test_chrome_app_id";
constexpr char kUuidFormat[] = "9e186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kNameFormat[] = "template %d";
const base::GUID kTestUuid1 =
    base::GUID::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 1));
const base::GUID kTestUuid2 =
    base::GUID::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 2));

void FillExampleBrowserAppWindow(WorkspaceDeskSpecifics_App* app) {
  BrowserAppWindow* app_window =
      app->mutable_app()->mutable_browser_app_window();
  BrowserAppTab* tab1 = app_window->add_tabs();
  tab1->set_url("https://www.testdomain1.com/");

  BrowserAppTab* tab2 = app_window->add_tabs();
  tab2->set_url("https://www.testdomain2.com/");

  app_window->set_active_tab_index(1);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(110);
  window_bound->set_top(120);
  window_bound->set_width(1330);
  window_bound->set_height(1440);
  app->set_window_state(WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL);
  app->set_display_id(99887766l);
  app->set_z_index(133);
  app->set_window_id(1555);
}

void FillExampleProgressiveWebAppWindow(WorkspaceDeskSpecifics_App* app) {
  ProgressiveWebApp* app_window =
      app->mutable_app()->mutable_progress_web_app();
  app_window->set_app_id(kTestPwaAppId);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(210);
  window_bound->set_top(220);
  window_bound->set_width(2330);
  window_bound->set_height(2440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED);
  app->set_pre_minimized_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN);
  app->set_display_id(99887766l);
  app->set_z_index(233);
  app->set_window_id(2555);
}

void FillExampleChromeAppWindow(WorkspaceDeskSpecifics_App* app) {
  ChromeApp* app_window = app->mutable_app()->mutable_chrome_app();
  app_window->set_app_id(kTestChromeAppId);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(210);
  window_bound->set_top(220);
  window_bound->set_width(2330);
  window_bound->set_height(2440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED);
  app->set_display_id(99887766l);
  app->set_z_index(233);
  app->set_window_id(2555);
}

WorkspaceDeskSpecifics ExampleWorkspaceDeskSpecifics(
    const std::string uuid,
    const std::string template_name,
    base::Time created_time = base::Time::Now()) {
  WorkspaceDeskSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_name(template_name);
  specifics.set_created_time_usec(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  Desk* desk = specifics.mutable_desk();
  FillExampleBrowserAppWindow(desk->add_apps());
  FillExampleChromeAppWindow(desk->add_apps());
  FillExampleProgressiveWebAppWindow(desk->add_apps());
  return specifics;
}

WorkspaceDeskSpecifics CreateWorkspaceDeskSpecifics(
    int templateIndex,
    base::Time created_time = base::Time::Now()) {
  return ExampleWorkspaceDeskSpecifics(
      base::StringPrintf(kUuidFormat, templateIndex),
      base::StringPrintf(kNameFormat, templateIndex), created_time);
}

ModelTypeState StateWithEncryption(const std::string& encryption_key_name) {
  ModelTypeState state;
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

apps::mojom::AppPtr MakeApp(const char* app_id,
                            const char* name,
                            apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = name;
  return app;
}

class MockDeskModelObserver : public DeskModelObserver {
 public:
  MOCK_METHOD0(DeskModelLoaded, void());
  MOCK_METHOD1(EntriesAddedOrUpdatedRemotely,
               void(const std::vector<const DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<std::string>&));
  MOCK_METHOD1(EntriesAddedOrUpdatedLocally,
               void(const std::vector<const DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedLocally, void(const std::vector<std::string>&));
};

MATCHER_P(UuidIs, e, "") {
  return testing::ExplainMatchResult(e, arg->uuid(), result_listener);
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "Expected:\n"
                     << expected.SerializeAsString() << "Actual\n"
                     << arg.SerializeAsString() << "\n";
    return false;
  }
  return true;
}

class DeskSyncBridgeTest : public testing::Test {
 public:
  DeskSyncBridgeTest(const DeskSyncBridgeTest&) = delete;
  DeskSyncBridgeTest& operator=(const DeskSyncBridgeTest&) = delete;

 protected:
  DeskSyncBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")) {}

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        account_id_);
    bridge_->AddObserver(&mock_observer_);
  }

  void FinishInitialization() { base::RunLoop().RunUntilIdle(); }

  void InitializeBridge() {
    CreateBridge();
    FinishInitialization();
  }

  void DisableBridgeSync() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  void ShutdownBridge() {
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(&mock_observer_);
  }

  void RestartBridge() {
    ShutdownBridge();
    InitializeBridge();
  }

  void PopulateAppRegistryCache() {
    std::vector<apps::mojom::AppPtr> deltas;

    deltas.push_back(
        MakeApp(kTestPwaAppId, "Test PWA App", apps::mojom::AppType::kWeb));
    deltas.push_back(MakeApp(extension_misc::kChromeAppId, "Chrome Browser",
                             apps::mojom::AppType::kWeb));
    deltas.push_back(MakeApp(kTestChromeAppId, "Test Chrome App",
                             apps::mojom::AppType::kExtension));

    cache_->OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                   false /* should_notify_initialized */);

    cache_->SetAccountId(account_id_);

    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             cache_.get());
  }

  void SetUp() override { PopulateAppRegistryCache(); }

  void WriteToStoreWithMetadata(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list,
      ModelTypeState state) {
    std::unique_ptr<ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
    }
    batch->GetMetadataChangeList()->UpdateModelTypeState(state);
    CommitToStoreAndWait(std::move(batch));
  }

  void CommitToStoreAndWait(std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop, const absl::optional<ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  EntityData MakeEntityData(
      const WorkspaceDeskSpecifics& workspace_desk_specifics) {
    EntityData entity_data;

    *entity_data.specifics.mutable_workspace_desk() = workspace_desk_specifics;

    entity_data.name = workspace_desk_specifics.name();
    return entity_data;
  }

  EntityData MakeEntityData(const DeskTemplate& desk_template) {
    return MakeEntityData(bridge()->ToSyncProto(&desk_template));
  }

  // Helper method to reduce duplicated code between tests. Wraps the given
  // specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
  // returns an EntityChangeList containing them all. Order is maintained.
  EntityChangeList EntityAddList(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list) {
    EntityChangeList changes;
    for (const auto& specifics : specifics_list) {
      changes.push_back(
          EntityChange::CreateAdd(specifics.uuid(), MakeEntityData(specifics)));
    }
    return changes;
  }

  base::Time AdvanceAndGetTime(base::TimeDelta delta = base::Milliseconds(10)) {
    clock_.Advance(delta);
    return clock_.Now();
  }

  void AddTwoTemplates() {
    auto desk_template1 =
        DeskSyncBridge::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            kTestUuid1.AsLowercaseString(), "template 1", AdvanceAndGetTime()));
    auto desk_template2 =
        DeskSyncBridge::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            kTestUuid2.AsLowercaseString(), "template 2", AdvanceAndGetTime()));

    base::RunLoop loop1;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template1),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template2),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop2.Quit();
            }));
    loop2.Run();
  }

  MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  DeskSyncBridge* bridge() { return bridge_.get(); }

  MockDeskModelObserver* mock_observer() { return &mock_observer_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::SimpleTestClock clock_;

  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ModelTypeStore> store_;

  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<DeskSyncBridge> bridge_;

  testing::NiceMock<MockDeskModelObserver> mock_observer_;

  std::unique_ptr<apps::AppRegistryCache> cache_;

  AccountId account_id_;
};

TEST_F(DeskSyncBridgeTest, DeskTemplateConversionShouldBeLossless) {
  CreateBridge();

  WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
      kTestUuid1.AsLowercaseString(), "template 1");

  std::unique_ptr<DeskTemplate> desk_template =
      DeskSyncBridge::FromSyncProto(desk_proto);
  WorkspaceDeskSpecifics converted_desk_proto =
      bridge()->ToSyncProto(desk_template.get());

  EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
}

TEST_F(DeskSyncBridgeTest, IsBridgeReady) {
  CreateBridge();
  ASSERT_FALSE(bridge()->IsReady());

  FinishInitialization();
  ASSERT_TRUE(bridge()->IsReady());
}

TEST_F(DeskSyncBridgeTest, IsBridgeSyncing) {
  InitializeBridge();
  ASSERT_TRUE(bridge()->IsSyncing());

  DisableBridgeSync();
  ASSERT_FALSE(bridge()->IsSyncing());
}

TEST_F(DeskSyncBridgeTest, InitializationWithLocalDataAndMetadata) {
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  ModelTypeState state = StateWithEncryption("test_encryption_key");
  WriteToStoreWithMetadata({template1, template2}, state);
  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("test_encryption_key"),
                                /*entities=*/_)));

  InitializeBridge();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Verify both local specifics are loaded correctly.
  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template1.uuid())))
                .SerializeAsString(),
            template1.SerializeAsString());

  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, AddEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  auto specifics1 = ExampleWorkspaceDeskSpecifics(
      kTestUuid1.AsLowercaseString(), "template 1", AdvanceAndGetTime());
  auto specifics2 = ExampleWorkspaceDeskSpecifics(
      kTestUuid2.AsLowercaseString(), "template 2", AdvanceAndGetTime());

  base::RunLoop loop1;
  bridge()->AddOrUpdateEntry(
      DeskSyncBridge::FromSyncProto(specifics1),
      base::BindLambdaForTesting([&](DeskModel::AddOrUpdateEntryStatus status) {
        EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
        loop1.Quit();
      }));
  loop1.Run();

  base::RunLoop loop2;
  bridge()->AddOrUpdateEntry(
      DeskSyncBridge::FromSyncProto(specifics2),
      base::BindLambdaForTesting([&](DeskModel::AddOrUpdateEntryStatus status) {
        EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
        loop2.Quit();
      }));
  loop2.Run();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Verify the added desk template content.
  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(kTestUuid1))
                .SerializeAsString(),
            specifics1.SerializeAsString());

  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(kTestUuid2))
                .SerializeAsString(),
            specifics2.SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldSucceedWheSyncIsDisabled) {
  InitializeBridge();
  DisableBridgeSync();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "template 1", AdvanceAndGetTime()),
      base::BindLambdaForTesting([&](DeskModel::AddOrUpdateEntryStatus status) {
        EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldFailWhenBridgeIsNotReady) {
  // Only create sync bridge but do not allow it to finish initialization.
  CreateBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "template 1", AdvanceAndGetTime()),
      base::BindLambdaForTesting([&](DeskModel::AddOrUpdateEntryStatus status) {
        EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kFailure);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldSucceed) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  base::RunLoop loop;
  bridge()->GetEntryByUUID(
      kTestUuid1.AsLowercaseString(),
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kOk);
        EXPECT_TRUE(entry);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldFailWhenUuidIsNotFound) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  const std::string nonExistingUuid = base::StringPrintf(kUuidFormat, 5);

  base::RunLoop loop;
  bridge()->GetEntryByUUID(
      nonExistingUuid,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kNotFound);
        EXPECT_FALSE(entry);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldFailWhenUuidIsInvalid) {
  InitializeBridge();

  base::RunLoop loop;
  bridge()->GetEntryByUUID(
      "invalid uuid",
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kInvalidUuid);
        EXPECT_FALSE(entry);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, UpdateEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Update template 1
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "updated template 1", AdvanceAndGetTime()),
      base::BindLambdaForTesting([&](DeskModel::AddOrUpdateEntryStatus status) {
        EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
  // Template 1 should be updated.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid1)->template_name()),
      "updated template 1");

  // Template 2 should be unchanged.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid2)->template_name()),
      "template 2");
}

TEST_F(DeskSyncBridgeTest, DeleteEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Delete template 1.
  base::RunLoop loop;
  bridge()->DeleteEntry(
      kTestUuid1.AsLowercaseString(),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // We should have only 1 template.
  EXPECT_EQ(1ul, bridge()->GetAllEntryUuids().size());
  // Template 2 should be unchanged.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid2)->template_name()),
      "template 2");
}

TEST_F(DeskSyncBridgeTest, DeleteAllEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Delete all templates.
  base::RunLoop loop;
  bridge()->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // We should have no templates.
  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesEmpty) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithTwoAdditions) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  auto error =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 EntityAddList({template1, template2}));
  EXPECT_FALSE(error);

  // We should have two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithOneUpdate) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({template1, template2}));

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Now update template 1 with a new content.
  WorkspaceDeskSpecifics updated_template1 = CreateWorkspaceDeskSpecifics(1);
  updated_template1.set_name("updated template 1");

  EntityChangeList update_changes;
  update_changes.push_back(EntityChange::CreateUpdate(
      template1.uuid(), MakeEntityData(updated_template1)));

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(update_changes));
  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
  // Template 1 should be updated to new content.
  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template1.uuid())))
                .SerializeAsString(),
            updated_template1.SerializeAsString());
  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

// Tests that remote desk template can be correctly removed.
TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithOneDeletion) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({template1, template2}));

  // Verify that we have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Now delete template 1.
  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(template1.uuid()));

  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(delete_changes));

  // Verify that we only have template 2.
  EXPECT_EQ(1ul, bridge()->GetAllEntryUuids().size());
  EXPECT_EQ(bridge()
                ->ToSyncProto(bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesDeleteNonexistent) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);

  EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("no-such-uuid"));
  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, MergeSyncDataWithTwoEntries) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->MergeSyncData(std::move(metadata_change_list),
                          EntityAddList({template1, template2}));
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, MergeSyncDataUploadsLocalOnlyEntries) {
  InitializeBridge();

  // Seed two templates.
  // Seeded templates will be "template 1" and "template 2".
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Create server-side templates "template 2" and "template 3".
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(2);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(3);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));

  // MergeSyncData should upload the local-only template "template 1".
  EXPECT_CALL(*processor(), Put(StrEq(kTestUuid1.AsLowercaseString()), _, _))
      .Times(1);

  bridge()->MergeSyncData(std::move(metadata_change_list),
                          EntityAddList({template1, template2}));

  // Merged data should contain 3 templtes.
  EXPECT_EQ(3ul, bridge()->GetAllEntryUuids().size());
}

}  // namespace

}  // namespace desks_storage