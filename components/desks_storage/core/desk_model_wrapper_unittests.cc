// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model_wrapper.h"

#include <stddef.h>

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "desk_model_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

using GetAllEntriesResult = DeskModel::GetAllEntriesResult;
using GetEntryByUuidResult = DeskModel::GetEntryByUuidResult;
using TestUuidId = base::StrongAlias<class TestUuidIdTag, int>;

constexpr char kUuidFormat[] = "1c186d5a-502e-49ce-9ee1-00000000000%d";

std::string MakeTestUuidString(TestUuidId uuid_id) {
  return base::StringPrintf(kUuidFormat, uuid_id.value());
}

std::string GetPolicyStringWithOneTemplate() {
  return "[{\"version\":1,\"uuid\":\"" + MakeTestUuidString(TestUuidId(5)) +
         "\",\"name\":\""
         "Admin Template 1"
         "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
         "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
         "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
         "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
         "\"url\":\"https://"
         "example.com\",\"title\":\"Example\"},{\"url\":\"https://"
         "example.com/"
         "2\",\"title\":\"Example2\"}],\"active_tab_index\":1,\"window_id\":0,"
         "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}}"
         "]";
}

// Search `entry_list` for `uuid_query` as a uuid and returns true if
// found, false if not.
bool FindUuidInUuidList(
    const std::string& uuid_query,
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        entry_list) {
  base::Uuid guid = base::Uuid::ParseCaseInsensitive(uuid_query);
  DCHECK(guid.is_valid());

  for (const ash::DeskTemplate* entry : entry_list) {
    if (entry->uuid() == guid)
      return true;
  }

  return false;
}

// Verifies that the status passed into it is kOk.
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status,
                               std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

void VerifyEntryAddedErrorHitMaximumLimit(
    DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit);
}

// Verifies that the status passed into it is kInvalidArgument/
void VerifyEntryAddedInvalidArgument(
    DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kInvalidArgument);
}

// Make test template with ID containing the index. Defaults to desk template
// type if a type is not specified.

std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    int index,
    ash::DeskTemplateType type) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, index)),
      ash::DeskTemplateSource::kUser, base::StringPrintf("desk_%d", index),
      base::Time::Now(), type);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

// Make test template with default restore data.
std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    const std::string& uuid,
    ash::DeskTemplateSource source,
    const std::string& name,
    const base::Time created_time) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      base::Uuid::ParseCaseInsensitive(uuid), source, name, created_time,
      ash::DeskTemplateType::kTemplate);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

// Make test save and recall desk with default restore data.
std::unique_ptr<ash::DeskTemplate> MakeTestSaveAndRecallDesk(
    const std::string& uuid,
    const std::string& name,
    const base::Time created_time) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      base::Uuid::ParseCaseInsensitive(uuid), ash::DeskTemplateSource::kUser,
      name, created_time, ash::DeskTemplateType::kSaveAndRecall);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

}  // namespace

class MockDeskModelObserver : public DeskModelObserver {
 public:
  MOCK_METHOD0(DeskModelLoaded, void());
  MOCK_METHOD1(EntriesAddedOrUpdatedRemotely,
               void(const std::vector<
                    raw_ptr<const ash::DeskTemplate, VectorExperimental>>&));
  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<base::Uuid>&));
};

// This test class only tests the overall wrapper desk model class. The
// correctness of the underlying desk model storages that
// `DeskModelWrapper` uses are tested in their own unittests.
class DeskModelWrapperTest : public testing::Test {
 public:
  DeskModelWrapperTest()
      : sample_desk_template_one_(
            MakeTestDeskTemplate(MakeTestUuidString(TestUuidId(1)),
                                 ash::DeskTemplateSource::kUser,
                                 "desk_01",
                                 base::Time::Now())),
        sample_desk_template_two_(
            MakeTestDeskTemplate(MakeTestUuidString(TestUuidId(2)),
                                 ash::DeskTemplateSource::kUser,
                                 "desk_02",
                                 base::Time::Now())),
        sample_save_and_recall_desk_one_(
            MakeTestSaveAndRecallDesk(MakeTestUuidString(TestUuidId(3)),
                                      "save_and_recall_desk_01",
                                      base::Time::Now())),
        sample_save_and_recall_desk_two_(
            MakeTestSaveAndRecallDesk(MakeTestUuidString(TestUuidId(4)),
                                      "save_and_recall_desk_02",
                                      base::Time::Now())),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")),
        data_manager_(std::unique_ptr<LocalDeskDataManager>()),
        store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  DeskModelWrapperTest(const DeskModelWrapperTest&) = delete;
  DeskModelWrapperTest& operator=(const DeskModelWrapperTest&) = delete;

