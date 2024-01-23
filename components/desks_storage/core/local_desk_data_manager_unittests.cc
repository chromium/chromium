// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include <stddef.h>

#include <string>
#include <string_view>

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/desks_storage/core/desk_storage_metrics_util.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

using TestUuidId = base::StrongAlias<class TestUuidIdTag, int>;

// Search `entry_list` for `entry_query` as a uuid and returns true if
// found, false if not.
bool FindUuidInUuidList(
    const base::Uuid& uuid,
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        entry_list) {
  for (const ash::DeskTemplate* entry : entry_list) {
    if (entry->uuid() == uuid)
      return true;
  }

  return false;
}

base::FilePath GetInvalidFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("?"));
}

base::Uuid GetTestUuid(TestUuidId uuid_id) {
  return base::Uuid::ParseCaseInsensitive(
      base::StringPrintf("1c186d5a-502e-49ce-9ee1-%012d", uuid_id.value()));
}

std::string GetTestFileNameString(TestUuidId uuid_id) {
  return base::StringPrintf("%s.template",
                            GetTestUuid(uuid_id).AsLowercaseString().c_str());
}

std::string GetTestSaveDeskFileNameString(TestUuidId uuid_id) {
  return base::StringPrintf("%s.saveddesk",
                            GetTestUuid(uuid_id).AsLowercaseString().c_str());
}

std::string GetTestTemplateJSONData() {
  return "{\"version\":1,\"uuid\":\"" +
         GetTestUuid(TestUuidId(1)).AsLowercaseString() +
         "\",\"name\":\""
         "Saved Desk Template 1"
         "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
         "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
         "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
         "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
         "\"url\":\"https://"
         "example.com\",\"title\":\"Example\"},{\"url\":\"https://"
         "example.com/"
         "2\",\"title\":\"Example2\"}],\"active_tab_index\":1,\"window_id\":0,"
         "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}}";
}

std::string GetPolicyWithOneTemplate() {
  return "[{\"version\":1,\"uuid\":\"" +
         GetTestUuid(TestUuidId(9)).AsLowercaseString() +
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

std::string GetUnParseableTemplate() {
  return "????????/asdfg;.lkawhuerfflizHxcbklnmzd;kldfgjha;owwem";
}

// Verifies that the status passed into it is kOk
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status,
                               std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

// Verifies that the status passed into it is kOk
void VerifyEntryDeletedCorrectly(DeskModel::DeleteEntryStatus status) {
  EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
}

// Verifies that the status passed into it is kFailure
void VerifyEntryAddedFailure(DeskModel::AddOrUpdateEntryStatus status,
                             std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kFailure);
}

// Verifies that the status passed into it is kInvalidArgument
void VerifyEntryAddedErrorInvalidArgument(
    DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kInvalidArgument);
}

void VerifyEntryAddedErrorHitMaximumLimit(
    DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<ash::DeskTemplate> new_entry) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit);
}

void WriteJunkData(const base::FilePath& temp_dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EXPECT_TRUE(
      base::WriteFile(temp_dir.Append(GetTestFileNameString(TestUuidId(1))),
                      "This is not valid template data."));
}

void WriteIncorrectlyNamedData(const base::FilePath& temp_dir) {
  base::FilePath saved_desk_path = temp_dir.Append("saveddesk");
  base::CreateDirectory(saved_desk_path);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EXPECT_TRUE(base::WriteFile(
      saved_desk_path.Append(GetTestSaveDeskFileNameString(TestUuidId(2))),
      GetTestTemplateJSONData()));
}

// Make test template with ID containing the index. Defaults to desk template
// type if a type is not specified.
std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    int index,
    ash::DeskTemplateType type) {
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          base::Uuid::ParseCaseInsensitive(base::StringPrintf(
              "1c186d5a-502e-49ce-9ee1-00000000000%d", index)),
          ash::DeskTemplateSource::kUser, base::StringPrintf("desk_%d", index),
          base::Time::Now(), type);
  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>());
  return desk_template;
}

// Make test template with default restore data.
std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(
    const base::Uuid& uuid,
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
    const base::Uuid& uuid,
    const std::string& name,
    const base::Time created_time) {
  auto entry = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, name, created_time,
      ash::DeskTemplateType::kSaveAndRecall);
  entry->set_desk_restore_data(std::make_unique<app_restore::RestoreData>());
  return entry;
}

