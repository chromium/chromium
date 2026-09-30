// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_migration_vfs/page_migration_tracker.h"

#include <optional>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_migration_vfs {
namespace {

using Block = PageMigrationTracker::Block;

class PageMigrationTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PageMigrationTrackerTest, EmptyTracker) {
  PageMigrationTracker tracker(MigrationTarget::kTest, 0);
  EXPECT_EQ(tracker.total_pages_for_test(), 0u);
  EXPECT_TRUE(tracker.IsMigrationComplete());
  // Pages beyond total_pages are considered migrated to enable double writes.
  EXPECT_TRUE(tracker.IsMigrated(1));
  EXPECT_EQ(tracker.GetNextBlockToMigrateAndLockIt(), std::nullopt);

  EXPECT_CHECK_DEATH(tracker.IsMigrated(0));
}

TEST_F(PageMigrationTrackerTest, SinglePageTracker) {
  PageMigrationTracker tracker(MigrationTarget::kTest, 1);
  EXPECT_EQ(tracker.total_pages_for_test(), 1u);
  EXPECT_FALSE(tracker.IsMigrationComplete());
  EXPECT_FALSE(tracker.IsMigrated(1));

  auto block = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(block.has_value());
  EXPECT_EQ(block->start_page, 1u);
  EXPECT_EQ(block->end_page, 1u);

  tracker.MarkMigrationAsCompletedAndUnlockIt(*block,
                                              BlockMigrationResult::kSuccess);
  EXPECT_TRUE(tracker.IsMigrationComplete());
  EXPECT_TRUE(tracker.IsMigrated(1));
  EXPECT_EQ(tracker.GetNextBlockToMigrateAndLockIt(), std::nullopt);
}

TEST_F(PageMigrationTrackerTest, BlockBoundaries) {
  // 64 pages = 1 block [1, 64]
  PageMigrationTracker tracker64(MigrationTarget::kTest, 64);
  auto b64 = tracker64.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b64.has_value());
  EXPECT_EQ(b64->start_page, 1u);
  EXPECT_EQ(b64->end_page, 64u);
  tracker64.MarkMigrationAsCompletedAndUnlockIt(*b64,
                                                BlockMigrationResult::kSuccess);

  // 65 pages = 2 blocks: [1, 64], [65, 65]
  PageMigrationTracker tracker65(MigrationTarget::kTest, 65);
  auto b65_0 = tracker65.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b65_0.has_value());
  EXPECT_EQ(b65_0->start_page, 1u);
  EXPECT_EQ(b65_0->end_page, 64u);
  tracker65.MarkMigrationAsCompletedAndUnlockIt(*b65_0,
                                                BlockMigrationResult::kSuccess);

  auto b65_1 = tracker65.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b65_1.has_value());
  EXPECT_EQ(b65_1->start_page, 65u);
  EXPECT_EQ(b65_1->end_page, 65u);
  tracker65.MarkMigrationAsCompletedAndUnlockIt(*b65_1,
                                                BlockMigrationResult::kSuccess);

  // 128 pages = 2 blocks: [1, 64], [65, 128]
  PageMigrationTracker tracker128(MigrationTarget::kTest, 128);
  auto b128_0 = tracker128.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b128_0.has_value());
  EXPECT_EQ(b128_0->end_page, 64u);
  tracker128.MarkMigrationAsCompletedAndUnlockIt(
      *b128_0, BlockMigrationResult::kSuccess);

  auto b128_1 = tracker128.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b128_1.has_value());
  EXPECT_EQ(b128_1->start_page, 65u);
  EXPECT_EQ(b128_1->end_page, 128u);
  tracker128.MarkMigrationAsCompletedAndUnlockIt(
      *b128_1, BlockMigrationResult::kSuccess);
}

TEST_F(PageMigrationTrackerTest, DynamicPageGrowth) {
  PageMigrationTracker tracker(MigrationTarget::kTest, 100);

  // Pages within snapshot are unmigrated initially.
  EXPECT_FALSE(tracker.IsMigrated(1));
  EXPECT_FALSE(tracker.IsMigrated(100));

  // Pages allocated after migration began are immediately double-written.
  EXPECT_TRUE(tracker.IsMigrated(101));
  EXPECT_TRUE(tracker.IsMigrated(500));
}