  ~DeskModelWrapperTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    data_manager_ = std::make_unique<LocalDeskDataManager>(temp_dir_.GetPath(),
                                                           account_id_);
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
    model_wrapper_ = std::make_unique<DeskModelWrapper>(data_manager_.get());
    task_environment_.RunUntilIdle();
    testing::Test::SetUp();
  }

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        account_id_);
    bridge_->AddObserver(&mock_observer_);
  }

  void FinishInitialization() { base::RunLoop().RunUntilIdle(); }

  void InitializeBridge() {
    CreateBridge();
    FinishInitialization();
    model_wrapper_->SetDeskSyncBridge(bridge_.get());
  }

  void ShutdownBridge() {
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(&mock_observer_);
  }

  void RestartBridge() {
    ShutdownBridge();
    InitializeBridge();
  }

  void AddTwoTemplates() {
    base::RunLoop loop1;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_desk_template_one_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_desk_template_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void AddTwoSaveAndRecallDeskTemplates() {
    base::RunLoop loop1;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_one_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void AddSavedDeskToDeskModel(std::unique_ptr<ash::DeskTemplate> entry) {
    base::RunLoop loop;
    model_wrapper_->AddOrUpdateEntry(
        std::move(entry),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop.Quit();
            }));
    loop.Run();
  }

  void VerifyAllEntries(size_t expected_size, const std::string& trace_string) {
    SCOPED_TRACE(trace_string);

    task_environment_.RunUntilIdle();

    GetAllEntriesResult result = model_wrapper_->GetAllEntries();
    EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
    EXPECT_EQ(result.entries.size(), expected_size);
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_two_;
  std::unique_ptr<ash::DeskTemplate> sample_save_and_recall_desk_one_;
  std::unique_ptr<ash::DeskTemplate> sample_save_and_recall_desk_two_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
  std::unique_ptr<LocalDeskDataManager> data_manager_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  testing::NiceMock<MockDeskModelObserver> mock_observer_;
  std::unique_ptr<DeskSyncBridge> bridge_;
  std::unique_ptr<DeskModelWrapper> model_wrapper_;
};

TEST_F(DeskModelWrapperTest, CanAddDeskTemplateEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one desk template");

  // Verify that it's not desk template entry in the save and recall desk
  // storage.
  GetAllEntriesResult result = model_wrapper_->GetAllEntries();
  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 1ul);
  EXPECT_EQ(result.entries[0]->type(), ash::DeskTemplateType::kTemplate);

  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 1ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 0ul);
}

TEST_F(DeskModelWrapperTest, CanAddSaveAndRecallDeskEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  // Verify that it's not SaveAndRecall entry in the desk template storage.
  GetAllEntriesResult result = model_wrapper_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 1ul);
  EXPECT_EQ(result.entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);

  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 1ul);
}

TEST_F(DeskModelWrapperTest,
       ReturnsErrorWhenAddingTooManySaveAndRecallDeskEntry) {
  InitializeBridge();
  for (size_t index = 0;
       index < model_wrapper_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    AddSavedDeskToDeskModel(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall));
  }

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(
          model_wrapper_->GetMaxSaveAndRecallDeskEntryCount() + 1,
          ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedErrorHitMaximumLimit));
  task_environment_.RunUntilIdle();
}

TEST_F(DeskModelWrapperTest, CanGetAllEntries) {
  InitializeBridge();

  AddTwoTemplates();

  GetAllEntriesResult result = model_wrapper_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 2ul);
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(1)), result.entries));
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(2)), result.entries));

  // Sanity check for the search function.
  EXPECT_FALSE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(3)), result.entries));
}

TEST_F(DeskModelWrapperTest, GetAllEntriesIncludesPolicyValues) {
  InitializeBridge();

  AddTwoTemplates();
  AddTwoSaveAndRecallDeskTemplates();
  model_wrapper_->SetPolicyDeskTemplates(GetPolicyStringWithOneTemplate());

  GetAllEntriesResult result = model_wrapper_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 5ul);
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(1)), result.entries));
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(2)), result.entries));
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(3)), result.entries));
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(4)), result.entries));
  EXPECT_TRUE(
      FindUuidInUuidList(MakeTestUuidString(TestUuidId(5)), result.entries));
  // One of these templates should be from policy.
  EXPECT_EQ(base::ranges::count_if(result.entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            1l);

  model_wrapper_->SetPolicyDeskTemplates("");
}

