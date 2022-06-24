// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

constexpr char kJunkData[] = "This is not valid template data.";
constexpr char kTemplateFileNameFormat[] = "%s.template";
constexpr char kUuidFormat[] = "1c186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kSaveAndRecallDeskUuidFormat[] =
    "1c186d5a-502e-49ce-9ee1-0000000000%d";
constexpr char kTemplateNameFormat[] = "desk_%d";
constexpr uint32_t kThreadSafeIterations = 1000;

const base::FilePath kInvalidFilePath = base::FilePath("?");
const std::string kTestUuid1 = base::StringPrintf(kUuidFormat, 1);
const std::string kTestUuid2 = base::StringPrintf(kUuidFormat, 2);
const std::string kTestUuid3 = base::StringPrintf(kUuidFormat, 3);
const std::string kTestUuid4 = base::StringPrintf(kUuidFormat, 4);
const std::string kTestUuid5 = base::StringPrintf(kUuidFormat, 5);
const std::string kTestUuid6 = base::StringPrintf(kUuidFormat, 6);
const std::string kTestUuid7 = base::StringPrintf(kUuidFormat, 7);
const std::string kTestUuid8 = base::StringPrintf(kUuidFormat, 8);
const std::string kTestUuid9 = base::StringPrintf(kUuidFormat, 9);
const std::string kTestSaveAndRecallDeskUuid1 =
    base::StringPrintf(kSaveAndRecallDeskUuidFormat, 10);
const std::string kTestSaveAndRecallDeskUuid2 =
    base::StringPrintf(kSaveAndRecallDeskUuidFormat, 11);
const std::string kTestSaveAndRecallDeskUuid3 =
    base::StringPrintf(kSaveAndRecallDeskUuidFormat, 12);

const base::Time kTestTime1 = base::Time();
const std::string kTestFileName1 =
    base::StringPrintf(kTemplateFileNameFormat, kTestUuid1.c_str());
const std::string kPolicyWithOneTemplate =
    "[{\"version\":1,\"uuid\":\"" + kTestUuid9 +
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

// Verifies that the status passed into it is kOk
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

// Verifies that the status passed into it is kOk
void VerifyEntryDeletedCorrectly(DeskModel::DeleteEntryStatus status) {
  EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
}

// Verifies that the status passed into it is kFailure
void VerifyEntryAddedFailure(DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kFailure);
}

void VerifyEntryAddedErrorHitMaximumLimit(
    DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit);
}

