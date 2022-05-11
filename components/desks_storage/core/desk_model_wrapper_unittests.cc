// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/desks_storage/core/desk_model_wrapper.h"

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/model_type_store_test_util.h"
#include "components/sync/test/model/test_matchers.h"
#include "desk_model_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

constexpr char kTemplateFileNameFormat[] = "%s.saveddesk";
constexpr char kUuidFormat[] = "1c186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kTemplateNameFormat[] = "desk_%d";
constexpr char kDeskOneTemplateDuplicateExpectedName[] = "desk_01 (1)";
constexpr char kDeskOneTemplateDuplicateTwoExpectedName[] = "desk_01 (2)";
const std::string kTestUuid1 = base::StringPrintf(kUuidFormat, 1);
const std::string kTestUuid2 = base::StringPrintf(kUuidFormat, 2);
const std::string kTestUuid3 = base::StringPrintf(kUuidFormat, 3);
const std::string kTestUuid4 = base::StringPrintf(kUuidFormat, 4);
const std::string kTestUuid5 = base::StringPrintf(kUuidFormat, 5);

const std::string kTestFileName1 =
    base::StringPrintf(kTemplateFileNameFormat, kTestUuid1.c_str());
const std::string kPolicyWithOneTemplate =
    "[{\"version\":1,\"uuid\":\"" + kTestUuid5 +
    "\",\"name\":\""
    "Admin Template 1"
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"https://example.com\",\"title\":\"Example\"},{\"url\":\"https://"
    "example.com/"
    "2\",\"title\":\"Example2\"}],\"active_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}}]";

// Search |entry_list| for |entry_query| as a uuid and returns true if
// found, false if not.
bool FindUuidInUuidList(
    const std::string& uuid_query,
    const std::vector<const ash::DeskTemplate*>& entry_list) {
  base::GUID guid = base::GUID::ParseCaseInsensitive(uuid_query);
  DCHECK(guid.is_valid());

  for (auto* entry : entry_list) {
    if (entry->uuid() == guid)
      return true;
  }

  return false;
}

// Takes in a vector of DeskTemplate pointers and a uuid, returns a pointer to
// the DeskTemplate with matching uuid if found in vector, nullptr if not.
const ash::DeskTemplate* FindEntryInEntryList(
    const std::string& uuid_string,
    const std::vector<const ash::DeskTemplate*>& entries) {
  base::GUID uuid = base::GUID::ParseLowercase(uuid_string);
  auto found_entry = std::find_if(entries.begin(), entries.end(),
                                  [&uuid](const ash::DeskTemplate* entry) {
                                    return uuid == entry->uuid();
                                  });

  return found_entry != entries.end() ? *found_entry : nullptr;
}

// Verifies that the status passed into it is kOk
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

// Make test template with ID containing the index. Defaults to desk template
// type if a type is not specified.

std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    int index,
    ash::DeskTemplateType type) {
  const std::string template_uuid = base::StringPrintf(kUuidFormat, index);
  const std::string template_name =
      base::StringPrintf(kTemplateNameFormat, index);
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          template_uuid, ash::DeskTemplateSource::kUser, template_name,
          base::Time::Now(), type);
  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>());
  return desk_template;
}

// Make test template with default restore data.
std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    const std::string& uuid,
    ash::DeskTemplateSource source,
    const std::string& name,
    const base::Time created_time) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      uuid, source, name, created_time, ash::DeskTemplateType::kTemplate);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

// Make test save and recall desk with default restore data.
std::unique_ptr<ash::DeskTemplate> MakeTestSaveAndRecallDesk(
    const std::string& uuid,
    const std::string& name,
    const base::Time created_time) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, name, created_time,
      ash::DeskTemplateType::kSaveAndRecall);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

}  // namespace