TEST_F(DeskModelWrapperTest, CanDetectDuplicateEntryNames) {
  InitializeBridge();

  // Add desk template entry to desk model.
  AddSavedDeskToDeskModel(std::move(sample_desk_template_one_));
  // Add desk template entry with the duplicated name to desk model.
  auto dupe_template_uuid = base::StringPrintf(kUuidFormat, 6);
  AddSavedDeskToDeskModel(MakeTestDeskTemplate(dupe_template_uuid,
                                               ash::DeskTemplateSource::kUser,
                                               "desk_01", base::Time::Now()));
  // Add save and recall desk to desk model.
  AddSavedDeskToDeskModel(std::move(sample_save_and_recall_desk_one_));
  // Add save and recall entry with the duplicated name to desk model.
  AddSavedDeskToDeskModel(
      MakeTestSaveAndRecallDesk(base::StringPrintf(kUuidFormat, 7),
                                "save_and_recall_desk_01", base::Time::Now()));

  // Add save and recall entry with the duplicated name as a desk template to
  // desk model. This is to test that the two desk types don't share the same
  // namespace for the sake of duplication checks.
  auto dupe_second_save_and_recall_uuid = base::StringPrintf(kUuidFormat, 8);
  AddSavedDeskToDeskModel(MakeTestSaveAndRecallDesk(
      dupe_second_save_and_recall_uuid, "desk_01", base::Time::Now()));

  GetAllEntriesResult result = model_wrapper_->GetAllEntries();
  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 5ul);

  EXPECT_TRUE(model_wrapper_->FindOtherEntryWithName(
      u"desk_01", ash::DeskTemplateType::kTemplate,
      base::Uuid::ParseCaseInsensitive(dupe_template_uuid)));

  EXPECT_TRUE(model_wrapper_->FindOtherEntryWithName(
      u"save_and_recall_desk_01", ash::DeskTemplateType::kSaveAndRecall,
      base::Uuid::ParseCaseInsensitive(dupe_template_uuid)));

  EXPECT_FALSE(model_wrapper_->FindOtherEntryWithName(
      u"desk_01", ash::DeskTemplateType::kSaveAndRecall,
      base::Uuid::ParseCaseInsensitive(dupe_second_save_and_recall_uuid)));
}

TEST_F(DeskModelWrapperTest, CanDetectNoDuplicateEntryNames) {
  InitializeBridge();

  // Add desk template entry to desk model.
  AddSavedDeskToDeskModel(std::move(sample_desk_template_one_));

  // Add a second desk template entry to the desk model with a unique name.
  auto second_template_uuid = base::StringPrintf(kUuidFormat, 7);
  AddSavedDeskToDeskModel(MakeTestDeskTemplate(second_template_uuid,
                                               ash::DeskTemplateSource::kUser,
                                               "desk_02", base::Time::Now()));

  // Add save and recall desk to desk model.
  AddSavedDeskToDeskModel(std::move(sample_save_and_recall_desk_one_));
  // Add save and recall entry with the duplicated name to desk model.
  auto second_save_and_recall_uuid = base::StringPrintf(kUuidFormat, 7);
  AddSavedDeskToDeskModel(MakeTestSaveAndRecallDesk(second_save_and_recall_uuid,
                                                    "save_and_recall_desk_02",
                                                    base::Time::Now()));

  GetAllEntriesResult result = model_wrapper_->GetAllEntries();
  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 4ul);

  EXPECT_FALSE(model_wrapper_->FindOtherEntryWithName(
      u"desk_02", ash::DeskTemplateType::kTemplate,
      base::Uuid::ParseCaseInsensitive(second_template_uuid)));

  EXPECT_FALSE(model_wrapper_->FindOtherEntryWithName(
      u"save_and_recall_desk_02", ash::DeskTemplateType::kSaveAndRecall,
      base::Uuid::ParseCaseInsensitive(second_save_and_recall_uuid)));
}