// Creates a template with the same UUID as the test policy template but with
// a different policy.
std::unique_ptr<ash::DeskTemplate> MakePolicyTemplateWithEmptyPolicy() {
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          base::Uuid::ParseCaseInsensitive(
              "27ea906b-a7d3-40b1-8c36-76d332d7f184"),
          ash::DeskTemplateSource::kUser, base::StringPrintf("desk_01"),
          base::Time::Now(), ash::DeskTemplateType::kTemplate, false,
          base::Value());
  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>());
  return desk_template;
}

}  // namespace

// TODO(crbug:1320940): Clean up tests to move on from std::string.
class LocalDeskDataManagerTest : public testing::Test {
 public:
  LocalDeskDataManagerTest()
      : sample_desk_template_one_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(1)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 base::Time())),
        sample_desk_template_one_duplicate_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(5)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 base::Time::Now())),
        sample_desk_template_one_duplicate_two_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(6)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(7)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_two_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(8)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        duplicate_pattern_matching_named_desk_three_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(9)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("(1) desk_template"),
                                 base::Time::Now())),
        sample_desk_template_two_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(2)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_02"),
                                 base::Time::Now())),
        sample_desk_template_three_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(3)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_03"),
                                 base::Time::Now())),
        sample_save_and_recall_desk_one_(
            MakeTestSaveAndRecallDesk(GetTestUuid(TestUuidId(10)),
                                      "save_and_recall_desk_01",
                                      base::Time())),
        sample_save_and_recall_desk_two_(
            MakeTestSaveAndRecallDesk(GetTestUuid(TestUuidId(11)),
                                      "save_and_recall_desk_02",
                                      base::Time::Now())),
        sample_save_and_recall_desk_three_(
            MakeTestSaveAndRecallDesk(GetTestUuid(TestUuidId(12)),
                                      "save_and_recall_desk_03",
                                      base::Time::Now())),
        modified_sample_desk_template_one_(
            MakeTestDeskTemplate(GetTestUuid(TestUuidId(1)),
                                 ash::DeskTemplateSource::kUser,
                                 std::string("desk_01_mod"),
                                 base::Time())),
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
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
    task_environment_.RunUntilIdle();
    testing::Test::SetUp();
  }

  void AddTwoTemplates() {
    base::RunLoop loop1;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_desk_template_one_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_desk_template_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
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
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    data_manager_->AddOrUpdateEntry(
        std::move(sample_save_and_recall_desk_two_),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void VerifyAllEntries(size_t expected_size, const std::string& trace_string) {
    SCOPED_TRACE(trace_string);

    task_environment_.RunUntilIdle();

    auto result = data_manager_->GetAllEntries();

    EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
    EXPECT_EQ(result.entries.size(), expected_size);
  }

  void VerifyUpdateEntryDuplicate() {
    EXPECT_EQ(data_manager_->last_update_status_,
              LocalDeskDataManager::UpdateEntryStatus::kDuplicate);
  }

  void VerifyUpdateEntryOk() {
    EXPECT_EQ(data_manager_->last_update_status_,
              LocalDeskDataManager::UpdateEntryStatus::kOk);
  }

  void VerifyUpdateEntryOutdatePolicy() {
    EXPECT_EQ(data_manager_->last_update_status_,
              LocalDeskDataManager::UpdateEntryStatus::kOutdatedPolicy);
  }

  void VerifyUpdateEntryNotFound() {
    EXPECT_EQ(data_manager_->last_update_status_,
              LocalDeskDataManager::UpdateEntryStatus::kNotFound);
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
  for (size_t index = 0u; index < data_manager_->GetMaxDeskTemplateEntryCount();
       ++index) {
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
  for (size_t index = 0u;
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

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 3ul);
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(1)), result.entries));
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(2)), result.entries));
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(3)), result.entries));

  // Sanity check for the search function.
  EXPECT_FALSE(FindUuidInUuidList(GetTestUuid(TestUuidId(4)), result.entries));
}

TEST_F(LocalDeskDataManagerTest, CanGetAllUuids) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();

  std::set<base::Uuid> entry_uuids = data_manager_->GetAllEntryUuids();

  entry_uuids.erase(GetTestUuid(TestUuidId(1)));
  entry_uuids.erase(GetTestUuid(TestUuidId(2)));
  entry_uuids.erase(GetTestUuid(TestUuidId(3)));

  // We should have exactly the correct set of IDs returned from the model.
  EXPECT_TRUE(entry_uuids.empty());
}

