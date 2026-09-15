// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_deadline.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class SurfaceDependencyDeadlineTest : public testing::Test {
 public:
  SurfaceDependencyDeadlineTest() : deadline_(&test_clock_) {}
  ~SurfaceDependencyDeadlineTest() override { deadline_.Cancel(); }

 protected:
  base::SimpleTestTickClock test_clock_;
  SurfaceDependencyDeadline deadline_;
};

TEST_F(SurfaceDependencyDeadlineTest,
       GlobalDeadlineIgnoredWithPerDependencyDeadlines) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  // Global deadline is 20ms.
  FrameDeadline frame_deadline(start_time, 2u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  SurfaceId dep_id(kArbitraryFrameSinkId,
                   LocalSurfaceId(1, 1, base::UnguessableToken::Create()));

  deadline_.SetFrameDeadline(frame_deadline);
  // Dependency deadline is 100ms.
  deadline_.SetDependencyDeadlines(
      {{dep_id, start_time + base::Milliseconds(100)}});

  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 20ms: global deadline passes, but it is ignored because
  // kPerDependencyDeadlines is enabled. The dependency deadline is 100ms.
  test_clock_.Advance(base::Milliseconds(20));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 100ms: dependency deadline expires.
  test_clock_.Advance(base::Milliseconds(80));
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest, ActivatesEarlyWhenAllDependenciesExpire) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  // Global deadline is large (100ms).
  FrameDeadline frame_deadline(start_time, 10u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  SurfaceId dep1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  SurfaceId dep2(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, 1, base::UnguessableToken::Create()));

  deadline_.SetFrameDeadline(frame_deadline);
  deadline_.SetDependencyDeadlines({
      {dep1, start_time + base::Milliseconds(20)},
      {dep2, start_time + base::Milliseconds(30)},
  });

  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 10ms: neither dependency has passed.
  test_clock_.Advance(base::Milliseconds(10));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 25ms: dep1 has passed, but dep2 is still pending (until 30ms).
  test_clock_.Advance(base::Milliseconds(15));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 35ms: both dep1 and dep2 have passed, activating early before 100ms.
  test_clock_.Advance(base::Milliseconds(10));
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest,
       ActivatesEarlyWhenDependencyResolvedAndOtherExpires) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  // Global deadline is 100ms.
  FrameDeadline frame_deadline(start_time, 10u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  SurfaceId dep1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  SurfaceId dep2(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, 1, base::UnguessableToken::Create()));

  deadline_.SetFrameDeadline(frame_deadline);
  deadline_.SetDependencyDeadlines({
      {dep1, start_time + base::Milliseconds(20)},
      {dep2, start_time + base::Milliseconds(80)},
  });

  // At 10ms: dep2 is resolved early.
  test_clock_.Advance(base::Milliseconds(10));
  deadline_.OnActivationDependencyResolved(dep2);
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 25ms: dep1 has expired. Since dep2 was resolved, all remaining
  // dependency deadlines have passed.
  test_clock_.Advance(base::Milliseconds(15));
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest, DisabledFeatureOnlyUsesGlobalDeadline) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  FrameDeadline frame_deadline(start_time, 5u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);

  deadline_.SetFrameDeadline(frame_deadline);

  // At 40ms (before 50ms):
  test_clock_.Advance(base::Milliseconds(40));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 50ms (global deadline expires):
  test_clock_.Advance(base::Milliseconds(10));
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest, NoDeadlineSetReturnsTrue) {
  // When no deadline is set (deadline_ is std::nullopt), HasDeadlinePassed()
  // returns true.
  EXPECT_FALSE(deadline_.has_deadline());
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest, RespectsViewTransitionDeadline) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  // Global deadline is 100ms.
  FrameDeadline frame_deadline(start_time, 10u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  SurfaceId dep1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, 1, base::UnguessableToken::Create()));

  deadline_.SetFrameDeadline(frame_deadline);
  deadline_.SetDependencyDeadlines({
      {dep1, start_time + base::Milliseconds(20)},
  });
  // View transition deadline is 50ms.
  deadline_.SetViewTransitionDeadline(start_time + base::Milliseconds(50));

  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 10ms: neither dependency has passed.
  test_clock_.Advance(base::Milliseconds(10));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 25ms: dep1 has passed, but view transition deadline is still pending.
  test_clock_.Advance(base::Milliseconds(15));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 55ms: both dep1 and view transition deadline have passed, activating
  // early before 100ms.
  test_clock_.Advance(base::Milliseconds(30));
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest,
       ActivatesEarlyWhenViewTransitionResolvedEarly) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  // Global deadline is 100ms.
  FrameDeadline frame_deadline(start_time, 10u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  SurfaceId dep1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, 1, base::UnguessableToken::Create()));

  deadline_.SetFrameDeadline(frame_deadline);
  deadline_.SetDependencyDeadlines({
      {dep1, start_time + base::Milliseconds(20)},
  });
  // View transition deadline is 80ms.
  deadline_.SetViewTransitionDeadline(start_time + base::Milliseconds(80));

  // At 25ms: dep1 has passed, but view transition is still pending.
  test_clock_.Advance(base::Milliseconds(25));
  EXPECT_FALSE(deadline_.HasDeadlinePassed());

  // At 30ms: view transition is resolved early.
  test_clock_.Advance(base::Milliseconds(5));
  deadline_.SetViewTransitionDeadline(base::TimeTicks());
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

TEST_F(SurfaceDependencyDeadlineTest, CancelResetsViewTransitionDeadline) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPerDependencyDeadlines);

  base::TimeTicks start_time = test_clock_.NowTicks();
  FrameDeadline frame_deadline(start_time, 10u, base::Milliseconds(10),
                               /*use_default_lower_bound_deadline=*/false);
  deadline_.SetFrameDeadline(frame_deadline);
  deadline_.SetViewTransitionDeadline(start_time + base::Milliseconds(50));
  EXPECT_FALSE(deadline_.view_transition_deadline_for_testing().is_null());

  deadline_.Cancel();
  EXPECT_TRUE(deadline_.view_transition_deadline_for_testing().is_null());
  EXPECT_TRUE(deadline_.HasDeadlinePassed());
}

}  // namespace
}  // namespace viz
