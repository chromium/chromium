// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background_snapshot_controller.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class BackgroundSnapshotControllerTest
    : public testing::Test,
      public BackgroundSnapshotController::Client {
 public:
  BackgroundSnapshotControllerTest();
  ~BackgroundSnapshotControllerTest() override;

  BackgroundSnapshotController* controller() { return controller_.get(); }
  int snapshot_count() { return snapshot_count_; }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // BackgroundSnapshotController::Client
  void StartSnapshot() override;

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);

 private:
  std::unique_ptr<BackgroundSnapshotController> controller_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  bool snapshot_started_;
  int snapshot_count_;
};

BackgroundSnapshotControllerTest::BackgroundSnapshotControllerTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      snapshot_started_(true),
      snapshot_count_(0) {}

BackgroundSnapshotControllerTest::~BackgroundSnapshotControllerTest() = default;

void BackgroundSnapshotControllerTest::SetUp() {
  controller_ =
      std::make_unique<BackgroundSnapshotController>(task_runner_, this, false);
  snapshot_started_ = true;
}

void BackgroundSnapshotControllerTest::TearDown() {
  controller_.reset();
}

void BackgroundSnapshotControllerTest::StartSnapshot() {
  snapshot_count_++;
}

void BackgroundSnapshotControllerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void BackgroundSnapshotControllerTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

TEST_F(BackgroundSnapshotControllerTest, OnLoad) {
  // Onload should make snapshot after its delay.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(BackgroundSnapshotControllerTest, Stop) {
  // OnDOM should make snapshot after a delay.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  controller()->Stop();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  // Should not start snapshots
  EXPECT_EQ(0, snapshot_count());
  // Also should not start snapshot.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_EQ(0, snapshot_count());
}

// This simulated a Reset while there is ongoing snapshot, which is reported
// as done later. That reporting should have no effect nor crash.
TEST_F(BackgroundSnapshotControllerTest, ClientReset) {
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());
  // This normally happens when navigation starts.
  controller()->Reset();
  // Next snapshot should be initiated when new document is loaded.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(2, snapshot_count());
}

}  // namespace offline_pages
