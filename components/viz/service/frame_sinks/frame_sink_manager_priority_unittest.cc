// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_output_surface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr FrameSinkId kFrameSinkIdA(1, 1);
constexpr FrameSinkId kFrameSinkIdB(2, 1);
constexpr FrameSinkId kFrameSinkIdC(3, 1);

class FrameSinkManagerPriorityTest : public testing::Test {
 public:
  FrameSinkManagerPriorityTest() {
    feature_list_.InitAndEnableFeature(
        features::kThrottleFrameSinksOnInteraction);
    FrameSinkManagerImpl::InitParams init_params(&output_surface_provider_);
    manager_ = std::make_unique<FrameSinkManagerImpl>(std::move(init_params));
  }
  ~FrameSinkManagerPriorityTest() override = default;

  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id) {
    auto support = std::make_unique<CompositorFrameSinkSupport>(
        &mock_client_, manager_.get(), frame_sink_id, /*is_root=*/false);
    support->SetBeginFrameSource(&begin_frame_source_);
    // Use a fixed vsync to avoid rounding issues in tests.
    support->GetThrottlerForTesting().SetLastKnownVsync(base::Hertz(60),
                                                        base::Hertz(60));
    return support;
  }

  void SubmitFrame(CompositorFrameSinkSupport* support,
                   bool is_handling_interaction) {
    CompositorFrame frame = MakeDefaultCompositorFrame();
    frame.metadata.is_handling_interaction = is_handling_interaction;
    LocalSurfaceId local_surface_id(1, 1, base::UnguessableToken::Create());
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  base::TimeDelta GetThrottleInterval(CompositorFrameSinkSupport* support) {
    return support->GetThrottlerForTesting().begin_frame_interval();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestOutputSurfaceProvider output_surface_provider_;
  std::unique_ptr<FrameSinkManagerImpl> manager_;
  MockCompositorFrameSinkClient mock_client_;
  FakeExternalBeginFrameSource begin_frame_source_{0.f, false};
};

TEST_F(FrameSinkManagerPriorityTest, ThrottleUnimportantFrameSinks) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);

  // Initially no throttling.
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));

  // Submit interactive frame to A.
  SubmitFrame(support_a.get(), /*is_handling_interaction=*/true);

  // B should be throttled. A should not.
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));

  // Submit non-interactive frame to A.
  SubmitFrame(support_a.get(), /*is_handling_interaction=*/false);

  // Throttling should be removed.
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
}

TEST_F(FrameSinkManagerPriorityTest, MultipleInteractions) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  auto support_c = CreateCompositorFrameSinkSupport(kFrameSinkIdC);

  // A interacts. B and C throttled.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_c.get()));

  // B interacts too. C still throttled. A and B not.
  SubmitFrame(support_b.get(), true);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_c.get()));

  // A stops. B still interacting. C still throttled. A becomes throttled.
  SubmitFrame(support_a.get(), false);
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_c.get()));

  // B stops. All clear.
  SubmitFrame(support_b.get(), false);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_c.get()));
}

TEST_F(FrameSinkManagerPriorityTest, ExplicitThrottlingWinsIfSlower) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);

  // Throttle B explicitly to 10Hz (100ms).
  manager_->Throttle({kFrameSinkIdB}, base::Seconds(0.1));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_b.get()));

  // A interacts. Interaction implies 30Hz (33ms).
  // B is already at 100ms. 100ms > 33ms, so it should stay at 100ms.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_b.get()));

  // Throttle B explicitly to 60Hz (16ms).
  // Interaction implies 30Hz (33ms).
  // 33ms > 16ms, so it should be throttled to 33ms.
  manager_->Throttle({kFrameSinkIdB}, base::Seconds(0.016));
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));
}

TEST_F(FrameSinkManagerPriorityTest, ExplicitThrottlingOnInteractiveSink) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);

  // A interacts.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));

  // Throttle A explicitly to 1Hz.
  // Explicit throttling should override interaction status.
  manager_->Throttle({kFrameSinkIdA}, base::Seconds(1));
  EXPECT_EQ(base::Seconds(1), GetThrottleInterval(support_a.get()));
}

TEST_F(FrameSinkManagerPriorityTest, CapturedFrameSinksAreNotThrottled) {
  auto support_a =
      CreateCompositorFrameSinkSupport(kFrameSinkIdA);  // Interactive
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);  // Captured

  // Register B as captured.
  manager_->OnCaptureStarted(kFrameSinkIdB);

  // A interacts.
  SubmitFrame(support_a.get(), true);

  // A is interactive -> not throttled.
  // B is captured -> not throttled (even though A is interacting).
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));

  manager_->OnCaptureStopped(kFrameSinkIdB);

  // Now B should be throttled because A is still interacting.
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));
}

TEST_F(FrameSinkManagerPriorityTest, GlobalThrottlingWinsIfSlower) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);

  // Global throttle to 10Hz (100ms).
  manager_->StartThrottlingAllFrameSinks(base::Seconds(0.1));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_b.get()));

  // A interacts. Interaction implies 30Hz (33ms).
  // 100ms > 33ms, so it should stay at 100ms.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_b.get()));

  // Interaction stops. Should still be 100ms due to global throttle.
  SubmitFrame(support_a.get(), false);
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::Seconds(0.1), GetThrottleInterval(support_b.get()));
}

TEST_F(FrameSinkManagerPriorityTest, ThrottlingUpdatedOnSinkDestruction) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);

  // A interacts. B throttled.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));

  // Destroy A. Interaction should stop and B should no longer be throttled.
  support_a.reset();
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
}

TEST_F(FrameSinkManagerPriorityTest, MultipleCapturedSinks) {
  auto support_a = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  auto support_c = CreateCompositorFrameSinkSupport(kFrameSinkIdC);

  manager_->OnCaptureStarted(kFrameSinkIdB);
  manager_->OnCaptureStarted(kFrameSinkIdC);

  // A interacts. B and C captured, so NOT throttled.
  SubmitFrame(support_a.get(), true);
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_a.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_c.get()));

  // Stop capturing B. It should become throttled.
  manager_->OnCaptureStopped(kFrameSinkIdB);
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_b.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_c.get()));
}

TEST_F(FrameSinkManagerPriorityTest, ThrottleParentsOfInteractiveFrameSinks) {
  auto support_parent = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto support_child = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  auto support_other = CreateCompositorFrameSinkSupport(kFrameSinkIdC);

  // Register A as parent of B.
  manager_->RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);

  // B interacts.
  SubmitFrame(support_child.get(), true);

  // B is interactive -> not throttled.
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_child.get()));

  // A is parent of interactive sink -> throttled.
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_parent.get()));

  // C is unrelated -> throttled.
  EXPECT_EQ(base::Hertz(60) * 2, GetThrottleInterval(support_other.get()));

  // Stop interaction.
  SubmitFrame(support_child.get(), false);

  // All cleared.
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_child.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_parent.get()));
  EXPECT_EQ(base::TimeDelta(), GetThrottleInterval(support_other.get()));
}

}  // namespace
}  // namespace viz
