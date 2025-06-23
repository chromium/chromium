// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/persistent_cache/backend_params_manager.h"

#include <cstdint>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/persistent_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

class BackendParamsManagerTest : public testing::Test {
  void SetUp() override { CHECK(temp_dir_.CreateUniqueTempDir()); }

 protected:
  int CountFiles() {
    base::FileEnumerator file_enumarator(temp_dir_.GetPath(),
                                         /*recursive=*/true,
                                         base::FileEnumerator::FILES);
    int file_count = 0;
    file_enumarator.ForEach(
        [&file_count](const base::FilePath& file_path) { ++file_count; });

    return file_count;
  }

  std::vector<base::Time> GetSortedModificationTimes() {
    std::vector<base::Time> modification_times;

    base::FileEnumerator file_enumarator(
        temp_dir_.GetPath(), /*recursive=*/true, base::FileEnumerator::FILES);
    file_enumarator.ForEach(
        [&modification_times](const base::FilePath& file_path) {
          base::File::Info info;
          base::GetFileInfo(file_path, &info);
          modification_times.push_back(info.last_accessed);
        });

    // Sort so that oldest entries are first.
    std::sort(modification_times.begin(), modification_times.end());

    return modification_times;
  }

  void Fill(persistent_cache::BackendParamsManager& params_manager,
            int64_t file_count,
            int64_t file_size) {
    for (int i = 0; i < file_count; ++i) {
      const std::string key = base::NumberToString(i);

      // Actual sleep is necessary to get timestamp variety since
      // BackendParamsManager relies on OS file timestamps which are not
      // affected by mock time.
      base::PlatformThread::Sleep(base::Milliseconds(5));

      BackendParams params = params_manager.GetOrCreateParamsSync(
          BackendType::kSqlite, key,
          BackendParamsManager::AccessRights::kReadWrite);
      params.db_file.SetLength(file_size);
    }
  }

  // Create enough file that are big enough to go over kTargetFootprint.
  void OverFill(persistent_cache::BackendParamsManager& params_manager) {
    static constexpr int64_t kFileCount = 10;
    // Just large enough to ensure the target footprint will be surpassed.
    static constexpr int64_t kFileSize = (kTargetFootprint / kFileCount) + 100;
    Fill(params_manager, kFileCount, kFileSize);
  }

  static constexpr int64_t kTargetFootprint = 20000;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BackendParamsManagerTest,
       SynchronousCreationEqualsSubsequentSynchronousRetrieval) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  BackendParams params = params_manager.GetOrCreateParamsSync(
      BackendType::kSqlite, "key",
      BackendParamsManager::AccessRights::kReadWrite);
  EXPECT_TRUE(params.db_file.IsValid());
  EXPECT_TRUE(params.journal_file.IsValid());

  params.db_file.SetLength(42);

  BackendParams other_params = params_manager.GetOrCreateParamsSync(
      BackendType::kSqlite, "key",
      BackendParamsManager::AccessRights::kReadWrite);
  EXPECT_EQ(other_params.db_file.GetLength(), params.db_file.GetLength());
}

TEST_F(BackendParamsManagerTest, UnknownKeyTypePairQueryServedAsynchronously) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  base::RunLoop run_loop;

  BackendParams backend_params;
  params_manager.GetParamsSyncOrCreateAsync(
      BackendType::kSqlite, "key",
      BackendParamsManager::AccessRights::kReadWrite,
      base::BindLambdaForTesting(
          [&backend_params, &run_loop](const BackendParams& result) {
            backend_params = result.Copy();
            run_loop.Quit();
          }));

  // The callback was not invoked yet so files are not populated.
  EXPECT_FALSE(backend_params.db_file.IsValid());
  EXPECT_FALSE(backend_params.journal_file.IsValid());

  // Wait for the callback to be invoked. If never invoked the test will time
  // out.
  run_loop.Run();

  // Once received the params contain valid files.
  EXPECT_TRUE(backend_params.db_file.IsValid());
  EXPECT_TRUE(backend_params.journal_file.IsValid());
}

