// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/snapshot_controller.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class SnapshotControllerTest : public testing::Test,
                               public SnapshotController::Client {
 public:
  SnapshotControllerTest();
  ~SnapshotControllerTest() override;

  SnapshotController* controller() { return controller_.get(); }
  int snapshot_count() { return snapshot_count_; }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // SnapshotController::Client
  void StartSnapshot() override;

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);

 private:
  std::unique_ptr<SnapshotController> controller_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  bool snapshot_started_;
  int snapshot_count_;
};

SnapshotControllerTest::SnapshotControllerTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      snapshot_started_(true),
      snapshot_count_(0) {}

SnapshotControllerTest::~SnapshotControllerTest() = default;

void SnapshotControllerTest::SetUp() {
  controller_ = std::make_unique<SnapshotController>(task_runner_, this);
  snapshot_started_ = true;
}

void SnapshotControllerTest::TearDown() {
  controller_.reset();
}

void SnapshotControllerTest::StartSnapshot() {
  snapshot_count_++;
}

void SnapshotControllerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void SnapshotControllerTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

TEST_F(SnapshotControllerTest, OnLoad) {
  // Onload should make snapshot after its delay.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnDocumentAvailable) {
  EXPECT_GT(controller()->GetDelayAfterDocumentAvailableForTest(), 0LL);
  // OnDOM should make snapshot after a delay.
  controller()->PrimaryMainDocumentElementAvailable();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnLoadSnapshotIsTheLastOne) {
  // This test assumes DocumentAvailable delay is longer than OnLoadCompleted.
  EXPECT_GT(controller()->GetDelayAfterDocumentAvailableForTest(),
            controller()->GetDelayAfterDocumentOnLoadCompletedForTest());
  // OnDOM should make snapshot after a delay.
  controller()->PrimaryMainDocumentElementAvailable();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  // Advance time to OnLoadCompleted delay to trigger snapshot.
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());
  // Report that snapshot is completed.
  controller()->PendingSnapshotCompleted();
  // Even though previous snapshot is completed, new one should not start
  // when this DocumentAvailable delay expires.
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnLoadSnapshotAfterLongDelay) {
  // OnDOM should make snapshot after a delay.
  controller()->PrimaryMainDocumentElementAvailable();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
  // Report that snapshot is completed.
  controller()->PendingSnapshotCompleted();
  // OnLoad should make 2nd snapshot after its delay.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(2, snapshot_count());
}

TEST_F(SnapshotControllerTest, Stop) {
  // OnDOM should make snapshot after a delay.
  controller()->PrimaryMainDocumentElementAvailable();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  controller()->Stop();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // Should not start snapshots
  EXPECT_EQ(0, snapshot_count());
  // Also should not start snapshot.
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_EQ(0, snapshot_count());
}

TEST_F(SnapshotControllerTest, ClientReset) {
  controller()->PrimaryMainDocumentElementAvailable();

  controller()->Reset();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(0, snapshot_count());
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());

  controller()->Reset();
  controller()->PrimaryMainDocumentElementAvailable();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(2, snapshot_count());
}

// This simulated a Reset while there is ongoing snapshot, which is reported
// as done later. That reporting should have no effect nor crash.
TEST_F(SnapshotControllerTest, ClientResetWhileSnapshotting) {
  controller()->DocumentOnLoadCompletedInPrimaryMainFrame();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentOnLoadCompletedForTest()));
  EXPECT_EQ(1, snapshot_count());
  // This normally happens when navigation starts.
  controller()->Reset();
  controller()->PendingSnapshotCompleted();
  // Next snapshot should be initiated when new document is loaded.
  controller()->PrimaryMainDocumentElementAvailable();
  FastForwardBy(base::Milliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(2, snapshot_count());
}

}  // namespace offline_pages
