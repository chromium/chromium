// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

class SyncStatusTrackerTest : public testing::Test {
 public:
  SyncStatusTrackerTest() = default;

  SyncStatusTrackerTest(const SyncStatusTrackerTest&) = delete;
  SyncStatusTrackerTest& operator=(const SyncStatusTrackerTest&) = delete;

  SyncStatus GetSyncStatus(SyncStatusTracker& tracker,
                           const std::string& path) {
    return tracker.GetSyncStatusForPath(base::FilePath(path));
  }

  void AddSyncStatus(SyncStatusTracker& tracker,
                     const std::string& path,
                     SyncStatus status) {
    return tracker.AddSyncStatusForPath(base::FilePath(path), status);
  }
};

TEST_F(SyncStatusTrackerTest, PathReturnsValueForLeafAndAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, ErrorTakesPrecedenceInAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c", SyncStatus::kInProgress);
  AddSyncStatus(tracker, "/a/b/d", SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/a"), SyncStatus::kError);
  EXPECT_EQ(GetSyncStatus(tracker, "/"), SyncStatus::kError);
}

TEST_F(SyncStatusTrackerTest, PathsNotInTrackerReturnNotFound) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/d"), SyncStatus::kNotFound);
}

TEST_F(SyncStatusTrackerTest, RemovingAPathRemovesSingleUseAncestors) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c/f", SyncStatus::kInProgress);
  AddSyncStatus(tracker, "/a/b/d", SyncStatus::kInProgress);
  AddSyncStatus(tracker, "/a/b/e", SyncStatus::kInProgress);

  tracker.RemovePath(base::FilePath("/a/b/c/f"));
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/f"), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c"), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, OnlyLeafPathsCanBeRemoved) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c/d", SyncStatus::kInProgress);

  tracker.RemovePath(base::FilePath("/a/b/c"));
  tracker.RemovePath(base::FilePath("/a/b"));
  tracker.RemovePath(base::FilePath("/a"));

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, Utf8PathsAreSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/日本", SyncStatus::kInProgress);
  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/日本"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, DeletingNonexistingPathIsNoOp) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c/d", SyncStatus::kInProgress);

  tracker.RemovePath(base::FilePath("/a/b/c/d/e"));

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, AddingExistingPathReplacesStatus) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "/a/b/c/d", SyncStatus::kInProgress);
  AddSyncStatus(tracker, "/a/b/c/d", SyncStatus::kError);

  EXPECT_EQ(GetSyncStatus(tracker, "/a/b/c/d"), SyncStatus::kError);
}

TEST_F(SyncStatusTrackerTest, MalformedPathsAreSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "////", SyncStatus::kInProgress);

  EXPECT_EQ(GetSyncStatus(tracker, "////"), SyncStatus::kInProgress);
}

TEST_F(SyncStatusTrackerTest, RelativePathsAreNotSupported) {
  SyncStatusTracker tracker;

  AddSyncStatus(tracker, "./..", SyncStatus::kInProgress);
  AddSyncStatus(tracker, "../", SyncStatus::kInProgress);

  EXPECT_EQ(GetSyncStatus(tracker, "./.."), SyncStatus::kNotFound);
  EXPECT_EQ(GetSyncStatus(tracker, "../"), SyncStatus::kNotFound);
}

}  // namespace
}  // namespace drivefs