TEST_F(LocalDeskDataManagerTest, GetAllEntriesIncludesPolicyValues) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->SetPolicyDeskTemplates(GetPolicyWithOneTemplate());

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 4ul);
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(1)), result.entries));
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(2)), result.entries));
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(3)), result.entries));
  EXPECT_TRUE(FindUuidInUuidList(GetTestUuid(TestUuidId(9)), result.entries));

  // One of these templates should be from policy.
  EXPECT_EQ(base::ranges::count_if(result.entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            1l);

  // Sanity check for the search function.
  EXPECT_FALSE(FindUuidInUuidList(GetTestUuid(TestUuidId(4)), result.entries));

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
      ash::DeskTemplateType::kTemplate, GetTestUuid(TestUuidId(1))));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanDetectNoDuplicateEntryNames) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  EXPECT_FALSE(data_manager_->FindOtherEntryWithName(
      base::UTF8ToUTF16(std::string("desk_01")),
      ash::DeskTemplateType::kTemplate, GetTestUuid(TestUuidId(1))));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanGetEntryByUuid) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_EQ(GetTestUuid(TestUuidId(1)), result.entry->uuid());
  EXPECT_EQ(base::UTF8ToUTF16(std::string("desk_01")),
            result.entry->template_name());
  EXPECT_EQ(base::Time(), result.entry->created_time());
}

TEST_F(LocalDeskDataManagerTest, GetEntryByUuidShouldReturnAdminTemplate) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  // Set admin template with UUID: GetTestUuid(TestUuidId(9)).
  data_manager_->SetPolicyDeskTemplates(GetPolicyWithOneTemplate());

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(9)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_EQ(GetTestUuid(TestUuidId(9)), result.entry->uuid());
  EXPECT_EQ(ash::DeskTemplateSource::kPolicy, result.entry->source());
  EXPECT_EQ(base::UTF8ToUTF16(std::string("Admin Template 1")),
            result.entry->template_name());
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsNotFoundIfEntryDoesNotExist) {
  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, result.status);
}

TEST_F(LocalDeskDataManagerTest, DeskTemplateIsIgnoredIfItHasBadData) {
  auto task_runner = task_environment_.GetMainThreadTaskRunner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&WriteJunkData, temp_dir_.GetPath()));

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, result.status);
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsFailureIfDeskManagerHasInvalidPath) {
  data_manager_ =
      std::make_unique<LocalDeskDataManager>(GetInvalidFilePath(), account_id_);
  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kFailure, result.status);
}

TEST_F(LocalDeskDataManagerTest,
       CanRenameSavedDeskTemplateIfFilenameDoesNotMatchUUID) {
  // Initialize new local temp directory
  base::ScopedTempDir local_temp_dir;
  EXPECT_TRUE(local_temp_dir.CreateUniqueTempDir());

  // Pre-write file into directory for correcting
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteIncorrectlyNamedData, local_temp_dir.GetPath()));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(
      local_temp_dir.GetPath()
          .Append("saveddesk")
          .Append(GetTestSaveDeskFileNameString(TestUuidId(2)))));
  EXPECT_FALSE(base::PathExists(
      local_temp_dir.GetPath()
          .Append("saveddesk")
          .Append(GetTestSaveDeskFileNameString(TestUuidId(1)))));

  // Initialize temp data manager
  auto temp_data_manager_ = std::make_unique<LocalDeskDataManager>(
      local_temp_dir.GetPath(), account_id_);
  task_environment_.RunUntilIdle();

  // Retrieve entry from test directory
  auto result = temp_data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_EQ(result.entry->uuid(), GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(temp_data_manager_->GetEntryCount(), 1u);
  EXPECT_TRUE(base::PathExists(
      local_temp_dir.GetPath()
          .Append("saveddesk")
          .Append(GetTestSaveDeskFileNameString(TestUuidId(1)))));
  EXPECT_FALSE(base::PathExists(
      local_temp_dir.GetPath()
          .Append("saveddesk")
          .Append(GetTestSaveDeskFileNameString(TestUuidId(2)))));
}