class MockDeskModelObserver : public DeskModelObserver {
 public:
  MOCK_METHOD0(DeskModelLoaded, void());
  MOCK_METHOD1(EntriesAddedOrUpdatedRemotely,
               void(const std::vector<const ash::DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<std::string>&));
  MOCK_METHOD1(EntriesAddedOrUpdatedLocally,
               void(const std::vector<const ash::DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedLocally, void(const std::vector<std::string>&));
};

// This test class only tests the overall wrapper desk model class. The
// correctness of the underlying desk model storages that
// `DeskModelWrapper` uses are tested in their own unittests.
class DeskModelWrapperTest : public testing::Test {
 public:
  DeskModelWrapperTest()
      : sample_desk_template_one_(
            MakeTestDeskTemplate(kTestUuid1,
                                 ash::DeskTemplateSource::kUser,
                                 "desk_01",
                                 base::Time::Now())),
        sample_desk_template_two_(
            MakeTestDeskTemplate(kTestUuid2,
                                 ash::DeskTemplateSource::kUser,
                                 "desk_02",
                                 base::Time::Now())),
        sample_save_and_recall_desk_one_(
            MakeTestSaveAndRecallDesk(kTestUuid3,
                                      "save_and_recall_desk_01",
                                      base::Time::Now())),
        sample_save_and_recall_desk_two_(
            MakeTestSaveAndRecallDesk(kTestUuid4,
                                      "save_and_recall_desk_02",
                                      base::Time::Now())),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")),
        data_manager_(std::unique_ptr<LocalDeskDataManager>()),
        store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  DeskModelWrapperTest(const DeskModelWrapperTest&) = delete;
  DeskModelWrapperTest& operator=(const DeskModelWrapperTest&) = delete;

  ~DeskModelWrapperTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    data_manager_ = std::make_unique<LocalDeskDataManager>(temp_dir_.GetPath(),
                                                           account_id_);
    data_manager_->SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(false);
    desk_template_util::PopulateAppRegistryCache(account_id_, cache_.get());
    model_wrapper_ = std::make_unique<DeskModelWrapper>(data_manager_.get());
    task_environment_.RunUntilIdle();
    testing::Test::SetUp();
  }

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
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
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_desk_template_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
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
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    model_wrapper_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void VerifyAllEntries(size_t expected_size, const std::string& trace_string) {
    SCOPED_TRACE(trace_string);
    base::RunLoop loop;

    model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
        [&](DeskModel::GetAllEntriesStatus status,
            const std::vector<const ash::DeskTemplate*>& entries) {
          EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
          EXPECT_EQ(entries.size(), expected_size);
          loop.Quit();
        }));

    loop.Run();
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
  std::unique_ptr<syncer::ModelTypeStore> store_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<DeskSyncBridge> bridge_;
  testing::NiceMock<MockDeskModelObserver> mock_observer_;
  std::unique_ptr<DeskModelWrapper> model_wrapper_;
};

TEST_F(DeskModelWrapperTest, CanAddDeskTemplateEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one desk template");
  base::RunLoop loop;
  // Verify that it's not desk template entry in the save and recall desk
  // storage.
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 1ul);
        EXPECT_EQ(entries[0]->type(), ash::DeskTemplateType::kTemplate);
        loop.Quit();
      }));
  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 1ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 0ul);
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanAddSaveAndRecallDeskEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  base::RunLoop loop;
  // Verify that it's not SaveAndRecall entry in the desk template storage.
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 1ul);
        EXPECT_EQ(entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);
        loop.Quit();
      }));
  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 1ul);
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanGetAllEntries) {
  InitializeBridge();

  AddTwoTemplates();
  base::RunLoop loop;
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 2ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(kTestUuid3, entries));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskModelWrapperTest, GetAllEntriesIncludesPolicyValues) {
  InitializeBridge();

  AddTwoTemplates();
  AddTwoSaveAndRecallDeskTemplates();
  model_wrapper_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  base::RunLoop loop;
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 5ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid3, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid4, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid5, entries));
        // One of these templates should be from policy.
        EXPECT_EQ(
            base::ranges::count_if(entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            1l);

        loop.Quit();
      }));
  loop.Run();

  model_wrapper_->SetPolicyDeskTemplates("");
}

