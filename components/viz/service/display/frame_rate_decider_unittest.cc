// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_rate_decider.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_manager_delegate.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/stub_surface_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

class FrameRateDeciderTest : public testing::Test,
                             public FrameRateDecider::Client,
                             public SurfaceManagerDelegate {
 public:
  FrameRateDeciderTest() : frame_(MakeDefaultCompositorFrame()) {}
  ~FrameRateDeciderTest() override = default;

  void SetUp() override {
    surface_manager_ = std::make_unique<SurfaceManager>(
        this, /*activation_deadline_in_frames=*/absl::nullopt,
        /*max_uncommitted_frames=*/0);
    bool hw_support_for_multiple_refresh_rates = true;
    frame_rate_decider_ = std::make_unique<FrameRateDecider>(
        surface_manager_.get(), this, hw_support_for_multiple_refresh_rates,
        false);
    frame_rate_decider_->set_min_num_of_frames_to_toggle_interval_for_testing(
        0u);
  }

  void TearDown() override {
    frame_rate_decider_.reset();
    surface_manager_.reset();
  }

  // FrameRateDecider::Client implementation.
  void SetPreferredFrameInterval(base::TimeDelta interval) override {
    display_interval_ = interval;
  }
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) override {
    if (type) {
      auto it = frame_sink_types_.find(id);
      *type = it == frame_sink_types_.end()
                  ? mojom::CompositorFrameSinkType::kMediaStream
                  : it->second;
    }
    return preferred_intervals_[id];
  }

  // SurfaceManagerDelegate implementation.
  base::StringPiece GetFrameSinkDebugLabel(
      const FrameSinkId& frame_sink_id) const override {
    return base::StringPiece();
  }
  void AggregatedFrameSinksChanged() override {}

 protected:
  base::WeakPtr<SurfaceClient> surface_client() {
    return surface_client_.weak_factory.GetWeakPtr();
  }

  Surface* CreateAndDrawSurface(
      const FrameSinkId& frame_sink_id,
      LocalSurfaceId local_surface_id =
          LocalSurfaceId(1u, base::UnguessableToken::Create())) {
    SurfaceId surface_id(frame_sink_id, local_surface_id);
    SurfaceInfo surface_info(surface_id, frame_.device_scale_factor(),
                             frame_.size_in_pixels());
    auto* surface =
        surface_manager_->CreateSurface(surface_client(), surface_info);

    {
      FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
      frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
    }

    return surface;
  }

  void UpdateFrame(Surface* surface) {
    uint64_t frame_index = surface->GetActiveFrameIndex() + 1u;
    ASSERT_TRUE(surface->QueueFrame(MakeDefaultCompositorFrame(), frame_index,
                                    base::ScopedClosureRunner()));
    surface->ActivatePendingFrameForDeadline();
    ASSERT_EQ(surface->GetActiveFrameIndex(), frame_index);
  }

  base::TimeDelta display_interval_;
  base::flat_map<FrameSinkId, base::TimeDelta> preferred_intervals_;
  base::flat_map<FrameSinkId, mojom::CompositorFrameSinkType> frame_sink_types_;

  std::unique_ptr<SurfaceManager> surface_manager_;
  std::unique_ptr<FrameRateDecider> frame_rate_decider_;

  CompositorFrame frame_;
  StubSurfaceClient surface_client_;
};

TEST_F(FrameRateDeciderTest, ActiveSurfaceTrackingFrameIndexChange) {
  const FrameSinkId frame_sink_id(1u, 1u);
  const base::TimeDelta preferred_interval = base::Seconds(1);
  preferred_intervals_[frame_sink_id] = preferred_interval;

  const std::vector<base::TimeDelta> supported_intervals = {
      preferred_interval / 2, preferred_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  auto* surface = CreateAndDrawSurface(frame_sink_id);
  EXPECT_EQ(display_interval_, preferred_interval);

  // Do a draw with the same surface and same CompositorFrame. Its assumed that
  // the surface is not being updated and we retain the current state.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
  }
  EXPECT_EQ(display_interval_, preferred_interval);

  // Submit a new frame to this surface and draw again. The interval should be
  // set to the surface's preferred rate.
  UpdateFrame(surface);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
  }
  EXPECT_EQ(display_interval_, preferred_interval);
}