TEST_F(PageMigrationTrackerTest, SequentialMigrationAndRetry) {
  // 130 pages = 3 blocks: [1, 64], [65, 128], [129, 130]
  PageMigrationTracker tracker(MigrationTarget::kTest, 130);

  // Migrate block 0.
  auto b0 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0.has_value());
  EXPECT_EQ(b0->start_page, 1u);
  EXPECT_EQ(b0->end_page, 64u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0,
                                              BlockMigrationResult::kSuccess);

  EXPECT_TRUE(tracker.IsMigrated(1));
  EXPECT_TRUE(tracker.IsMigrated(64));
  EXPECT_FALSE(tracker.IsMigrated(65));

  // Block 1 fails on first try.
  auto b1 = tracker.GetNextBlockToMigrateAndLockIt(b0);
  ASSERT_TRUE(b1.has_value());
  EXPECT_EQ(b1->start_page, 65u);
  EXPECT_EQ(b1->end_page, 128u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b1,
                                              BlockMigrationResult::kReadError);

  EXPECT_FALSE(tracker.IsMigrated(65));

  // Retry block 1 (passing b1 starts scan at block 1) and succeed.
  auto b1_retry = tracker.GetNextBlockToMigrateAndLockIt(b1);
  ASSERT_TRUE(b1_retry.has_value());
  EXPECT_EQ(b1_retry->start_page, 65u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b1_retry,
                                              BlockMigrationResult::kSuccess);
  EXPECT_TRUE(tracker.IsMigrated(65));
  EXPECT_TRUE(tracker.IsMigrated(128));

  // Migrate block 2 to complete.
  auto b2 = tracker.GetNextBlockToMigrateAndLockIt(b1_retry);
  ASSERT_TRUE(b2.has_value());
  EXPECT_EQ(b2->start_page, 129u);
  EXPECT_EQ(b2->end_page, 130u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b2,
                                              BlockMigrationResult::kSuccess);

  EXPECT_TRUE(tracker.IsMigrationComplete());
  EXPECT_EQ(tracker.GetNextBlockToMigrateAndLockIt(b2), std::nullopt);
}

TEST_F(PageMigrationTrackerTest, ScanFromPreviousBlockAndWrapAround) {
  // 192 pages = 3 blocks: [1, 64], [65, 128], [129, 192]
  PageMigrationTracker tracker(MigrationTarget::kTest, 192);

  // Worker A locks block 0.
  auto b0 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0.has_value());
  EXPECT_EQ(b0->start_page, 1u);

  // Worker B locks block 1 (since block 0 is locked) and completes it.
  auto b1 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b1.has_value());
  EXPECT_EQ(b1->start_page, 65u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b1,
                                              BlockMigrationResult::kSuccess);

  // Worker A fails block 0, leaving block 0 unmigrated and unlocked.
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0,
                                              BlockMigrationResult::kReadError);

  // Worker B requests next block starting from b1 -> should get block 2 first
  // without doubling back to block 0 yet.
  auto b2 = tracker.GetNextBlockToMigrateAndLockIt(b1);
  ASSERT_TRUE(b2.has_value());
  EXPECT_EQ(b2->start_page, 129u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b2,
                                              BlockMigrationResult::kSuccess);

  // Worker B requests next block starting from b2 -> wraps around to block 0.
  auto b0_retry = tracker.GetNextBlockToMigrateAndLockIt(b2);
  ASSERT_TRUE(b0_retry.has_value());
  EXPECT_EQ(b0_retry->start_page, 1u);
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0_retry,
                                              BlockMigrationResult::kSuccess);

  EXPECT_TRUE(tracker.IsMigrationComplete());
}

