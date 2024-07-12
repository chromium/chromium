// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_metadata.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_common_export.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/latency/latency_info.h"

namespace viz {
namespace {

bool AreBeginFrameAcksEqual(const BeginFrameAck& a, const BeginFrameAck& b) {
  return a.frame_id == b.frame_id && a.trace_id == b.trace_id &&
         a.has_damage == b.has_damage &&
         a.preferred_frame_interval == b.preferred_frame_interval;
}

bool AreLatencyInfosEqual(const ui::LatencyInfo& a, const ui::LatencyInfo& b) {
  return a.began() == b.began() && a.terminated() == b.terminated() &&
         a.coalesced() == b.coalesced() && a.trace_id() == b.trace_id() &&
         a.gesture_scroll_id() == b.gesture_scroll_id();
}

bool AreDelegatedInkMetadataEqual(const gfx::DelegatedInkMetadata& a,
                                  const gfx::DelegatedInkMetadata& b) {
  return a.point() == b.point() && a.diameter() == b.diameter() &&
         a.color() == b.color() && a.timestamp() == b.timestamp() &&
         a.presentation_area() == b.presentation_area() &&
         a.frame_time() == b.frame_time() && a.is_hovering() == b.is_hovering();
}

bool AreTransitionDirectivesEqual(const CompositorFrameTransitionDirective& a,
                                  const CompositorFrameTransitionDirective& b) {
  return a.sequence_id() == b.sequence_id() && a.type() == b.type();
}

TEST(CompositorFrameMetadata, Clone) {
  const FrameSinkId frame_sink_id(1, 2);
  const LocalSurfaceId local_id1(1, base::UnguessableToken::Create());
  const LocalSurfaceId local_id2(2, base::UnguessableToken::Create());

  CompositorFrameMetadata metadata;
  metadata.device_scale_factor = 12.3f;
  metadata.root_scroll_offset = gfx::PointF(4.f, 5.f);
  metadata.page_scale_factor = 6.7f;
  metadata.scrollable_viewport_size = gfx::SizeF(89.0f, 12.3f);
  metadata.content_color_usage = gfx::ContentColorUsage::kHDR;
  metadata.may_contain_video = true;
  metadata.is_handling_interaction = true;
  metadata.root_background_color = SkColors::kBlue;
  metadata.latency_info.emplace_back();
  metadata.referenced_surfaces.emplace_back(
      SurfaceId(frame_sink_id, local_id1), SurfaceId(frame_sink_id, local_id2));
  metadata.activation_dependencies.emplace_back(
      SurfaceId(frame_sink_id, local_id1));
  metadata.deadline = FrameDeadline(base::TimeTicks() + base::Seconds(123), 15,
                                    base::Milliseconds(16), true);
  metadata.begin_frame_ack = BeginFrameAck(999, 888, true, 777);
  metadata.begin_frame_ack.preferred_frame_interval.emplace(
      base::Milliseconds(11));
  metadata.frame_token = 6;
  metadata.send_frame_token_to_embedder = true;
  metadata.min_page_scale_factor = 123.3f;
  metadata.top_controls_visible_height.emplace(0.5);
  metadata.display_transform_hint = gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  metadata.delegated_ink_metadata = std::make_unique<gfx::DelegatedInkMetadata>(
      gfx::PointF(88.8, 44.4), 1.f, SK_ColorRED,
      base::TimeTicks() + base::Seconds(125), gfx::RectF(1, 2, 3, 4), true);
  metadata.transition_directives.emplace_back(
      CompositorFrameTransitionDirective::CreateSave(
          blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false, 4u,
          {}, {}));

  CompositorFrameMetadata clone = metadata.Clone();
  EXPECT_FLOAT_EQ(clone.device_scale_factor, metadata.device_scale_factor);
  EXPECT_EQ(clone.root_scroll_offset, metadata.root_scroll_offset);
  EXPECT_FLOAT_EQ(clone.page_scale_factor, metadata.page_scale_factor);
  EXPECT_EQ(clone.scrollable_viewport_size, metadata.scrollable_viewport_size);
  EXPECT_EQ(clone.content_color_usage, metadata.content_color_usage);
  EXPECT_EQ(clone.may_contain_video, metadata.may_contain_video);
  EXPECT_EQ(clone.is_handling_interaction, metadata.is_handling_interaction);
  EXPECT_EQ(clone.root_background_color, metadata.root_background_color);

  EXPECT_EQ(clone.latency_info.size(), metadata.latency_info.size());
  EXPECT_TRUE(
      AreLatencyInfosEqual(clone.latency_info[0], metadata.latency_info[0]));

  EXPECT_EQ(clone.referenced_surfaces, metadata.referenced_surfaces);
  EXPECT_EQ(clone.activation_dependencies, metadata.activation_dependencies);
  EXPECT_EQ(clone.deadline, metadata.deadline);

  EXPECT_TRUE(
      AreBeginFrameAcksEqual(clone.begin_frame_ack, metadata.begin_frame_ack));

  EXPECT_EQ(clone.frame_token, metadata.frame_token);
  EXPECT_EQ(clone.send_frame_token_to_embedder,
            metadata.send_frame_token_to_embedder);
  EXPECT_FLOAT_EQ(clone.min_page_scale_factor, metadata.min_page_scale_factor);
  EXPECT_EQ(clone.top_controls_visible_height,
            metadata.top_controls_visible_height);
  EXPECT_FLOAT_EQ(*clone.top_controls_visible_height,
                  *metadata.top_controls_visible_height);
  EXPECT_EQ(clone.display_transform_hint, metadata.display_transform_hint);

  EXPECT_EQ(!!clone.delegated_ink_metadata, !!metadata.delegated_ink_metadata);
  EXPECT_TRUE(AreDelegatedInkMetadataEqual(*clone.delegated_ink_metadata,
                                           *metadata.delegated_ink_metadata));

  EXPECT_EQ(clone.transition_directives.size(),
            metadata.transition_directives.size());
  EXPECT_TRUE(AreTransitionDirectivesEqual(clone.transition_directives[0],
                                           metadata.transition_directives[0]));
}

}  // namespace
}  // namespace viz
