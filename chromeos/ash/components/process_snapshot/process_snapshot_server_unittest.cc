// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/process_snapshot/process_snapshot_server.h"

#include <memory>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// -----------------------------------------------------------------------------
// TestObserver:

class TestObserver : public ProcessSnapshotServer::Observer {
 public:
  explicit TestObserver(base::TimeDelta desired_refresh_time)
      : ProcessSnapshotServer::Observer(desired_refresh_time) {}
  ~TestObserver() override = default;

  base::Time last_refresh_time() const { return last_refresh_time_; }

  void WaitForRefresh() {
    if (!refresh_received_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    refresh_received_ = false;
  }

  // ProcessSnapshotServer::Observer:
  void OnProcessSnapshotRefreshed(
      const base::ProcessIterator::ProcessEntries& snapshot) override {
    last_refresh_time_ = base::Time::Now();
    refresh_received_ = true;

    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  base::Time last_refresh_time_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool refresh_received_ = false;
};

// -----------------------------------------------------------------------------
// ProcessSnapshotServerTest:

class ProcessSnapshotServerTest : public testing::Test {
 public:
  ProcessSnapshotServerTest() = default;
  ProcessSnapshotServerTest(const ProcessSnapshotServerTest&) = delete;
  ProcessSnapshotServerTest& operator=(const ProcessSnapshotServerTest&) =
      delete;
  ~ProcessSnapshotServerTest() override = default;

  ProcessSnapshotServer* server() const { return ProcessSnapshotServer::Get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ProcessSnapshotServerTest, FirstObserverTriggersImmediateRefresh) {
  constexpr base::TimeDelta kDesiredDelay = base::Seconds(20);
  TestObserver observer(kDesiredDelay);

  server()->AddObserver(&observer);
  observer.WaitForRefresh();
  EXPECT_LT(base::Time::Now() - observer.last_refresh_time(), kDesiredDelay);

  server()->RemoveObserver(&observer);
}

TEST_F(ProcessSnapshotServerTest, AddRemoveObservers) {
  constexpr base::TimeDelta kDelay1 = base::Seconds(10);
  constexpr base::TimeDelta kDelay2 = base::Seconds(5);
  constexpr base::TimeDelta kDelay3 = base::Seconds(20);
  TestObserver observer1(kDelay1);
  TestObserver observer2(kDelay2);
  TestObserver observer3(kDelay3);

  server()->AddObserver(&observer1);
  const auto& timer = server()->GetTimerForTesting();
  EXPECT_TRUE(timer.IsRunning());
  EXPECT_EQ(kDelay1, timer.GetCurrentDelay());

  // Adding an observer with a smaller delay updates the timer.
  server()->AddObserver(&observer2);
  EXPECT_EQ(kDelay2, timer.GetCurrentDelay());

  // Adding an observer with a larger delay should not change the timer delay.
  server()->AddObserver(&observer3);
  EXPECT_EQ(kDelay2, timer.GetCurrentDelay());

  // Removing observers with delay larger than the minimum one shouldn't change
  // the timer delay.
  server()->RemoveObserver(&observer3);
  EXPECT_EQ(kDelay2, timer.GetCurrentDelay());

  // Removing the observer with the minimum delay will pick the next minimum.
  server()->RemoveObserver(&observer2);
  EXPECT_EQ(kDelay1, timer.GetCurrentDelay());

  // Removing the last observer should stop the timer.
  server()->RemoveObserver(&observer1);
  EXPECT_FALSE(timer.IsRunning());
}

}  // namespace
}  // namespace ash