void WriteJunkData(const base::FilePath& temp_dir) {
  base::FilePath full_path = temp_dir.Append(kTestFileName1);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EXPECT_TRUE(base::WriteFile(full_path, kJunkData));
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

// TODO(crbug:1320940): Clean up tests to move on from std::string.
class LocalDeskDataManagerTest : public testing::Test {
 public:
  LocalDeskDataManagerTest()
      : sample_desk_template_one_(
            MakeTestDeskTemplate(kTestUuid1,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 kTestTime1)),
        sample_desk_template_one_duplicate_(
            MakeTestDeskTemplate(kTestUuid5,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 base::Time::Now())),
        sample_desk_template_one_duplicate_two_(
            MakeTestDeskTemplate(kTestUuid6,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_(
            MakeTestDeskTemplate(kTestUuid7,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_two_(
            MakeTestDeskTemplate(kTestUuid8,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_three_(
            MakeTestDeskTemplate(kTestUuid9,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        sample_desk_template_two_(
            MakeTestDeskTemplate(kTestUuid2,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_02"),
                                 base::Time::Now())),
        sample_desk_template_three_(
            MakeTestDeskTemplate(kTestUuid3,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_03"),
                                 base::Time::Now())),
        sample_save_and_recall_desk_one_(
            MakeTestSaveAndRecallDesk(kTestSaveAndRecallDeskUuid1,
                                      "save_and_recall_desk_01",
                                      kTestTime1)),
        sample_save_and_recall_desk_two_(
            MakeTestSaveAndRecallDesk(kTestSaveAndRecallDeskUuid2,
                                      "save_and_recall_desk_02",
                                      base::Time::Now())),
        sample_save_and_recall_desk_three_(
            MakeTestSaveAndRecallDesk(kTestSaveAndRecallDeskUuid3,
                                      "save_and_recall_desk_03",
                                      base::Time::Now())),
        modified_sample_desk_template_one_(
            MakeTestDeskTemplate(kTestUuid1,
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01_mod"),
                                 kTestTime1)),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")),
        data_manager_(std::unique_ptr<LocalDeskDataManager>()) {}

  LocalDeskDataManagerTest(const LocalDeskDataManagerTest&) = delete;
  LocalDeskDataManagerTest& operator=(const LocalDeskDataManagerTest&) = delete;

  ~LocalDeskDataManagerTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    data_manager_ = std::make_unique<LocalDeskDataManager>(temp_dir_.GetPath(),
                                                           account_id_);
    data_manager_->SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(false);
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
    task_environment_.RunUntilIdle();
    testing::Test::SetUp();
  }

  void AddTwoTemplates() {
    base::RunLoop loop1;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_desk_template_one_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_desk_template_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void AddTwoSaveAndRecallDeskTemplates() {
    base::RunLoop loop1;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_one_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void VerifyAllEntries(size_t expected_size, const std::string& trace_string) {
    SCOPED_TRACE(trace_string);
    base::RunLoop loop;

    data_manager_->GetAllEntries(base::BindLambdaForTesting(
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
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_duplicate_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_duplicate_two_;
  std::unique_ptr<ash::DeskTemplate> duplicate_pattern_matching_named_desk_;
  std::unique_ptr<ash::DeskTemplate> duplicate_pattern_matching_named_desk_two_;
  std::unique_ptr<ash::DeskTemplate>
      duplicate_pattern_matching_named_desk_three_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_two_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_three_;
  std::unique_ptr<ash::DeskTemplate> sample_save_and_recall_desk_one_;
  std::unique_ptr<ash::DeskTemplate> sample_save_and_recall_desk_two_;
  std::unique_ptr<ash::DeskTemplate> sample_save_and_recall_desk_three_;
  std::unique_ptr<ash::DeskTemplate> modified_sample_desk_template_one_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
  std::unique_ptr<LocalDeskDataManager> data_manager_;
};

// TODO(crbug/1320949): Pass a callback to VerifyEntryAdded.
TEST_F(LocalDeskDataManagerTest, CanAddEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, ReturnsErrorWhenAddingTooManyEntry) {
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxDeskTemplateEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(data_manager_->GetMaxDeskTemplateEntryCount() + 1,
                           ash::DeskTemplateType::kTemplate),
      base::BindOnce(&VerifyEntryAddedErrorHitMaximumLimit));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest,
       ReturnsErrorWhenAddingTooManySaveAndRecallDeskEntry) {
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(
          data_manager_->GetMaxSaveAndRecallDeskEntryCount() + 1,
          ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedErrorHitMaximumLimit));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanGetAllEntries) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 3ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid3, entries));

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(kTestUuid4, entries));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, GetAllEntriesIncludesPolicyValues) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 4ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid3, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid9, entries));

        // One of these templates should be from policy.
        EXPECT_EQ(
            base::ranges::count_if(entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            1l);

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(kTestUuid4, entries));
        loop.Quit();
      }));
  loop.Run();

  data_manager_->SetPolicyDeskTemplates("");
}

TEST_F(LocalDeskDataManagerTest, CanDetectDuplicateEntryNames) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(
      std::move(sample_desk_template_one_duplicate_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(
      std::move(sample_desk_template_one_duplicate_two_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  EXPECT_TRUE(data_manager_->FindOtherEntryWithName(
      base::UTF8ToUTF16(std::string("desk_01")),
      ash::DeskTemplateType::kTemplate,
      base::GUID::ParseCaseInsensitive(kTestUuid1)));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanDetectNoDuplicateEntryNames) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  EXPECT_FALSE(data_manager_->FindOtherEntryWithName(
      base::UTF8ToUTF16(std::string("desk_01")),
      ash::DeskTemplateType::kTemplate,
      base::GUID::ParseCaseInsensitive(kTestUuid1)));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanGetEntryByUuid) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(entry->template_name(),
                  base::UTF8ToUTF16(std::string("desk_01")));
        EXPECT_EQ(entry->created_time(), kTestTime1);
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, GetEntryByUuidShouldReturnAdminTemplate) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  // Set admin template with UUID: kTestUuid9.
  data_manager_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  data_manager_->GetEntryByUUID(
      kTestUuid9,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);
        EXPECT_EQ(base::GUID::ParseCaseInsensitive(kTestUuid9), entry->uuid());
        EXPECT_EQ(ash::DeskTemplateSource::kPolicy, entry->source());
        EXPECT_EQ(base::UTF8ToUTF16(std::string("Admin Template 1")),
                  entry->template_name());
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsNotFoundIfEntryDoesNotExist) {
  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, DeskTemplateIsIgnoredIfItHasBadData) {
  auto task_runner = task_environment_.GetMainThreadTaskRunner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&WriteJunkData, temp_dir_.GetPath()));

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsFailureIfDeskManagerHasInvalidPath) {
  data_manager_ =
      std::make_unique<LocalDeskDataManager>(kInvalidFilePath, account_id_);
  task_environment_.RunUntilIdle();

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kFailure, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanUpdateEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(modified_sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(entry->template_name(),
                  base::UTF8ToUTF16(std::string("desk_01_mod")));
        EXPECT_EQ(entry->created_time(), kTestTime1);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanDeleteEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->DeleteEntry(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));
  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 0ul);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanDeleteAllEntries) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;

  data_manager_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));
  task_environment_.RunUntilIdle();

  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 0ul);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryCountShouldIncludeBothUserAndAdminTemplates) {
  // Add two user templates.
  AddTwoTemplates();

  // Set one admin template.
  data_manager_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  // There should be 3 templates: 2 user templates + 1 admin template.
  EXPECT_EQ(3ul, data_manager_->GetEntryCount());
}