TEST_F(FrameRateDeciderTest, ActiveSurfaceTrackingSurfaceIdChange) {
  const FrameSinkId frame_sink_id(1u, 1u);
  const base::TimeDelta preferred_interval = base::Seconds(1);
  preferred_intervals_[frame_sink_id] = preferred_interval;

  const std::vector<base::TimeDelta> supported_intervals = {
      preferred_interval / 2, preferred_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  auto* surface = CreateAndDrawSurface(frame_sink_id);
  EXPECT_EQ(display_interval_, preferred_interval);

  // Do a draw with the same surface and same CompositorFrame. Its assumed that
  // no content changed onscreen and we retain the current state.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
  }
  EXPECT_EQ(display_interval_, preferred_interval);

  // Create a new surface with the same frame sink id. The interval should be
  // set to the surface's preferred rate.
  LocalSurfaceId prev_surface_id = surface->surface_id().local_surface_id();
  LocalSurfaceId new_surface_id(prev_surface_id.parent_sequence_number() + 1,
                                prev_surface_id.embed_token());
  CreateAndDrawSurface(frame_sink_id, new_surface_id);
  EXPECT_EQ(display_interval_, preferred_interval);
}

TEST_F(FrameRateDeciderTest,
       SurfaceWithMinIntervalPicksLowestSupportedInterval) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 3, min_supported_interval * 2,
      min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId frame_sink_id(1u, 1u);
  base::TimeDelta frame_sink_id_interval = min_supported_interval * 2;
  preferred_intervals_[frame_sink_id] = frame_sink_id_interval;
  auto* surface = CreateAndDrawSurface(frame_sink_id);

  FrameSinkId min_interval_frame_sink_id(1u, 2u);
  preferred_intervals_[min_interval_frame_sink_id] =
      BeginFrameArgs::MinInterval();
  auto* min_interval_surface = CreateAndDrawSurface(min_interval_frame_sink_id);

  // Only draw frame sink with non-default frame sink id, display interval is
  // toggled to its preference.
  UpdateFrame(surface);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(min_interval_surface);
  }
  EXPECT_EQ(display_interval_, frame_sink_id_interval);

  // Draw both frame sink ids, the least supported interval is picked if one
  // active surface requests min config.
  UpdateFrame(surface);
  UpdateFrame(min_interval_surface);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(min_interval_surface);
  }
  // Min interval picks the default interval which is no preference.
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());
}

TEST_F(FrameRateDeciderTest, OptimalFrameSinkIntervelIsPicked) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 2, min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId frame_sink_id1(1u, 1u);
  preferred_intervals_[frame_sink_id1] = min_supported_interval * 2.5;
  auto* surface1 = CreateAndDrawSurface(frame_sink_id1);

  FrameSinkId frame_sink_id2(1u, 2u);
  preferred_intervals_[frame_sink_id2] = min_supported_interval * 2.03;
  auto* surface2 = CreateAndDrawSurface(frame_sink_id2);

  FrameSinkId frame_sink_id3(1u, 3u);
  preferred_intervals_[frame_sink_id3] = min_supported_interval * 0.5;
  auto* surface3 = CreateAndDrawSurface(frame_sink_id3);

  UpdateFrame(surface1);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
  }
  EXPECT_EQ(display_interval_, min_supported_interval * 2);

  UpdateFrame(surface2);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
  }
  EXPECT_EQ(display_interval_, min_supported_interval * 2);

  UpdateFrame(surface3);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface3);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());
}

