// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(DataTypeDebugInfoEmitterTest, ShouldEmitCommitsToUMAIfChanged) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  CommitCounters* counters = emitter.GetMutableCommitCounters();
  counters->num_deletion_commits_attempted += 3;
  counters->num_creation_commits_attempted += 2;
  counters->num_update_commits_attempted += 1;

  base::HistogramTester histogram_tester;
  emitter.EmitCommitCountersUpdate();
  EXPECT_EQ(
      3, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_DELETION=*/0));
  EXPECT_EQ(
      2, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_CREATION=*/1));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.BOOKMARK", /*LOCAL_UPDATE=*/2));
}

TEST(DataTypeDebugInfoEmitterTest, ShouldNotEmitCommitsToUMAIfNotChanged) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  base::HistogramTester histogram_tester;
  emitter.EmitCommitCountersUpdate();
  histogram_tester.ExpectTotalCount("Sync.ModelTypeEntityChange3.BOOKMARK", 0);
}

// Tests that at each EmitCommitCountersUpdate() call, only the changes since
// the last call to EmitCommitCountersUpdate() are reported to UMA.
TEST(DataTypeDebugInfoEmitterTest, ShouldEmitCommitsToUMAIncrementally) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  CommitCounters* counters = emitter.GetMutableCommitCounters();
  counters->num_deletion_commits_attempted += 3;
  counters->num_creation_commits_attempted += 2;
  counters->num_update_commits_attempted += 1;

  // First emission - tested in the test above.
  emitter.EmitCommitCountersUpdate();

  counters = emitter.GetMutableCommitCounters();
  counters->num_deletion_commits_attempted += 1;
  counters->num_creation_commits_attempted += 2;
  counters->num_update_commits_attempted += 3;

  // Test the second emission that it only reports the increment in counters.
  base::HistogramTester histogram_tester;
  emitter.EmitCommitCountersUpdate();
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_DELETION=*/0));
  EXPECT_EQ(
      2, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_CREATION=*/1));
  EXPECT_EQ(3, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.BOOKMARK", /*LOCAL_UPDATE=*/2));
}

TEST(DataTypeDebugInfoEmitterTest, ShouldEmitUpdatesToUMAIfChanged) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  UpdateCounters* counters = emitter.GetMutableUpdateCounters();
  counters->num_initial_updates_received += 5;
  counters->num_non_initial_updates_received += 3;
  counters->num_non_initial_tombstone_updates_received += 1;

  base::HistogramTester histogram_tester;
  emitter.EmitUpdateCountersUpdate();
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_DELETION=*/3));
  EXPECT_EQ(
      2, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
  EXPECT_EQ(
      5, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

TEST(DataTypeDebugInfoEmitterTest, ShouldNotEmitUpdatesToUMAIfNotChanged) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  base::HistogramTester histogram_tester;
  emitter.EmitUpdateCountersUpdate();
  histogram_tester.ExpectTotalCount("Sync.ModelTypeEntityChange3.BOOKMARK", 0);
}

// Tests that at each EmitUpdateCountersUpdate() call, only the changes since
// the last call to EmitUpdateCountersUpdate() are reported to UMA.
TEST(DataTypeDebugInfoEmitterTest, ShouldEmitUpdatesToUMAIncrementally) {
  DataTypeDebugInfoEmitter emitter(BOOKMARKS);

  UpdateCounters* counters = emitter.GetMutableUpdateCounters();
  counters->num_initial_updates_received += 5;
  counters->num_non_initial_updates_received += 3;
  counters->num_non_initial_tombstone_updates_received += 1;

  // First emission - tested in the test above.
  emitter.EmitUpdateCountersUpdate();

  counters = emitter.GetMutableUpdateCounters();
  counters->num_initial_updates_received += 4;
  counters->num_non_initial_updates_received += 3;
  counters->num_non_initial_tombstone_updates_received += 2;

  // Test the second emission that it only reports the increment in counters.
  base::HistogramTester histogram_tester;
  emitter.EmitUpdateCountersUpdate();
  EXPECT_EQ(
      2, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_DELETION=*/3));
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
  EXPECT_EQ(
      4, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

}  // namespace
}  // namespace syncer