TEST_F(PageMigrationTrackerTest, ConcurrentMigration) {
  // 16 blocks = 1024 pages
  constexpr size_t kTotalPages = 1024;
  constexpr size_t kNumWorkers = 8;
  PageMigrationTracker tracker(MigrationTarget::kTest, kTotalPages);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(kNumWorkers, run_loop.QuitClosure());

  for (size_t i = 0; i < kNumWorkers; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](PageMigrationTracker* trk, base::RepeatingClosure done) {
              std::optional<Block> block;
              while ((block = trk->GetNextBlockToMigrateAndLockIt(block))) {
                trk->MarkMigrationAsCompletedAndUnlockIt(
                    *block, BlockMigrationResult::kSuccess);
              }
              done.Run();
            },
            base::Unretained(&tracker), barrier));
  }

  run_loop.Run();

  EXPECT_TRUE(tracker.IsMigrationComplete());
  EXPECT_EQ(tracker.GetNextBlockToMigrateAndLockIt(), std::nullopt);

  // Assert every single page in the database is migrated.
  for (size_t p = 1; p <= kTotalPages; ++p) {
    EXPECT_TRUE(tracker.IsMigrated(p));
  }
}

TEST_F(PageMigrationTrackerTest, HistogramsOnSuccessfulMigration) {
  base::HistogramTester histogram_tester;

  // 128 pages = 2 blocks
  PageMigrationTracker tracker(MigrationTarget::kTest, 128);

  const std::string status_histogram = GetStatusHistogramName(tracker.target());
  const std::string block_result_histogram =
      GetBlockResultHistogramName(tracker.target());
  const std::string block_time_histogram =
      GetBlockTimeHistogramName(tracker.target());
  const std::string total_time_histogram =
      GetTotalTimeHistogramName(tracker.target());
  const std::string total_blocks_histogram =
      GetTotalBlocksHistogramName(tracker.target());

  // Before any migration starts, histograms have not fired.
  histogram_tester.ExpectTotalCount(status_histogram, 0);
  histogram_tester.ExpectTotalCount(block_result_histogram, 0);
  histogram_tester.ExpectTotalCount(total_blocks_histogram, 0);

  // Migrate block 0.
  auto b0 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0.has_value());
  histogram_tester.ExpectUniqueSample(status_histogram,
                                      MigrationStatus::kStarted, 1);
  histogram_tester.ExpectUniqueSample(total_blocks_histogram, 2, 1);

  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0,
                                              BlockMigrationResult::kSuccess);
  histogram_tester.ExpectUniqueSample(block_result_histogram,
                                      BlockMigrationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(status_histogram,
                                     MigrationStatus::kCompleted, 0);
  histogram_tester.ExpectTotalCount(block_time_histogram, 1);

  // Migrate block 1.
  auto b1 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b1.has_value());
  histogram_tester.ExpectUniqueSample(status_histogram,
                                      MigrationStatus::kStarted, 1);

  tracker.MarkMigrationAsCompletedAndUnlockIt(*b1,
                                              BlockMigrationResult::kSuccess);
  histogram_tester.ExpectUniqueSample(block_result_histogram,
                                      BlockMigrationResult::kSuccess, 2);
  histogram_tester.ExpectTotalCount(block_time_histogram, 2);

  // Both blocks migrated -> kCompleted and TotalTime should now have fired.
  EXPECT_TRUE(tracker.IsMigrationComplete());
  histogram_tester.ExpectBucketCount(status_histogram,
                                     MigrationStatus::kCompleted, 1);
  histogram_tester.ExpectTotalCount(status_histogram, 2);
  histogram_tester.ExpectTotalCount(total_time_histogram, 1);
}

