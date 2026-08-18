// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_eviction_manager.h"

#include <algorithm>
#include <vector>

#include "base/at_exit.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/features.h"
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

class FakeFrameEvictorClient : public FrameEvictorClient {
 public:
  FakeFrameEvictorClient() = default;
  ~FakeFrameEvictorClient() override = default;

  void EvictDelegatedFrame(const std::vector<SurfaceId>& surface_ids) override {
    evicted_ = true;
  }
  EvictIds CollectSurfaceIdsForEviction() const override { return EvictIds(); }
  SurfaceId GetCurrentSurfaceId() const override { return SurfaceId(); }
  SurfaceId GetPreNavigationSurfaceId() const override { return SurfaceId(); }

  bool evicted() const { return evicted_; }

 private:
  bool evicted_ = false;
};

}  // namespace

class FrameEvictionManagerTest
    // Cannot use base::WithFeatureOverride because of VizTestSuite setup.
    : public testing::TestWithParam<bool> {
 public:
  FrameEvictionManagerTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kScalableFrameEviction);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kScalableFrameEviction);
    }
  }

 private:
  // Required to force the `FrameEvictionManager` singleton to be destroyed and
  // re-created for each parameterized test run, because its constructor caches
  // the state of `features::kScalableFrameEviction`.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ShadowingAtExitManager shadowing_at_exit_manager_;
};

INSTANTIATE_TEST_SUITE_P(All, FrameEvictionManagerTest, testing::Bool());

TEST_P(FrameEvictionManagerTest, ScopedPause) {
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
    EXPECT_EQ(kFrames, std::ranges::count_if(
                           frames, &TestFrameEvictionManagerClient::has_frame));
  }

  // Frame eviction happens when |scoped_pause| goes out of scope.
  EXPECT_EQ(kMaxSavedFrames,
            std::ranges::count_if(frames,
                                  &TestFrameEvictionManagerClient::has_frame));
}

TEST_P(FrameEvictionManagerTest, PeriodicCulling) {
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

TEST_P(FrameEvictionManagerTest, ScalableEvictionLimits) {
  if (!GetParam()) {
    return;
  }

  base::TestMemoryConsumerRegistry test_registry;

  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();

  // Trigger 50% limit, which should bring us exactly to the baseline.
  test_registry.NotifyUpdateMemoryLimit(50);
  size_t baseline = manager->GetMaxNumberOfSavedFrames();

  // Trigger 100% limit, which should bring us to baseline * 2.
  test_registry.NotifyUpdateMemoryLimit(100);
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline * 2);

  // Trigger 25% limit, which should clamp to baseline.
  test_registry.NotifyUpdateMemoryLimit(25);
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline);

  // Trigger 75% limit, which should be baseline * 2 * 75 / 100.
  test_registry.NotifyUpdateMemoryLimit(75);
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline * 2 * 75 / 100);
}

TEST_P(FrameEvictionManagerTest, ScalableEvictionReleaseMemory) {
  if (!GetParam()) {
    return;
  }

  base::TestMemoryConsumerRegistry test_registry;
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();

  // Get the baseline.
  test_registry.NotifyUpdateMemoryLimit(50);
  size_t baseline = manager->GetMaxNumberOfSavedFrames();

  // Reset to 100% to start with 2x baseline capacity.
  test_registry.NotifyUpdateMemoryLimit(100);
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline * 2);

  // Add more frames than the capacity.
  size_t frames_to_add = baseline * 2 + 2;
  std::vector<TestFrameEvictionManagerClient> frames(frames_to_add);
  for (auto& frame : frames) {
    manager->AddFrame(&frame, false);
  }

  // Initial culling should keep baseline * 2 frames.
  EXPECT_EQ(baseline * 2,
            static_cast<size_t>(std::ranges::count_if(
                frames, &TestFrameEvictionManagerClient::has_frame)));

  // Lower limit to 50% (target capacity becomes baseline, but effective
  // capacity remains 2x baseline).
  test_registry.NotifyUpdateMemoryLimit(50);
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline * 2);

  // Verify we still have 2x baseline frames.
  EXPECT_EQ(baseline * 2,
            static_cast<size_t>(std::ranges::count_if(
                frames, &TestFrameEvictionManagerClient::has_frame)));

  // Notify release memory. Effective capacity drops to baseline, and culling
  // occurs.
  test_registry.NotifyReleaseMemory();
  EXPECT_EQ(manager->GetMaxNumberOfSavedFrames(), baseline);

  // Verify culling occurred down to baseline.
  EXPECT_EQ(baseline, static_cast<size_t>(std::ranges::count_if(
                          frames, &TestFrameEvictionManagerClient::has_frame)));
}

TEST_P(FrameEvictionManagerTest, FrameEvictorOptOutUnregistersFromManager) {
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  FakeFrameEvictorClient client;
  FrameEvictor evictor(&client);

  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());

  evictor.OnNewSurfaceEmbedded();
  evictor.SetVisible(true);
  EXPECT_EQ(1u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());

  // Opting out unregisters the frame from FrameEvictionManager.
  evictor.OptOutFrameEviction();
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());

  // Subsequent visibility changes do not re-register with FrameEvictionManager.
  evictor.SetVisible(false);
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());

  evictor.SetVisible(true);
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());
}

TEST_P(FrameEvictionManagerTest, FrameEvictorOptOutWhileUnlocked) {
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  FakeFrameEvictorClient client;
  FrameEvictor evictor(&client);

  evictor.OnNewSurfaceEmbedded();
  evictor.SetVisible(false);
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(1u, manager->GetUnlockedFramesCountForTesting());

  // Opting out while unlocked unregisters from FrameEvictionManager.
  evictor.OptOutFrameEviction();
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());
}

TEST_P(FrameEvictionManagerTest, FrameEvictorOptOutBeforeSurfaceEmbedded) {
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  FakeFrameEvictorClient client;
  FrameEvictor evictor(&client);

  evictor.OptOutFrameEviction();
  evictor.OnNewSurfaceEmbedded();
  evictor.SetVisible(true);
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());

  evictor.SetVisible(false);
  EXPECT_EQ(0u, manager->GetLockedFramesCountForTesting());
  EXPECT_EQ(0u, manager->GetUnlockedFramesCountForTesting());
}

TEST_P(FrameEvictionManagerTest, OptedOutFrameIsNotEvictedOnCulling) {
  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  manager->set_max_number_of_saved_frames(1);

  FakeFrameEvictorClient opted_out_client;
  FrameEvictor opted_out_evictor(&opted_out_client);
  opted_out_evictor.OnNewSurfaceEmbedded();
  opted_out_evictor.OptOutFrameEviction();

  // Add participating frames.
  TestFrameEvictionManagerClient participating_frame1{manager};
  manager->AddFrame(&participating_frame1, /*locked=*/false);
  EXPECT_TRUE(participating_frame1.has_frame());
  EXPECT_FALSE(opted_out_client.evicted());

  // Adding a second participating frame exceeds the limit (2 > 1 limit),
  // so participating_frame1 is culled.
  TestFrameEvictionManagerClient participating_frame2{manager};
  manager->AddFrame(&participating_frame2, /*locked=*/false);

  EXPECT_FALSE(participating_frame1.has_frame());
  EXPECT_TRUE(participating_frame2.has_frame());
  // The opted-out frame was not evicted.
  EXPECT_FALSE(opted_out_client.evicted());
}

}  // namespace viz