TEST_F(LocalDeskDataManagerTest,
       GetMaxEntryCountShouldIncreaseWithAdminTemplates) {
  // Add two user templates.
  AddTwoTemplates();

  std::size_t max_entry_count = data_manager_->GetMaxEntryCount();

  // Set one admin template.
  data_manager_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(max_entry_count + 1ul, data_manager_->GetMaxEntryCount());
}

TEST_F(LocalDeskDataManagerTest, AddDeskTemplatesAndSaveAndRecallDeskEntries) {
  // Add two user templates.
  AddTwoTemplates();

  // Add two SaveAndRecall desks.
  AddTwoSaveAndRecallDeskTemplates();

  EXPECT_EQ(data_manager_->GetEntryCount(), 4ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 2ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 2ul);

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        loop.Quit();
      }));

  loop.Run();
  VerifyAllEntries(4ul,
                   "Add two desks templates and two saved and recall desks");
}

TEST_F(LocalDeskDataManagerTest, AddSaveAndRecallDeskEntry) {
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  base::RunLoop loop;
  // Verify that it's not SaveAndRecall entry in the desk template cache.
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<const ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 1ul);
        EXPECT_EQ(entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);
        loop.Quit();
      }));
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 1ul);
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanGetSaveAndRecallDeskEntryByUuid) {
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->GetEntryByUUID(
      kTestSaveAndRecallDeskUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

        EXPECT_EQ(base::GUID::ParseCaseInsensitive(kTestSaveAndRecallDeskUuid1),
                  entry->uuid());
        EXPECT_EQ(u"save_and_recall_desk_01", entry->template_name());
        EXPECT_EQ(kTestTime1, entry->created_time());
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanDeleteSaveAndRecallDeskEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 1ul);
  data_manager_->DeleteEntry(
      kTestSaveAndRecallDeskUuid1,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Deleted one save and recall desk");
}

TEST_F(LocalDeskDataManagerTest, CanAddMaxEntriesForBothTypes) {
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxDeskTemplateEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  VerifyAllEntries(
      12ul, "Added max number of save and recall desks and desk templates");
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 6ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 6ul);
}

