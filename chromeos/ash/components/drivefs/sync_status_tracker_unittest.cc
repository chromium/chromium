// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

MATCHER_P(MatchesStatusAndProgress, value, "") {
  *result_listener << "where the progress difference is "
                   << std::abs(arg.progress - value.progress);
  return arg.status == value.status &&
         std::abs(arg.progress - value.progress) < 1e-4;
}

class SyncStatusTrackerTest : public testing::Test {
 public:
  SyncStatusTrackerTest() = default;

  SyncStatusTrackerTest(const SyncStatusTrackerTest&) = delete;
  SyncStatusTrackerTest& operator=(const SyncStatusTrackerTest&) = delete;

  SyncStatus GetSyncStatus(SyncStatusTracker& tracker,
                           const std::string& path) {
    return tracker.GetSyncStatusForPath(base::FilePath(path)).status;
  }

  SyncStatusAndProgress GetSyncStatusAndProgress(SyncStatusTracker& tracker,
                                                 const std::string& path) {
    return tracker.GetSyncStatusForPath(base::FilePath(path));
  }

  void AddSyncStatus(SyncStatusTracker& tracker,
                     const int64_t id,
                     const std::string& path,
                     SyncStatus status) {
    return tracker.AddSyncStatusForPath(id, base::FilePath(path), status, 0);
  }

  void AddSyncStatusAndProgress(SyncStatusTracker& tracker,
                                const int64_t id,
                                const std::string& path,
                                SyncStatus status,
                                float progress) {
    return tracker.AddSyncStatusForPath(id, base::FilePath(path), status,
                                        progress);
  }
};

TEST_F(SyncStatusTrackerTest, PathReturnsValueForLeafAndAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, ErrorTakesPrecedenceInAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c", SyncStatus::kInProgress);
  AddSyncStatus(tracker, 1, "/a/b/d", SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/a"), SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/"), SyncStatus::kError);
}

TEST_F(SyncStatusTrackerTest, PathsNotInTrackerReturnNotFound) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/d"), SyncStatus::kNotFound);
}

TEST_F(SyncStatusTrackerTest, RemovingAPathRemovesSingleUseAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c/f", SyncStatus::kInProgress);
  AddSyncStatus(tracker, 1, "/a/b/d", SyncStatus::kInProgress);
  AddSyncStatus(tracker, 2, "/a/b/e", SyncStatus::kInProgress);

  tracker.RemovePath(0, base::FilePath("/a/b/c/f"));
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/f"), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, OnlyLeafPathsCanBeRemoved) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c/d", SyncStatus::kInProgress);

  tracker.RemovePath(1, base::FilePath("/a/b/c"));
  tracker.RemovePath(2, base::FilePath("/a/b"));
  tracker.RemovePath(3, base::FilePath("/a"));

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, Utf8PathsAreSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/日本", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/日本"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, DeletingNonexistingPathIsNoOp) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c/d", SyncStatus::kInProgress);

  tracker.RemovePath(1, base::FilePath("/a/b/c/d/e"));

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, AddingExistingPathReplacesStatus) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "/a/b/c/d", SyncStatus::kInProgress);
  AddSyncStatus(tracker, 1, "/a/b/c/d", SyncStatus::kError);

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kError);
}

TEST_F(SyncStatusTrackerTest, MalformedPathsAreSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "////", SyncStatus::kInProgress);

  EXPECT_EQ(GetSyncStatus(tracker, "////"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, RelativePathsAreNotSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, 0, "./..", SyncStatus::kInProgress);
  AddSyncStatus(tracker, 1, "../", SyncStatus::kInProgress);

  EXPECT_EQ(GetSyncStatus(tracker, "./.."), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "../"), SyncStatus::kNotFound);
}

TEST_F(SyncStatusTrackerTest, MovingFileRemovesOldPath) {
  SyncStatusTracker tracker;

  AddSyncStatusAndProgress(tracker, 0, "/a/b/c/d", SyncStatus::kInProgress,
                           0.1);
  AddSyncStatusAndProgress(tracker, 1, "/a/b/c/e", SyncStatus::kQueued, 0);
  // Rename /a/b/c/d to /a/b/c/f.
  AddSyncStatusAndProgress(tracker, 0, "/a/b/c/f", SyncStatus::kInProgress,
                           0.5);

  // Old path is removed.
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b/c/d"),
              MatchesStatusAndProgress(SyncStatusAndProgress::kNotFound));
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b/c/e"),
              MatchesStatusAndProgress(SyncStatusAndProgress::kQueued));
  // New path is tracked.
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b/c/f"),
              MatchesStatusAndProgress(
                  SyncStatusAndProgress{SyncStatus::kInProgress, 0.5}));

  EXPECT_EQ(tracker.LeafCount(), 2u);
}

TEST_F(SyncStatusTrackerTest, MovingFileRemovesOldPathAndParents) {
  SyncStatusTracker tracker;

  AddSyncStatusAndProgress(tracker, 0, "/a/b/c/d", SyncStatus::kInProgress,
                           0.1);
  // Rename /a/b/c/d to /a/d.
  AddSyncStatusAndProgress(tracker, 0, "/a/d", SyncStatus::kInProgress, 0.2);

  // Old path is removed along with any childless parents.
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b/c/d"),
              MatchesStatusAndProgress(SyncStatusAndProgress::kNotFound));
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b/c"),
              MatchesStatusAndProgress(SyncStatusAndProgress::kNotFound));
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/b"),
              MatchesStatusAndProgress(SyncStatusAndProgress::kNotFound));
  // New path is tracked.
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a/d"),
              MatchesStatusAndProgress(
                  SyncStatusAndProgress{SyncStatus::kInProgress, 0.2}));
  EXPECT_THAT(GetSyncStatusAndProgress(tracker, "/a"),
              MatchesStatusAndProgress(
                  SyncStatusAndProgress{SyncStatus::kInProgress, -1}));

  EXPECT_EQ(tracker.LeafCount(), 1u);
}

}  // namespace
}  // namespace drivefs