TEST_F(FrameRateDeciderTest, MinFrameSinkIntervalIsPicked) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 3, min_supported_interval * 2,
      min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId frame_sink_id1(1u, 1u);
  preferred_intervals_[frame_sink_id1] = min_supported_interval * 3;
  auto* surface1 = CreateAndDrawSurface(frame_sink_id1);

  FrameSinkId frame_sink_id2(1u, 2u);
  preferred_intervals_[frame_sink_id2] = min_supported_interval * 2;
  auto* surface2 = CreateAndDrawSurface(frame_sink_id2);

  UpdateFrame(surface1);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
  }
  EXPECT_EQ(display_interval_, min_supported_interval * 3);

  UpdateFrame(surface1);
  UpdateFrame(surface2);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
  }
  EXPECT_EQ(display_interval_, min_supported_interval * 2);

  FrameSinkId frame_sink_id3(1u, 3u);
  preferred_intervals_[frame_sink_id3] = min_supported_interval * 1.8;
  auto* surface3 = CreateAndDrawSurface(frame_sink_id3);
  UpdateFrame(surface1);
  UpdateFrame(surface2);
  UpdateFrame(surface3);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface3);
  }
  // Even though surface3 has a frame interval that is closer to
  // min_supported_interval * 2, we need to pick a smaller interval
  // so that frames from that surface are not dropped.
  EXPECT_EQ(display_interval_, min_supported_interval);
}

TEST_F(FrameRateDeciderTest, TogglesAfterMinNumOfFrames) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 2, min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  frame_rate_decider_->set_min_num_of_frames_to_toggle_interval_for_testing(1u);
  FrameSinkId frame_sink_id(1u, 1u);
  auto preferred_interval = min_supported_interval * 2;
  preferred_intervals_[frame_sink_id] = preferred_interval;

  // First draw.
  auto* surface = CreateAndDrawSurface(frame_sink_id);
  EXPECT_NE(display_interval_, preferred_interval);

  // Second draw.
  UpdateFrame(surface);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
  }
  EXPECT_EQ(display_interval_, preferred_interval);

  // The frame rate is immediately toggled on drawing a surface with a lower
  // interval.
  frame_rate_decider_->set_min_num_of_frames_to_toggle_interval_for_testing(
      50u);
  FrameSinkId frame_sink_id2(1u, 1u);
  preferred_intervals_[frame_sink_id2] = min_supported_interval;
  auto* surface2 = CreateAndDrawSurface(frame_sink_id2);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
  }
  EXPECT_EQ(display_interval_, min_supported_interval);
}

TEST_F(FrameRateDeciderTest, TogglesWithSyntheticBFS) {
  bool hw_support_for_multiple_refresh_rate = false;
  frame_rate_decider_ = std::make_unique<FrameRateDecider>(
      surface_manager_.get(), this, hw_support_for_multiple_refresh_rate,
      false);
  frame_rate_decider_->set_min_num_of_frames_to_toggle_interval_for_testing(0u);
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 2, min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId frame_sink_id1(1u, 1u);
  preferred_intervals_[frame_sink_id1] = min_supported_interval * 2;
  auto* surface1 = CreateAndDrawSurface(frame_sink_id1);

  // If there is only one framesink with non-minimum interval,
  // UnspecifiedFrameInterval() should be used
  UpdateFrame(surface1);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId frame_sink_id2(1u, 2u);
  preferred_intervals_[frame_sink_id2] = min_supported_interval * 2;
  auto* surface2 = CreateAndDrawSurface(frame_sink_id2);

  // With two or more framesinks with non-minimum interval, calculate the
  // optimal interval
  UpdateFrame(surface1);
  UpdateFrame(surface2);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface2);
  }
  EXPECT_EQ(display_interval_, min_supported_interval * 2);

  // Make sure it goes back to UnspecifiedFrameInterval() if we have only one
  // framesink with non-minimum interval left
  UpdateFrame(surface1);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(surface1);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());
}

TEST_F(FrameRateDeciderTest, ManySinksWithMinInterval) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 3, min_supported_interval * 2,
      min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  Surface* surfaces[3];
  for (int i = 0; i < 3; ++i) {
    FrameSinkId frame_sink_id(1u, i);
    if (i == 0)
      preferred_intervals_[frame_sink_id] = min_supported_interval;
    else
      preferred_intervals_[frame_sink_id] = BeginFrameArgs::MinInterval();
    surfaces[i] = CreateAndDrawSurface(frame_sink_id);
  }

  for (int i = 0; i < 3; ++i)
    UpdateFrame(surfaces[i]);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    for (int i = 0; i < 3; ++i)
      frame_rate_decider_->OnSurfaceWillBeDrawn(surfaces[i]);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());
}