TEST_F(LocalDeskDataManagerTest, CanDeleteAllEntriesOfBothTypes) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(6ul,
                   "Added a mix of save and recall desks and desk templates");

  data_manager_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Deleted all entries");
  EXPECT_EQ(0ul, data_manager_->GetEntryCount());
}

TEST_F(LocalDeskDataManagerTest,
       CanAddMaxEntriesDeskTemplatesAndStillAddEntryForSaveAndRecallDesks) {
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxDeskTemplateEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1ul, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(7ul,
                   "Added one save and recall desk after capping "
                   "desk template entries");

  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 6ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 1ul);
}

TEST_F(LocalDeskDataManagerTest,
       CanAddMaxEntriesForSaveAndRecallDeskAndStillAddEntryForDeskTemplate) {
  for (std::size_t index = 0u;
       index < data_manager_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1ul, ash::DeskTemplateType::kTemplate),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(7ul,
                   "Added one desk template after capping "
                   "save and recall desk entries");

  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 1ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 6ul);
}

TEST_F(LocalDeskDataManagerTest, RollbackUpdateTemplatesOnFileWriteFailure) {
  // Add two user templates.
  for (std::size_t index = 0u; index < 2u; ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  EXPECT_EQ(data_manager_->GetEntryCount(), 2ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 2ul);
  task_environment_.RunUntilIdle();

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER);
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1ul, ash::DeskTemplateType::kTemplate),
      base::BindOnce(&VerifyEntryAddedFailure));

  VerifyAllEntries(2ul,
                   "Updated one desk template failed to write to file system");

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);
}

TEST_F(LocalDeskDataManagerTest, RollbackAddTemplatesOnFileWriteFailure) {
  // Add two user templates.
  for (std::size_t index = 0u; index < 2u; ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  EXPECT_EQ(data_manager_->GetEntryCount(), 2ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 2ul);
  task_environment_.RunUntilIdle();

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER);
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(3ul, ash::DeskTemplateType::kTemplate),
      base::BindOnce(&VerifyEntryAddedFailure));
  task_environment_.RunUntilIdle();

  VerifyAllEntries(2ul, "Add one desk template failed to write to file system");

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);
}

TEST_F(LocalDeskDataManagerTest, RollbackDeleteTemplatesOnFileDeleteFailure) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  EXPECT_EQ(data_manager_->GetEntryCount(), 1ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 1ul);
  task_environment_.RunUntilIdle();
  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER);
  data_manager_->DeleteEntry(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kFailure);
      }));
  task_environment_.RunUntilIdle();

  VerifyAllEntries(1ul, "Delete desk template failed to delete on file system");

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);
}

TEST_F(LocalDeskDataManagerTest,
       RollbackDeleteAllTemplatesOnFileDeleteFailure) {
  // Add four user templates.
  for (std::size_t index = 0u; index < 4u; ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  EXPECT_EQ(data_manager_->GetEntryCount(), 4ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 4ul);
  task_environment_.RunUntilIdle();
  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER);
  data_manager_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kFailure);
      }));
  task_environment_.RunUntilIdle();

  VerifyAllEntries(4ul,
                   "Delete all desk template failed to delete on file system");

  base::SetPosixFilePermissions(temp_dir_.GetPath(),
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);
}

// Note: To fully utilize this test build and run it in a tsan build.
// Instructions to do so can be found at:
// //docs/website/site/developers/testing/threadsanitizer-tsan-v2/index.md
// Otherwise the tsan tryjob should catch this test failing in CQ.
TEST_F(LocalDeskDataManagerTest, StressTestModifyingEntriesForThreadSafety) {
  for (uint32_t iteration = 0; iteration < kThreadSafeIterations; ++iteration) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(iteration % 10, ash::DeskTemplateType::kTemplate),
        base::BindOnce(&VerifyEntryAddedCorrectly));

    if (iteration % data_manager_->GetDeskTemplateEntryCount() == 0) {
      data_manager_->DeleteAllEntries(
          base::BindOnce(&VerifyEntryDeletedCorrectly));
    }
  }

  task_environment_.RunUntilIdle();
}

}  // namespace desks_storage