TEST_F(PageMigrationTrackerTest, HistogramsOnBlockMigrationFailures) {
  base::HistogramTester histogram_tester;

  // 65 pages = 2 blocks: [1, 64], [65, 65]
  PageMigrationTracker tracker(MigrationTarget::kTest, 65);

  const std::string status_histogram = GetStatusHistogramName(tracker.target());
  const std::string block_result_histogram =
      GetBlockResultHistogramName(tracker.target());

  auto b0 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0.has_value());

  // Fail block 0 with read error.
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0,
                                              BlockMigrationResult::kReadError);
  histogram_tester.ExpectBucketCount(block_result_histogram,
                                     BlockMigrationResult::kReadError, 1);

  // Retry block 0 and fail with write error.
  auto b0_retry1 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0_retry1.has_value());
  tracker.MarkMigrationAsCompletedAndUnlockIt(
      *b0_retry1, BlockMigrationResult::kWriteError);
  histogram_tester.ExpectBucketCount(block_result_histogram,
                                     BlockMigrationResult::kWriteError, 1);

  // Retry block 0 and fail with encryption error.
  auto b0_retry2 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0_retry2.has_value());
  tracker.MarkMigrationAsCompletedAndUnlockIt(
      *b0_retry2, BlockMigrationResult::kEncryptionError);
  histogram_tester.ExpectBucketCount(block_result_histogram,
                                     BlockMigrationResult::kEncryptionError, 1);

  // Retry block 0 and succeed.
  auto b0_retry3 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b0_retry3.has_value());
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b0_retry3,
                                              BlockMigrationResult::kSuccess);
  histogram_tester.ExpectBucketCount(block_result_histogram,
                                     BlockMigrationResult::kSuccess, 1);

  // Migration should still be incomplete.
  EXPECT_FALSE(tracker.IsMigrationComplete());
  histogram_tester.ExpectBucketCount(status_histogram,
                                     MigrationStatus::kCompleted, 0);

  // Migrate block 1 and succeed.
  auto b1 = tracker.GetNextBlockToMigrateAndLockIt();
  ASSERT_TRUE(b1.has_value());
  tracker.MarkMigrationAsCompletedAndUnlockIt(*b1,
                                              BlockMigrationResult::kSuccess);

  EXPECT_TRUE(tracker.IsMigrationComplete());
  histogram_tester.ExpectBucketCount(block_result_histogram,
                                     BlockMigrationResult::kSuccess, 2);
  histogram_tester.ExpectBucketCount(status_histogram,
                                     MigrationStatus::kStarted, 1);
  histogram_tester.ExpectBucketCount(status_histogram,
                                     MigrationStatus::kCompleted, 1);
}

TEST_F(PageMigrationTrackerTest, HistogramsEmptyTracker) {
  base::HistogramTester histogram_tester;

  PageMigrationTracker tracker(MigrationTarget::kTest, 0);
  EXPECT_TRUE(tracker.IsMigrationComplete());
  EXPECT_EQ(tracker.GetNextBlockToMigrateAndLockIt(), std::nullopt);

  // Empty tracker should not emit migration started or completed histograms.
  histogram_tester.ExpectTotalCount(GetStatusHistogramName(tracker.target()),
                                    0);
  histogram_tester.ExpectTotalCount(
      GetBlockResultHistogramName(tracker.target()), 0);
  histogram_tester.ExpectTotalCount(
      GetTotalBlocksHistogramName(tracker.target()), 0);
}

TEST_F(PageMigrationTrackerTest, ConcurrentMigrationHistograms) {
  base::HistogramTester histogram_tester;

  constexpr size_t kTotalPages = 1024;  // 16 blocks
  constexpr size_t kNumWorkers = 8;
  PageMigrationTracker tracker(MigrationTarget::kTest, kTotalPages);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(kNumWorkers, run_loop.QuitClosure());

  for (size_t i = 0; i < kNumWorkers; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](PageMigrationTracker* trk, base::RepeatingClosure done) {
              while (auto block = trk->GetNextBlockToMigrateAndLockIt()) {
                trk->MarkMigrationAsCompletedAndUnlockIt(
                    *block, BlockMigrationResult::kSuccess);
              }
              done.Run();
            },
            base::Unretained(&tracker), barrier));
  }

  run_loop.Run();

  EXPECT_TRUE(tracker.IsMigrationComplete());
  histogram_tester.ExpectBucketCount(GetStatusHistogramName(tracker.target()),
                                     MigrationStatus::kStarted, 1);
  histogram_tester.ExpectBucketCount(GetStatusHistogramName(tracker.target()),
                                     MigrationStatus::kCompleted, 1);
  histogram_tester.ExpectTotalCount(GetStatusHistogramName(tracker.target()),
                                    2);
  histogram_tester.ExpectUniqueSample(
      GetBlockResultHistogramName(tracker.target()),
      BlockMigrationResult::kSuccess, 16);
  histogram_tester.ExpectUniqueSample(
      GetTotalBlocksHistogramName(tracker.target()), 16, 1);
}

}  // namespace
}  // namespace sqlite_migration_vfs
