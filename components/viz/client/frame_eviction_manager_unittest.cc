// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_eviction_manager.h"

#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

class TestFrameEvictionManagerClient : public FrameEvictionManagerClient {
 public:
  TestFrameEvictionManagerClient() = default;
  explicit TestFrameEvictionManagerClient(FrameEvictionManager* manager)
      : manager_(manager) {}

  TestFrameEvictionManagerClient(const TestFrameEvictionManagerClient&) =
      delete;
  TestFrameEvictionManagerClient& operator=(
      const TestFrameEvictionManagerClient&) = delete;

  ~TestFrameEvictionManagerClient() override {
    if (has_frame_)
      manager_->RemoveFrame(this);
  }

  // FrameEvictionManagerClient:
  void EvictCurrentFrame() override {
    manager_->RemoveFrame(this);
    has_frame_ = false;
  }

  bool has_frame() const { return has_frame_; }

 private:
  raw_ptr<FrameEvictionManager> manager_ = FrameEvictionManager::GetInstance();
  bool has_frame_ = true;
};

}  // namespace

class FrameEvictionManagerTest : public testing::Test {};

TEST_F(FrameEvictionManagerTest, ScopedPause) {
  constexpr int kMaxSavedFrames = 1;
  constexpr int kFrames = 2;

  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  manager->set_max_number_of_saved_frames(kMaxSavedFrames);

  std::vector<TestFrameEvictionManagerClient> frames(kFrames);
  {
    FrameEvictionManager::ScopedPause scoped_pause;

    for (auto& frame : frames)
      manager->AddFrame(&frame, /*locked=*/false);

    // All frames stays because |scoped_pause| holds off frame eviction.
    EXPECT_EQ(kFrames, base::ranges::count_if(
                           frames, &TestFrameEvictionManagerClient::has_frame));
  }

  // Frame eviction happens when |scoped_pause| goes out of scope.
  EXPECT_EQ(kMaxSavedFrames,
            base::ranges::count_if(frames,
                                   &TestFrameEvictionManagerClient::has_frame));
}

TEST_F(FrameEvictionManagerTest, PeriodicCulling) {
  // Cannot use a TaskEnvironment as there is already one which is not using
  // MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  FrameEvictionManager manager;
  manager.set_max_number_of_saved_frames(5);
  manager.SetOverridesForTesting(task_runner, task_runner->GetMockTickClock());

  TestFrameEvictionManagerClient frame1{&manager}, frame2{&manager},
      frame3{&manager};
  manager.AddFrame(&frame1, false);
  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay / 10);
  manager.AddFrame(&frame2, true);
  manager.AddFrame(&frame3, false);

  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay);
  EXPECT_FALSE(frame1.has_frame());
  EXPECT_TRUE(frame2.has_frame());
  EXPECT_TRUE(frame3.has_frame());  // Too early for this one.
  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay);
  EXPECT_FALSE(frame3.has_frame());

  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay / 2);
  manager.UnlockFrame(&frame2);
  EXPECT_TRUE(frame2.has_frame());

  // Pause prevents eviction, but not rescheduling the task. Not using
  // ScopedPause because it impacts the singleton.
  manager.Pause();
  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay / 2);
  EXPECT_TRUE(frame2.has_frame());
  manager.Unpause();

  task_runner->FastForwardBy(FrameEvictionManager::kPeriodicCullingDelay);
  EXPECT_FALSE(frame2.has_frame());
}

TEST_F(FrameEvictionManagerTest, MemoryPressure) {
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();

  manager->set_max_number_of_saved_frames(5);
  TestFrameEvictionManagerClient frame1, frame2;
  manager->AddFrame(&frame1, false);
  manager->AddFrame(&frame2, false);

  // Critical memory pressure culls all unlocked frames.
  manager->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(frame1.has_frame());
  EXPECT_FALSE(frame2.has_frame());
}

}  // namespace viz