TEST_F(DeskModelWrapperTest, CanGetEntryByUuid) {
  InitializeBridge();

  // Add desk template entry to desk model.
  AddSavedDeskToDeskModel(std::move(sample_desk_template_one_));

  // Add save and recall desk to desk model.
  AddSavedDeskToDeskModel(std::move(sample_save_and_recall_desk_one_));

  task_environment_.RunUntilIdle();

  // Find the desk template by its uuid.
  GetEntryByUuidResult result1 = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(1))));
  EXPECT_EQ(result1.status, DeskModel::GetEntryByUuidStatus::kOk);

  EXPECT_EQ(result1.entry->uuid(), base::Uuid::ParseCaseInsensitive(
                                       MakeTestUuidString(TestUuidId(1))));
  EXPECT_EQ(base::UTF16ToUTF8(result1.entry->template_name()), "desk_01");

  // Find the save and recall desk by its uuid.
  GetEntryByUuidResult result2 = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(3))));

  EXPECT_EQ(result2.status, DeskModel::GetEntryByUuidStatus::kOk);

  EXPECT_EQ(result2.entry->uuid(), base::Uuid::ParseCaseInsensitive(
                                       MakeTestUuidString(TestUuidId(3))));
  EXPECT_EQ(base::UTF16ToUTF8(result2.entry->template_name()),
            "save_and_recall_desk_01");
}

TEST_F(DeskModelWrapperTest, GetEntryByUuidShouldReturnAdminTemplate) {
  InitializeBridge();

  AddSavedDeskToDeskModel(std::move(sample_desk_template_one_));

  // Set admin template with UUID: TestUuidId(5).
  model_wrapper_->SetPolicyDeskTemplates(GetPolicyStringWithOneTemplate());

  // Check that the admin template is included as an entry.
  EXPECT_EQ(model_wrapper_->GetAllEntryUuids().size(), 2ul);

  GetEntryByUuidResult result = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(5))));
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kOk);
  EXPECT_EQ(result.entry->uuid(), base::Uuid::ParseCaseInsensitive(
                                      MakeTestUuidString(TestUuidId(5))));
  EXPECT_EQ(result.entry->source(), ash::DeskTemplateSource::kPolicy);
  EXPECT_EQ(base::UTF16ToUTF8(result.entry->template_name()),
            "Admin Template 1");
}

TEST_F(DeskModelWrapperTest, GetEntryByUuidReturnsNotFoundIfEntryDoesNotExist) {
  InitializeBridge();

  GetEntryByUuidResult result = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(1))));
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kNotFound);
}

TEST_F(DeskModelWrapperTest, CanUpdateEntry) {
  InitializeBridge();

  // Make a clone of a desk template and modify its name.
  auto modified_desk_template = sample_desk_template_one_->Clone();
  modified_desk_template->set_template_name(u"desk_01_mod");

  AddSavedDeskToDeskModel(std::move(sample_desk_template_one_));

  AddSavedDeskToDeskModel(std::move(modified_desk_template));

  // Make a clone of a save and recall desk and modify its name.
  auto modified_save_and_recall_desk =
      sample_save_and_recall_desk_one_->Clone();
  modified_save_and_recall_desk->set_template_name(
      u"save_and_recall_desk_01_mod");

  AddSavedDeskToDeskModel(std::move(sample_save_and_recall_desk_one_));

  AddSavedDeskToDeskModel(std::move(modified_save_and_recall_desk));

  task_environment_.RunUntilIdle();

  // Check that the entries are updated.
  GetEntryByUuidResult result1 = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(1))));
  EXPECT_EQ(result1.status, DeskModel::GetEntryByUuidStatus::kOk);
  EXPECT_EQ(result1.entry->uuid(), base::Uuid::ParseCaseInsensitive(
                                       MakeTestUuidString(TestUuidId(1))));
  EXPECT_EQ(result1.entry->template_name(),
            base::UTF8ToUTF16(std::string("desk_01_mod")));

  GetEntryByUuidResult result3 = model_wrapper_->GetEntryByUUID(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(3))));
  EXPECT_EQ(result3.status, DeskModel::GetEntryByUuidStatus::kOk);
  EXPECT_EQ(result3.entry->uuid(), base::Uuid::ParseCaseInsensitive(
                                       MakeTestUuidString(TestUuidId(3))));
  EXPECT_EQ(result3.entry->template_name(), u"save_and_recall_desk_01_mod");
}

TEST_F(DeskModelWrapperTest, CanDeleteDeskTemplateEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->DeleteEntry(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(1))),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Delete desk template");
}

TEST_F(DeskModelWrapperTest, CanDeleteSaveAndRecallDeskEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->DeleteEntry(
      base::Uuid::ParseCaseInsensitive(MakeTestUuidString(TestUuidId(3))),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Delete save and recall desk");
}

TEST_F(DeskModelWrapperTest, CanDeleteAllEntries) {
  InitializeBridge();

  AddTwoTemplates();
  AddTwoSaveAndRecallDeskTemplates();

  model_wrapper_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul,
                   "Delete all entries after adding two desk templates and two "
                   "save and recall desks");
}

