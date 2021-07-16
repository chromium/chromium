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
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/desks_storage/core/desk_template.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

constexpr char kJunkFileName[] = "01.template";
constexpr char kJunkData[] = "dsjadsueAUWLKD293958";

// Search |uuid_list| for |uuid_query| returns true if found false if not.
//
// we don't know what order the dir_reader will read the files back to us so
// instead of relying on the operation returning the uuids in order we can
// simply run a linear search for each of the uuids we expect to find.
//
// we don't use std::find here because we want to run each member through
// the string compare method.
bool FindUuidInUuidList(const std::string& uuid_query,
                        const std::vector<std::string>& uuid_list) {
  for (const std::string& uuid : uuid_list) {
    if (uuid.compare(uuid_query) == 0)
      return true;
  }

  return false;
}

// Verifies that the status passed into it is kOk
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

void WriteJunkData(const base::FilePath& temp_dir) {
  base::FilePath full_path = temp_dir.Append(std::string(kJunkFileName));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EXPECT_TRUE(base::WriteFile(full_path, kJunkData));
}

}  // namespace

class LocalDeskDataManagerTest : public testing::Test {
 public:
  LocalDeskDataManagerTest()
      : sample_desk_template_one_(
            std::make_unique<ash::DeskTemplate>(std::string("01"),
                                                std::string("desk_01"),
                                                base::Time::Now())),
        sample_desk_template_two_(
            std::make_unique<ash::DeskTemplate>(std::string("02"),
                                                std::string("desk_02"),
                                                base::Time::Now())),
        sample_desk_template_three_(
            std::make_unique<ash::DeskTemplate>(std::string("03"),
                                                std::string("desk_03"),
                                                base::Time::Now())),
        modified_sample_desk_template_one_(
            std::make_unique<ash::DeskTemplate>(std::string("01"),
                                                std::string("desk_01_mod"),
                                                base::Time::Now())) {
    sample_desk_template_one_->set_desk_restore_data(
        std::make_unique<full_restore::RestoreData>());
    sample_desk_template_two_->set_desk_restore_data(
        std::make_unique<full_restore::RestoreData>());
    sample_desk_template_three_->set_desk_restore_data(
        std::make_unique<full_restore::RestoreData>());
    modified_sample_desk_template_one_->set_desk_restore_data(
        std::make_unique<full_restore::RestoreData>());
  }

  LocalDeskDataManagerTest(const LocalDeskDataManagerTest&) = delete;
  LocalDeskDataManagerTest& operator=(const LocalDeskDataManagerTest&) = delete;

  ~LocalDeskDataManagerTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    testing::Test::SetUp();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_two_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_three_;
  std::unique_ptr<ash::DeskTemplate> modified_sample_desk_template_one_;
};

TEST_F(LocalDeskDataManagerTest, CanAddEntry) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));
}

TEST_F(LocalDeskDataManagerTest, CanGetAllUuids) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  // Because we're using a SequencedTaskRunner we can assume that all of the
  // previous tasks have been completed by the time we actually attempt to
  // read the UUIDs.
  data_manager.GetAllUuids(
      base::BindLambdaForTesting([](DeskModel::GetAllUuidsStatus status,
                                    const std::vector<std::string>& uuids) {
        EXPECT_EQ(status, DeskModel::GetAllUuidsStatus::kOk);
        EXPECT_EQ(uuids.size(), static_cast<unsigned long>(3));
        EXPECT_TRUE(FindUuidInUuidList(std::string("01"), uuids));
        EXPECT_TRUE(FindUuidInUuidList(std::string("02"), uuids));
        EXPECT_TRUE(FindUuidInUuidList(std::string("03"), uuids));

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(std::string("04"), uuids));
      }));
}

TEST_F(LocalDeskDataManagerTest, CanGetEntryByUuid) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.GetEntryByUUID(
      std::string("01"),
      base::BindLambdaForTesting(
          [](DeskModel::GetEntryByUuidStatus status,
             std::unique_ptr<ash::DeskTemplate> result_template) {
            EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

            EXPECT_EQ(result_template->uuid(),
                      base::GUID::ParseCaseInsensitive(std::string("01")));
            EXPECT_EQ(result_template->template_name(),
                      base::UTF8ToUTF16(std::string("desk_01")));
            EXPECT_EQ(result_template->created_time(), base::Time());
          }));
}

TEST_F(LocalDeskDataManagerTest, GetEntryByUuidFailsIfEntryDoesntExist) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.GetEntryByUUID(
      std::string("01"),
      base::BindLambdaForTesting([](DeskModel::GetEntryByUuidStatus status,
                                    std::unique_ptr<ash::DeskTemplate> _) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, status);
      }));
}

TEST_F(LocalDeskDataManagerTest, GetEntryByUuidFailsIfEntryHasBadData) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  auto task_runner = task_environment.GetMainThreadTaskRunner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&WriteJunkData, temp_dir_.GetPath()));

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.GetEntryByUUID(
      std::string("01"),
      base::BindLambdaForTesting([](DeskModel::GetEntryByUuidStatus status,
                                    std::unique_ptr<ash::DeskTemplate> _) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kFailure, status);
      }));
}

TEST_F(LocalDeskDataManagerTest, CanUpdateEntry) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.AddOrUpdateEntry(std::move(modified_sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.GetEntryByUUID(
      std::string("01"),
      base::BindLambdaForTesting(
          [](DeskModel::GetEntryByUuidStatus status,
             std::unique_ptr<ash::DeskTemplate> result_template) {
            EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

            EXPECT_EQ(result_template->uuid(),
                      base::GUID::ParseCaseInsensitive(std::string("01")));
            EXPECT_EQ(result_template->template_name(),
                      base::UTF8ToUTF16(std::string("desk_01_mod")));
            EXPECT_EQ(result_template->created_time(), base::Time());
          }));
}

TEST_F(LocalDeskDataManagerTest, CanDeleteEntry) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.DeleteEntry(
      std::string("01"),
      base::BindLambdaForTesting([](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  data_manager.GetAllUuids(
      base::BindLambdaForTesting([](DeskModel::GetAllUuidsStatus status,
                                    const std::vector<std::string>& uuids) {
        EXPECT_EQ(status, DeskModel::GetAllUuidsStatus::kOk);
        EXPECT_EQ(uuids.size(), 0ul);
      }));
}

TEST_F(LocalDeskDataManagerTest, CanDeleteAllEntries) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  LocalDeskDataManager data_manager(temp_dir_.GetPath());

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager.DeleteAllEntries(
      base::BindLambdaForTesting([](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  data_manager.GetAllUuids(
      base::BindLambdaForTesting([](DeskModel::GetAllUuidsStatus status,
                                    const std::vector<std::string>& uuids) {
        EXPECT_EQ(status, DeskModel::GetAllUuidsStatus::kOk);
        EXPECT_EQ(uuids.size(), 0ul);
      }));
}

}  // namespace desks_storage