TEST_F(BackendParamsManagerTest, ExistingKeyTypePairQueryServedSynchronously) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  {
    BackendParams backend_params;
    base::RunLoop run_loop;
    params_manager.GetParamsSyncOrCreateAsync(
        BackendType::kSqlite, "key",
        BackendParamsManager::AccessRights::kReadWrite,
        base::BindLambdaForTesting(
            [&backend_params, &run_loop](const BackendParams& result) {
              backend_params = result.Copy();
              run_loop.Quit();
            }));

    // The callback was not invoked yet so files are not populated.
    EXPECT_FALSE(backend_params.db_file.IsValid());
    EXPECT_FALSE(backend_params.journal_file.IsValid());

    // Makes sure the callback runs on the ThreadPool.
    run_loop.Run();

    // Once received the params contain valid files.
    EXPECT_TRUE(backend_params.db_file.IsValid());
    EXPECT_TRUE(backend_params.journal_file.IsValid());
  }

  {
    BackendParams backend_params;
    base::RunLoop run_loop;
    params_manager.GetParamsSyncOrCreateAsync(
        BackendType::kSqlite, "key",
        BackendParamsManager::AccessRights::kReadWrite,
        base::BindLambdaForTesting(
            [&backend_params, &run_loop](const BackendParams& result) {
              backend_params = result.Copy();
              run_loop.Quit();
            }));

    // No need to run `run_loop` since the callback was invoked synchronously.

    // Once received the params contain valid files.
    EXPECT_TRUE(backend_params.db_file.IsValid());
    EXPECT_TRUE(backend_params.journal_file.IsValid());
  }
}

TEST_F(BackendParamsManagerTest, DeleteAllFiles) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  {
    BackendParams params = params_manager.GetOrCreateParamsSync(
        BackendType::kSqlite, "key",
        BackendParamsManager::AccessRights::kReadWrite);
    EXPECT_TRUE(params.db_file.IsValid());

    // Inserting an entry should have created at least one file.
    EXPECT_GE(CountFiles(), 1);
  }

  params_manager.DeleteAllFiles();
  EXPECT_EQ(CountFiles(), 0);
}

TEST_F(BackendParamsManagerTest, NoFileReductionIfNotNeeded) {
  const int64_t file_count = 1;
  const int64_t file_size = 100;

  BackendParamsManager params_manager(temp_dir_.GetPath());
  Fill(params_manager, file_count, file_size);
  EXPECT_LE(base::ComputeDirectorySize(temp_dir_.GetPath()), kTargetFootprint);

  // The footprint of the files do not approach the requested target size so
  // nothing is done.
  EXPECT_EQ(params_manager.BringDownTotalFootprintOfFiles(file_count *
                                                          file_size * 10),
            0);
}

TEST_F(BackendParamsManagerTest, BringDownTotalFootPrint) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  OverFill(params_manager);
  EXPECT_GE(base::ComputeDirectorySize(temp_dir_.GetPath()), kTargetFootprint);

  EXPECT_GT(params_manager.BringDownTotalFootprintOfFiles(kTargetFootprint), 0);

  EXPECT_LE(base::ComputeDirectorySize(temp_dir_.GetPath()), kTargetFootprint);
}

TEST_F(BackendParamsManagerTest, OldestEntriesAreRemovedFirst) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  OverFill(params_manager);

  std::vector<base::Time> modification_times = GetSortedModificationTimes();

  params_manager.BringDownTotalFootprintOfFiles(kTargetFootprint);

  std::vector<base::Time> modification_times_after_reduction =
      GetSortedModificationTimes();

  // Verify that the size of the files in the directory actually went down.
  ASSERT_GT(modification_times.size(),
            modification_times_after_reduction.size());

  auto original_timestamps_it = modification_times.rbegin();
  auto timestamps_after_reduction_it =
      modification_times_after_reduction.rbegin();

  // Verify that the entries removed were the oldest ones or tied for oldest.
  // The missing timestamps have to be the smallest since the data is sorted.
  // This test is tolerant of timestamps all being equal for whatever reason.
  while (timestamps_after_reduction_it !=
         modification_times_after_reduction.rend()) {
    EXPECT_EQ(*original_timestamps_it, *timestamps_after_reduction_it);

    ++original_timestamps_it;
    ++timestamps_after_reduction_it;
  }
}

}  // namespace persistent_cache