TEST_F(LocalDeskDataManagerTest, CanUpdateEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(modified_sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_EQ(GetTestUuid(TestUuidId(1)), result.entry->uuid());
  EXPECT_EQ(base::UTF8ToUTF16(std::string("desk_01_mod")),
            result.entry->template_name());
  EXPECT_EQ(base::Time(), result.entry->created_time());
}

TEST_F(LocalDeskDataManagerTest, CanDeleteEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->DeleteEntry(
      GetTestUuid(TestUuidId(1)),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Delete one entry");
}

TEST_F(LocalDeskDataManagerTest, CanDeleteAllEntries) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Delete all entries");
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryCountShouldIncludeBothUserAndAdminTemplates) {
  // Add two user templates.
  AddTwoTemplates();

  // Set one admin template.
  data_manager_->SetPolicyDeskTemplates(GetPolicyWithOneTemplate());

  // There should be 3 templates: 2 user templates + 1 admin template.
  EXPECT_EQ(3ul, data_manager_->GetEntryCount());
}

TEST_F(LocalDeskDataManagerTest,
       GetMaxEntryCountShouldIncreaseWithAdminTemplates) {
  // Add two user templates.
  AddTwoTemplates();

  size_t max_entry_count = data_manager_->GetMaxDeskTemplateEntryCount() +
                           data_manager_->GetMaxSaveAndRecallDeskEntryCount();
  EXPECT_EQ(12ul, max_entry_count);

  // Set one admin template.
  data_manager_->SetPolicyDeskTemplates(GetPolicyWithOneTemplate());

  size_t max_entry_count_with_admin_template =
      data_manager_->GetMaxDeskTemplateEntryCount() +
      data_manager_->GetMaxSaveAndRecallDeskEntryCount();

  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(13ul, max_entry_count_with_admin_template);
}

TEST_F(LocalDeskDataManagerTest, AddDeskTemplatesAndSaveAndRecallDeskEntries) {
  // Add two user templates.
  AddTwoTemplates();

  // Add two SaveAndRecall desks.
  AddTwoSaveAndRecallDeskTemplates();

  EXPECT_EQ(data_manager_->GetEntryCount(), 4ul);
  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 2ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 2ul);

  auto result = data_manager_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);

  VerifyAllEntries(4ul,
                   "Add two desks templates and two saved and recall desks");
}

TEST_F(LocalDeskDataManagerTest, AddSaveAndRecallDeskEntry) {
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kSaveAndRecall),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");

  // Verify that it's not SaveAndRecall entry in the desk template cache.
  auto result = data_manager_->GetAllEntries();

  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 1ul);
  EXPECT_EQ(result.entries[0]->type(), ash::DeskTemplateType::kSaveAndRecall);

  EXPECT_EQ(data_manager_->GetDeskTemplateEntryCount(), 0ul);
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 1ul);
}

