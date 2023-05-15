// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_manager.h"

#include <algorithm>
#include <memory>
#include <set>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

enum class CallbackStatus {
  NOT_CALLED,
  CALLED_FALSE,
  CALLED_TRUE,
};

class ArchiveManagerTest : public testing::Test {
 public:
  ArchiveManagerTest();
  void SetUp() override;

  void PumpLoop();
  void ResetResults();

  void ResetManager(const base::FilePath& temporary_dir,
                    const base::FilePath& private_archive_dir,
                    const base::FilePath& public_archive_dir);
  void Callback(bool result);
  void GetStorageStatsCallback(
      const ArchiveManager::StorageStats& storage_sizes);

  ArchiveManager* manager() { return manager_.get(); }
  const base::FilePath& temporary_archive_path() const {
    return manager_->GetTemporaryArchivesDir();
  }
  const base::FilePath& private_archive_path() const {
    return manager_->GetPrivateArchivesDir();
  }
  const base::FilePath& public_archive_path() const {
    return manager_->GetPublicArchivesDir();
  }
  CallbackStatus callback_status() const { return callback_status_; }
  const std::set<base::FilePath>& last_archive_paths() const {
    return last_archive_paths_;
  }
  ArchiveManager::StorageStats last_storage_sizes() const {
    return last_storage_sizes_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir private_archive_dir_;
  base::ScopedTempDir public_archive_dir_;

  std::unique_ptr<ArchiveManager> manager_;
  CallbackStatus callback_status_;
  std::set<base::FilePath> last_archive_paths_;
  ArchiveManager::StorageStats last_storage_sizes_;
};

ArchiveManagerTest::ArchiveManagerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_current_default_handle_(task_runner_),
      callback_status_(CallbackStatus::NOT_CALLED),
      last_storage_sizes_({0, 0, 0}) {}

void ArchiveManagerTest::SetUp() {
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(private_archive_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(public_archive_dir_.CreateUniqueTempDir());
  ResetManager(temporary_dir_.GetPath(), private_archive_dir_.GetPath(),
               public_archive_dir_.GetPath());
}

void ArchiveManagerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void ArchiveManagerTest::ResetResults() {
  callback_status_ = CallbackStatus::NOT_CALLED;
  last_archive_paths_.clear();
}

void ArchiveManagerTest::ResetManager(
    const base::FilePath& temporary_dir,
    const base::FilePath& private_archive_dir,
    const base::FilePath& public_archive_dir) {
  manager_ = std::make_unique<ArchiveManager>(
      temporary_dir, private_archive_dir, public_archive_dir,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ArchiveManagerTest::Callback(bool result) {
  callback_status_ =
      result ? CallbackStatus::CALLED_TRUE : CallbackStatus::CALLED_FALSE;
}

void ArchiveManagerTest::GetStorageStatsCallback(
    const ArchiveManager::StorageStats& storage_sizes) {
  last_storage_sizes_ = storage_sizes;
}

TEST_F(ArchiveManagerTest, EnsureArchivesDirCreated) {
  base::FilePath temporary_archive_dir =
      temporary_archive_path().Append(FILE_PATH_LITERAL("test_path"));
  base::FilePath private_archive_dir =
      private_archive_path().Append(FILE_PATH_LITERAL("test_path"));
  base::FilePath public_archive_dir(FILE_PATH_LITERAL("/sdcard/Download"));
  ResetManager(temporary_archive_dir, private_archive_dir, public_archive_dir);
  EXPECT_FALSE(base::PathExists(temporary_archive_dir));
  EXPECT_FALSE(base::PathExists(private_archive_dir));

  // Ensure archives dir exists, when it doesn't.
  manager()->EnsureArchivesDirCreated(base::BindOnce(
      &ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(temporary_archive_dir));
  EXPECT_TRUE(base::PathExists(private_archive_dir));

  // Try again when the file already exists.
  ResetResults();
  manager()->EnsureArchivesDirCreated(base::BindOnce(
      &ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(temporary_archive_dir));
  EXPECT_TRUE(base::PathExists(private_archive_dir));
}

TEST_F(ArchiveManagerTest, GetStorageStats) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temporary_archive_path(),
                                             &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(public_archive_path(), &archive_path_3));

  manager()->GetStorageStats(base::BindOnce(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_GT(last_storage_sizes().internal_free_disk_space, 0);
  EXPECT_GT(last_storage_sizes().external_free_disk_space, 0);
  EXPECT_EQ(last_storage_sizes().temporary_archives_size,
            base::ComputeDirectorySize(temporary_archive_path()));
  EXPECT_EQ(last_storage_sizes().private_archives_size,
            base::ComputeDirectorySize(private_archive_path()));
  EXPECT_EQ(last_storage_sizes().public_archives_size,
            base::ComputeDirectorySize(public_archive_path()));
}

TEST_F(ArchiveManagerTest, TryWithInvalidTemporaryPath) {
  base::FilePath invalid_path;
  ResetManager(invalid_path, private_archive_path(), public_archive_path());

  manager()->GetStorageStats(base::BindOnce(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(base::SysInfo::AmountOfFreeDiskSpace(temporary_archive_path()),
            last_storage_sizes().internal_free_disk_space);
  EXPECT_EQ(base::ComputeDirectorySize(private_archive_path()),
            last_storage_sizes().internal_archives_size());
  EXPECT_EQ(0, last_storage_sizes().temporary_archives_size);
}

TEST_F(ArchiveManagerTest, TryWithInvalidPublicPath) {
  base::FilePath invalid_path;
  ResetManager(temporary_archive_path(), private_archive_path(), invalid_path);

  manager()->GetStorageStats(base::BindOnce(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(base::SysInfo::AmountOfFreeDiskSpace(public_archive_path()),
            last_storage_sizes().external_free_disk_space);
  EXPECT_EQ(base::ComputeDirectorySize(temporary_archive_path()) +
                base::ComputeDirectorySize(private_archive_path()),
            last_storage_sizes().internal_archives_size());
  EXPECT_EQ(0, last_storage_sizes().public_archives_size);
}

}  // namespace offline_pages
