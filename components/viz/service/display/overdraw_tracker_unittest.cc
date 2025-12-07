// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overdraw_tracker.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

using OverdrawTrackerTest = testing::Test;

AggregatedFrame MakeAggregatedFrame(const gfx::Rect& output_rect) {
  static AggregatedRenderPassId::Generator id_generator;
  AggregatedFrame frame;

  frame.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
  frame.render_pass_list.back()->SetNew(id_generator.GenerateNextId(),
                                        output_rect, output_rect,
                                        gfx::Transform());
  return frame;
}

void AppendDrawQuad(AggregatedFrame* frame,
                    const gfx::Rect& visible_rect,
                    const gfx::Transform& transform = gfx::Transform(),
                    const std::optional<gfx::Rect>& clip_rect = std::nullopt) {
  auto& root_render_pass = frame->render_pass_list.back();

  auto* sqs = root_render_pass->CreateAndAppendSharedQuadState();
  sqs->quad_to_target_transform = transform;
  sqs->clip_rect = clip_rect;

  auto* quad = root_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetAll(sqs, visible_rect, visible_rect, false, SkColor4f(), true);
}

TEST_F(OverdrawTrackerTest, OverdrawCalculations) {
  AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(20, 20));
  for (const auto& rect :
       {gfx::Rect(0, 0, 20, 20), gfx::Rect(10, 0, 10, 20),
        gfx::Rect(10, 10, 10, 10), gfx::Rect(5, 5, 10, 10)}) {
    AppendDrawQuad(&frame, rect);
  }

  EXPECT_EQ(OverdrawTracker::EstimateOverdraw(&frame), 2.0);
}

TEST_F(OverdrawTrackerTest, OverdrawCalculationsOfQuadsWithTransform) {
  {
    // Check that Transforms of quads are taken into account.
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(20, 20));
    AppendDrawQuad(&frame, gfx::Rect(10, 10), gfx::Transform::MakeScale(2));

    EXPECT_EQ(OverdrawTracker::EstimateOverdraw(&frame), 1.0);
  }

  {
    // Check that the quads with transforms that do not preserve axis alignment
    // are ignored.
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(20, 20));

    gfx::Transform rotation_transform;
    rotation_transform.Rotate(5.0f);
    AppendDrawQuad(&frame, gfx::Rect(10, 10), rotation_transform);

    EXPECT_EQ(OverdrawTracker::EstimateOverdraw(&frame), 0.0);
  }
}

TEST_F(OverdrawTrackerTest, OverdrawCalculationsOfQuadsWithClip) {
  {
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(20, 20));
    AppendDrawQuad(&frame, gfx::Rect(100, 100), gfx::Transform(),
                   gfx::Rect(20, 20));

    EXPECT_EQ(OverdrawTracker::EstimateOverdraw(&frame), 1.0f);
  }

  {
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(20, 20));
    AppendDrawQuad(&frame, gfx::Rect(100, 100), gfx::Transform(),
                   gfx::Rect(5, 5, 10, 10));

    EXPECT_EQ(OverdrawTracker::EstimateOverdraw(&frame), 0.25f);
  }
}

TEST_F(OverdrawTrackerTest, OverdrawRecording) {
  OverdrawTracker::Settings settings;
  settings.interval_length_in_seconds = 2;

  OverdrawTracker tracker(settings);
  base::TimeTicks start_time = tracker.start_time_for_testing();

  {
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(10, 10));
    AppendDrawQuad(&frame, gfx::Rect(10, 10));  // overdraw is 1.0f.

    // Record overdraw at the 0th interval.
    tracker.EstimateAndRecordOverdraw(&frame, start_time);

    auto data = tracker.TakeDataAsTimeSeries();
    EXPECT_THAT(data, testing::ElementsAreArray({1.0f}));
    tracker.Reset();
  }

  {
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(10, 10));
    AppendDrawQuad(&frame, gfx::Rect(10, 10));  // overdraw is 1.0f.

    // Record overdraw at the 0th interval.
    tracker.EstimateAndRecordOverdraw(&frame, start_time);
    AppendDrawQuad(&frame, gfx::Rect(5, 10));  // overdraw is 1.5f.

    // Update overdraw at the 0th interval.
    tracker.EstimateAndRecordOverdraw(&frame, start_time + base::Seconds(1));

    auto data = tracker.TakeDataAsTimeSeries();
    EXPECT_THAT(data, testing::ElementsAreArray(
                          {1.25f}));  // average overdraw is 1.25f.
    tracker.Reset();
  }

  {
    AggregatedFrame frame = MakeAggregatedFrame(gfx::Rect(10, 10));
    AppendDrawQuad(&frame, gfx::Rect(10, 10));  // overdraw is 1.0f.

    // Record overdraw at the 0th interval.
    tracker.EstimateAndRecordOverdraw(&frame, start_time);
    AppendDrawQuad(&frame, gfx::Rect(5, 10));  // overdraw is 1.5f.

    // Record overdraw at a later interval.
    tracker.EstimateAndRecordOverdraw(&frame, start_time + base::Seconds(8));

    auto data = tracker.TakeDataAsTimeSeries();
    EXPECT_THAT(data,
                testing::ElementsAreArray({1.0f, 0.0f, 0.0f, 0.0f, 1.5f}));
    tracker.Reset();
  }
}

TEST_F(OverdrawTrackerTest, RequestDataWithoutRecordingAnyFrame) {
  OverdrawTracker::Settings settings;
  OverdrawTracker tracker(settings);

  auto data = tracker.TakeDataAsTimeSeries();
  EXPECT_EQ(data.size(), 0u);
}

}  // namespace
}  // namespace viz