TEST_F(LocalDeskDataManagerTest, CanGetSaveAndRecallDeskEntryByUuid) {
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();

  auto result = data_manager_->GetEntryByUUID(GetTestUuid(TestUuidId(10)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_EQ(GetTestUuid(TestUuidId(10)), result.entry->uuid());
  EXPECT_EQ(u"save_and_recall_desk_01", result.entry->template_name());
  EXPECT_EQ(base::Time(), result.entry->created_time());
}

TEST_F(LocalDeskDataManagerTest, CanDeleteSaveAndRecallDeskEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  VerifyAllEntries(1ul, "Added one save and recall desk");
  EXPECT_EQ(data_manager_->GetSaveAndRecallDeskEntryCount(), 1ul);
  data_manager_->DeleteEntry(
      GetTestUuid(TestUuidId(10)),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Deleted one save and recall desk");
}

TEST_F(LocalDeskDataManagerTest, CanAddMaxEntriesForBothTypes) {
  for (size_t index = 0u;
       index < data_manager_->GetMaxSaveAndRecallDeskEntryCount(); ++index) {
    data_manager_->AddOrUpdateEntry(
        MakeTestDeskTemplate(index, ash::DeskTemplateType::kSaveAndRecall),
        base::BindOnce(&VerifyEntryAddedCorrectly));
  }
  for (size_t index = 0u; index < data_manager_->GetMaxDeskTemplateEntryCount();
       ++index) {
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
  for (size_t index = 0u; index < data_manager_->GetMaxDeskTemplateEntryCount();
       ++index) {
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
  for (size_t index = 0u;
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
  for (size_t index = 0u; index < 2u; ++index) {
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
  for (size_t index = 0u; index < 2u; ++index) {
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
      GetTestUuid(TestUuidId(1)),
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
  for (size_t index = 0u; index < 4u; ++index) {
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
  constexpr uint32_t kThreadSafeIterations = 1000;

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

TEST_F(LocalDeskDataManagerTest, DeleteSameEntryAgain) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->DeleteEntry(
      GetTestUuid(TestUuidId(1)),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  data_manager_->DeleteEntry(
      GetTestUuid(TestUuidId(1)),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  VerifyAllEntries(0ul, "Delete one entry");
  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanHandleFileErrorGracefully) {
  base::ScopedTempDir local_temp_dir;
  EXPECT_TRUE(local_temp_dir.CreateUniqueTempDir());

  // Write out file that should not be parseable.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath path_to_dir = local_temp_dir.GetPath().Append("saveddesk");
  base::CreateDirectory(path_to_dir);
  EXPECT_TRUE(base::WriteFile(path_to_dir.Append("bad_format.saveddesk"),
                              GetUnParseableTemplate()));

  auto local_data_manager = std::make_unique<LocalDeskDataManager>(
      local_temp_dir.GetPath(), account_id_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(local_data_manager->GetEntryCount(), static_cast<size_t>(0));
}

TEST_F(LocalDeskDataManagerTest, CanRecordFileSizeMetrics) {
  base::HistogramTester histogram_tester;
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(std::move(sample_save_and_recall_desk_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(kTemplateSizeHistogramName, 1u);
  histogram_tester.ExpectTotalCount(kSaveAndRecallTemplateSizeHistogramName,
                                    1u);
}

TEST_F(LocalDeskDataManagerTest, AddUnknownDeskTypeShouldFail) {
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(1u, ash::DeskTemplateType::kUnknown),
      base::BindOnce(&VerifyEntryAddedErrorInvalidArgument));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, UpdtesAdminTemplatesCorrectly) {
  // populate with single template.
  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(GetTestUuid(TestUuidId(5)),
                           ash::DeskTemplateSource::kUser,
                           std::string("desk_01"), base::Time::Now()),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  // Update with modified template.
  data_manager_->UpdateEntry(MakeTestDeskTemplate(
      GetTestUuid(TestUuidId(5)), ash::DeskTemplateSource::kUser,
      std::string("desk_02"), base::Time::Now()));

  task_environment_.RunUntilIdle();

  VerifyAllEntries(1ul, "Updated template");
  auto result = data_manager_->GetAllEntries();
  EXPECT_EQ(result.entries.at(0)->template_name(), u"desk_02");
  VerifyUpdateEntryOk();
}

TEST_F(LocalDeskDataManagerTest, IngoresUpdateForNonExistantTemplate) {
  // Update with modified template.
  data_manager_->UpdateEntry(MakeTestDeskTemplate(
      GetTestUuid(TestUuidId(5)), ash::DeskTemplateSource::kUser,
      std::string("desk_02"), base::Time::Now()));

  task_environment_.RunUntilIdle();

  VerifyAllEntries(0ul, "Updated template");
  VerifyUpdateEntryNotFound();
}

TEST_F(LocalDeskDataManagerTest, DoesNotUpdateWhenRestoreContentIsTheSame) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kAdminTemplatePolicyWithOneTemplate));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_list());

  // "retrieve policy" and add it to the model. We do this to easily get a
  // fully enough defined template.
  std::vector<std::unique_ptr<ash::DeskTemplate>> parsed_policy =
      desk_template_conversion::ParseAdminTemplatesFromPolicyValue(
          parsed_json.value());
  data_manager_->AddOrUpdateEntry(parsed_policy.at(0)->Clone(),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  // If we update the template it should return kDuplicate.
  data_manager_->UpdateEntry(parsed_policy.at(0)->Clone());
  VerifyUpdateEntryDuplicate();
}

TEST_F(LocalDeskDataManagerTest, DoesNotOverwriteOnDifferentPolicy) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kAdminTemplatePolicyWithOneTemplate));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_list());

  // "retrieve policy" and add it to the model. We do this to easily get a
  // fully enough defined template.
  std::vector<std::unique_ptr<ash::DeskTemplate>> parsed_policy =
      desk_template_conversion::ParseAdminTemplatesFromPolicyValue(
          parsed_json.value());
  data_manager_->AddOrUpdateEntry(parsed_policy.at(0)->Clone(),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  // If we update the template it should return kOutdatedPolicy because the
  // policy definitions differ.  During runtime this happens if we attempt to
  // update a template that has had a new policy pushed to it.
  data_manager_->UpdateEntry(MakePolicyTemplateWithEmptyPolicy());
  VerifyUpdateEntryOutdatePolicy();
}

}  // namespace desks_storage