TEST_F(DeskModelWrapperTest, CanMarkDuplicateEntryNames) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));
  auto dupe_template_uuid = base::StringPrintf(kUuidFormat, 6);
  auto dupe_desk_template =
      MakeTestDeskTemplate(dupe_template_uuid, ash::DeskTemplateSource::kUser,
                           "desk_01", base::Time::Now());
  model_wrapper_->AddOrUpdateEntry(std::move(dupe_desk_template),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  auto second_dupe_template_uuid = base::StringPrintf(kUuidFormat, 7);
  auto second_dupe_desk_template = MakeTestDeskTemplate(
      second_dupe_template_uuid, ash::DeskTemplateSource::kUser, "desk_01",
      base::Time::Now());
  model_wrapper_->AddOrUpdateEntry(std::move(second_dupe_desk_template),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 3ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(dupe_template_uuid, entries));
        EXPECT_TRUE(FindUuidInUuidList(second_dupe_template_uuid, entries));
        const ash::DeskTemplate* duplicate_one =
            FindEntryInEntryList(dupe_template_uuid, entries);
        EXPECT_NE(duplicate_one, nullptr);
        EXPECT_EQ(base::UTF16ToUTF8(duplicate_one->template_name()),
                  kDeskOneTemplateDuplicateExpectedName);

        const ash::DeskTemplate* duplicate_two =
            FindEntryInEntryList(second_dupe_template_uuid, entries);
        EXPECT_NE(duplicate_two, nullptr);
        EXPECT_EQ(base::UTF16ToUTF8(duplicate_two->template_name()),
                  kDeskOneTemplateDuplicateTwoExpectedName);

        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanGetDeskTemplateEntryByUuid) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kOk);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(base::UTF16ToUTF8(entry->template_name()), "desk_01");
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(DeskModelWrapperTest, CanGetSaveAndRecallEntryByUuid) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->GetEntryByUUID(
      kTestUuid3,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kOk);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid3));
        EXPECT_EQ(base::UTF16ToUTF8(entry->template_name()),
                  "save_and_recall_desk_01");
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(DeskModelWrapperTest, GetEntryByUuidShouldReturnAdminTemplate) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  // Set admin template with UUID: kTestUuid5.
  model_wrapper_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  model_wrapper_->GetEntryByUUID(
      kTestUuid5,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kOk);
        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid5));
        EXPECT_EQ(entry->source(), ash::DeskTemplateSource::kPolicy);
        EXPECT_EQ(base::UTF16ToUTF8(entry->template_name()),
                  "Admin Template 1");
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(DeskModelWrapperTest, GetEntryByUuidReturnsNotFoundIfEntryDoesNotExist) {
  InitializeBridge();

  base::RunLoop loop;

  model_wrapper_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kNotFound);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanUpdateEntry) {
  InitializeBridge();

  auto modified_desk_template = sample_desk_template_one_->Clone();
  modified_desk_template->set_template_name(u"desk_01_mod");

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->AddOrUpdateEntry(std::move(modified_desk_template),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  model_wrapper_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(status, DeskModel::GetEntryByUuidStatus::kOk);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(entry->template_name(),
                  base::UTF8ToUTF16(std::string("desk_01_mod")));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanDeleteDeskTemplateEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                   base::BindOnce(&VerifyEntryAddedCorrectly));

  model_wrapper_->DeleteEntry(
      kTestUuid1,
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
      kTestUuid3,
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
  model_wrapper_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  // There should be 5 templates: 2 user templates + 1 admin template + 2 save
  // and recall desks.
  EXPECT_EQ(model_wrapper_->GetEntryCount(), 5ul);
}

TEST_F(DeskModelWrapperTest, GetMaxEntryCountShouldIncreaseWithAdminTemplates) {
  InitializeBridge();

  // Add two user templates.
  AddTwoTemplates();

  std::size_t max_entry_count = model_wrapper_->GetMaxDeskTemplateEntryCount();

  // Set one admin template.
  model_wrapper_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(model_wrapper_->GetMaxDeskTemplateEntryCount(),
            max_entry_count + 1ul);
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

  base::RunLoop loop;
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        loop.Quit();
      }));

  loop.Run();
  VerifyAllEntries(4ul,
                   "Add two desks templates and two saved and recall desks");
}

TEST_F(DeskModelWrapperTest, AddSaveAndRecallDeskEntry) {
  InitializeBridge();

  model_wrapper_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  base::RunLoop loop;
  // Verify that it's not SaveAndRecall entry in the desk template cache.
  model_wrapper_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 1ul);
        EXPECT_EQ(entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);
        loop.Quit();
      }));
  EXPECT_EQ(model_wrapper_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(model_wrapper_->GetSaveAndRecallDeskEntryCount(), 1ul);
  loop.Run();
}

TEST_F(DeskModelWrapperTest, CanAddMaxEntriesForBothTypes) {
  InitializeBridge();

  for (std::size_t index = 0u;
       index < model_wrapper_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    model_wrapper_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  for (std::size_t index = 0u;
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

  for (std::size_t index = 0u;
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

  for (std::size_t index = 0u;
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

}  // namespace desks_storage