// If there are no fixed frame sources, we should not lower the frame interval.
TEST_F(FrameRateDeciderTest, NoFixedIntervalSurfaces) {
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 3, min_supported_interval * 2,
      min_supported_interval};
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  Surface* surfaces[3];
  for (int i = 0; i < 3; ++i) {
    FrameSinkId frame_sink_id(1u, i);
    preferred_intervals_[frame_sink_id] = BeginFrameArgs::MaxInterval();
    frame_sink_types_[frame_sink_id] =
        mojom::CompositorFrameSinkType::kLayerTree;
    surfaces[i] = CreateAndDrawSurface(frame_sink_id);
  }

  for (int i = 0; i < 3; ++i)
    UpdateFrame(surfaces[i]);
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    for (int i = 0; i < 3; ++i)
      frame_rate_decider_->OnSurfaceWillBeDrawn(surfaces[i]);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());
}

TEST_F(FrameRateDeciderTest, NoHwSupportForMultiRefreshRates) {
  bool hw_support_for_multiple_refresh_rate = false;
  frame_rate_decider_ = std::make_unique<FrameRateDecider>(
      surface_manager_.get(), this, hw_support_for_multiple_refresh_rate,
      false);
  frame_rate_decider_->set_min_num_of_frames_to_toggle_interval_for_testing(0u);
  base::TimeDelta min_supported_interval = base::Seconds(1);
  const std::vector<base::TimeDelta> supported_intervals = {
      min_supported_interval * 3, min_supported_interval * 2,
      min_supported_interval};
  frame_rate_decider_
      ->set_frame_interval_for_sinks_with_no_preference_for_testing(
          min_supported_interval);
  frame_rate_decider_->SetSupportedFrameIntervals(supported_intervals);
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  FrameSinkId video_frame_sink_id(1u, 1u);
  Surface* video_surface = CreateAndDrawSurface(video_frame_sink_id);
  preferred_intervals_[video_frame_sink_id] = min_supported_interval;
  frame_sink_types_[video_frame_sink_id] =
      mojom::CompositorFrameSinkType::kVideo;

  FrameSinkId stream_frame_sink_id(1u, 2u);
  Surface* stream_surface = CreateAndDrawSurface(stream_frame_sink_id);
  preferred_intervals_[stream_frame_sink_id] = min_supported_interval;
  frame_sink_types_[stream_frame_sink_id] =
      mojom::CompositorFrameSinkType::kMediaStream;

  FrameSinkId content_frame_sink_id(1u, 3u);
  Surface* content_surface = CreateAndDrawSurface(content_frame_sink_id);
  preferred_intervals_[content_frame_sink_id] = BeginFrameArgs::MaxInterval();
  frame_sink_types_[content_frame_sink_id] =
      mojom::CompositorFrameSinkType::kLayerTree;

  // Only 1 fixed rate source, frame interval is unchanged.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    UpdateFrame(video_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(video_surface);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  // Multiple fixed rate sources, frame interval is lowered.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(video_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(stream_surface);
  }
  EXPECT_EQ(display_interval_, min_supported_interval);

  // One fixed rate source + content source with no preference, frame interval
  // is lowered.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    frame_rate_decider_->OnSurfaceWillBeDrawn(video_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(content_surface);
  }
  EXPECT_EQ(display_interval_, min_supported_interval);

  // The content source opts out of no preference, frame interval is not
  // lowered.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    preferred_intervals_[content_frame_sink_id] = BeginFrameArgs::MinInterval();
    frame_rate_decider_->OnSurfaceWillBeDrawn(video_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(content_surface);
  }
  EXPECT_EQ(display_interval_, FrameRateDecider::UnspecifiedFrameInterval());

  // Multiple fixed rate sources + content source with estimated interval.
  // Frame rate is lowered.
  {
    FrameRateDecider::ScopedAggregate scope(frame_rate_decider_.get());
    preferred_intervals_[content_frame_sink_id] = min_supported_interval;
    frame_rate_decider_->OnSurfaceWillBeDrawn(video_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(stream_surface);
    frame_rate_decider_->OnSurfaceWillBeDrawn(content_surface);
  }
  EXPECT_EQ(display_interval_, min_supported_interval);
}

}  // namespace
}  // namespace viz