TEST_F(DeskModelWrapperTest,
       GetEntryCountShouldIncludeBothUserAndAdminTemplates) {
  InitializeBridge();

  // Add two user templates.
  AddTwoTemplates();
  // Add two save and recall desks templates.
  AddTwoSaveAndRecallDeskTemplates();

  // Set one admin template.
  model_wrapper_->SetPolicyDeskTemplates(GetPolicyStringWithOneTemplate());

  // There should be 5 templates: 2 user templates + 1 admin template + 2 save
  // and recall desks.
  EXPECT_EQ(model_wrapper_->GetEntryCount(), 5ul);
  // MaxEntryCount should be 6 max save and recall desks + 6 max user templates
  // + 1 admin template.
  size_t max_entry_count = model_wrapper_->GetMaxDeskTemplateEntryCount() +
                           model_wrapper_->GetMaxSaveAndRecallDeskEntryCount();
  EXPECT_EQ(max_entry_count, 13ul);
}

TEST_F(DeskModelWrapperTest, GetMaxEntryCountShouldIncreaseWithAdminTemplates) {
  InitializeBridge();

  // Add two user templates.
  AddTwoTemplates();

  size_t max_entry_count = model_wrapper_->GetMaxDeskTemplateEntryCount() +
                           model_wrapper_->GetMaxSaveAndRecallDeskEntryCount();
  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(max_entry_count, 12ul);

  // Set one admin template.
  model_wrapper_->SetPolicyDeskTemplates(GetPolicyStringWithOneTemplate());
  size_t max_entry_count_with_admin_template =
      model_wrapper_->GetMaxDeskTemplateEntryCount() +
      model_wrapper_->GetMaxSaveAndRecallDeskEntryCount();
  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(max_entry_count_with_admin_template, 13ul);
  // Sanity check to make sure that save and recall desk max count isn't
  // affected by the admin template.
  EXPECT_EQ(model_wrapper_->GetMaxSaveAndRecallDeskEntryCount(), 6ul);
}

TEST_F(DeskModelWrapperTest, AddDeskTemplatesAndSaveAndRecallDeskEntries) {
  InitializeBridge();

  // Add two user templates.
  AddTwoTemplates();

  // Add two SaveAndRecall desks.
  AddTwoSaveAndRecallDeskTemplates();

  EXPECT_EQ(model_wrapper_->GetEntryCount(), 4ul);
  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 2ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 2ul);

  auto result = model_wrapper_->GetAllEntries();
  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);

  VerifyAllEntries(4ul,
                   "Add two desks templates and two saved and recall desks");
}

TEST_F(DeskModelWrapperTest, AddSaveAndRecallDeskEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");

  // Verify that it's not SaveAndRecall entry in the desk template cache.
  auto result = model_wrapper_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 1ul);
  EXPECT_EQ(result.entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);

  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 1ul);
}

TEST_F(DeskModelWrapperTest, CanAddMaxEntriesForBothTypes) {
  InitializeBridge();

  for (size_t index = 0u;
       index < model_wrapper_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    model_wrapper_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  for (size_t index = 0u;
       index < model_wrapper_->GetMaxDeskTemplateEntryCount(); ++index) {
    model_wrapper_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  VerifyAllEntries(
      12ul, "Added max number of save and recall desks and desk templates");
  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 6ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 6ul);
}

TEST_F(DeskModelWrapperTest,
       CanAddMaxEntriesDeskTemplatesAndStillAddEntryForSaveAndRecallDesks) {
  InitializeBridge();

  for (size_t index = 0u;
       index < model_wrapper_->GetMaxDeskTemplateEntryCount(); ++index) {
    model_wrapper_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1ul, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(7ul,
                   "Added one save and recall desk after capping "
                   "desk template entries");

  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 6ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 1ul);
}

TEST_F(DeskModelWrapperTest,
       CanAddMaxEntriesForSaveAndRecallDeskAndStillAddEntryForDeskTemplate) {
  InitializeBridge();

  for (size_t index = 0u;
       index < model_wrapper_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    model_wrapper_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1ul, ash::DeskTemplateType::kTemplate),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(7ul,
                   "Added one desk template after capping "
                   "save and recall desk entries");

  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 1ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 6ul);
}
TEST_F(DeskModelWrapperTest, AddUnknownDeskTypeShouldFail) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kUnknown),
      base::BindOnce(&VerifyEntryAddedInvalidArgument));
}
}  // namespace desks_storage
