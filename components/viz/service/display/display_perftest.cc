// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/occlusion_culler.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

constexpr int kTimeLimitMillis = 3000;
constexpr int kWarmupRuns = 5;
constexpr int kTimeCheckInterval = 10;
constexpr int kHeight = 1000;
constexpr int kWidth = 1000;
constexpr float kDeviceScaleFactor = 1.0f;

constexpr char kMetricPrefixRemoveOverdrawQuad[] = "RemoveOverdrawQuad.";
constexpr char kMetricOverlapThroughputRunsPerS[] = "overlap_throughput";
constexpr char kMetricIsolatedThroughputRunsPerS[] = "isolated_throughput";
constexpr char kMetricPartialOverlapThroughputRunsPerS[] =
    "partial_overlap_throughput";
constexpr char kMetricAdjacentThroughputRunsPerS[] = "adjacent_throughput";

perf_test::PerfResultReporter SetUpRemoveOverdrawQuadReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixRemoveOverdrawQuad,
                                         story);
  reporter.RegisterImportantMetric(kMetricOverlapThroughputRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricIsolatedThroughputRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricPartialOverlapThroughputRunsPerS,
                                   "runs/s");
  reporter.RegisterImportantMetric(kMetricAdjacentThroughputRunsPerS, "runs/s");
  return reporter;
}

class RemoveOverdrawQuadPerfTest : public testing::Test {
 public:
  RemoveOverdrawQuadPerfTest()
      : timer_(kWarmupRuns,
               base::Milliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        task_runner_(base::MakeRefCounted<base::NullTaskRunner>()) {}

 protected:
  // testing::Test:
  void SetUp() override {
    DCHECK(!occlusion_culler_);

    overlay_processor_ = std::make_unique<OverlayProcessorStub>();
    occlusion_culler_ = std::make_unique<OcclusionCuller>(
        overlay_processor_.get(), RendererSettings::OcclusionCullerSettings());
  }

  // Create an arbitrary SharedQuadState for the given |render_pass|.
  SharedQuadState* CreateSharedQuadState(AggregatedRenderPass* render_pass,
                                         gfx::Rect rect) {
    gfx::Transform quad_transform = gfx::Transform();
    bool are_contents_opaque = true;
    float opacity = 1.f;
    int sorting_context_id = 65536;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
    state->SetAll(quad_transform, rect, rect,
                  /*filter_info=*/gfx::MaskFilterInfo(),
                  /*clip=*/std::nullopt, are_contents_opaque, opacity,
                  blend_mode, sorting_context_id, /*layer_id=*/0u,
                  /*fast_rounded_corner=*/false);
    return state;
  }

  // Append draw quads to a given |shared_quad_state|.
  void AppendQuads(SharedQuadState* shared_quad_state,
                   int quad_height,
                   int quad_width) {
    bool needs_blending = false;
    ResourceId resource_id(1);
    bool premultiplied_alpha = true;
    gfx::PointF uv_top_left(0, 0);
    gfx::PointF uv_bottom_right(1, 1);
    SkColor4f background_color = SkColors::kRed;
    bool y_flipped = false;
    bool nearest_neighbor = true;

    int x_left = shared_quad_state->visible_quad_layer_rect.x();
    int x_right = x_left + shared_quad_state->visible_quad_layer_rect.width();
    int y_top = shared_quad_state->visible_quad_layer_rect.y();
    int y_bottom = y_top + shared_quad_state->visible_quad_layer_rect.height();
    int i = x_left;
    int j = y_top;
    while (i + quad_width <= x_right) {
      while (j + quad_height <= y_bottom) {
        auto* quad = frame_.render_pass_list.front()
                         ->CreateAndAppendDrawQuad<TextureDrawQuad>();
        gfx::Rect rect(i, j, quad_width, quad_height);
        quad->SetNew(shared_quad_state, rect, rect, needs_blending, resource_id,
                     premultiplied_alpha, uv_top_left, uv_bottom_right,
                     background_color, y_flipped, nearest_neighbor,
                     /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
        j += quad_height;
      }
      j = y_top;
      i += quad_width;
    }
  }

  // All SharedQuadState are overlapping the same region.
  //  +--------+
  //  | s1/2/3 |
  //  +--------+
  void IterateOverlapShareQuadStates(const std::string& story,
                                     int shared_quad_state_count,
                                     int quad_count) {
    frame_.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    CreateOverlapShareQuadStates(shared_quad_state_count, quad_count);

    timer_.Reset();
    do {
      occlusion_culler_->RemoveOverdrawQuads(&frame_, kDeviceScaleFactor);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpRemoveOverdrawQuadReporter(story);
    reporter.AddResult(kMetricOverlapThroughputRunsPerS,
                       timer_.LapsPerSecond());
    frame_ = AggregatedFrame{};
  }

  void CreateOverlapShareQuadStates(int shared_quad_state_count,
                                    int quad_count) {
    int quad_height = kHeight / quad_count;
    int quad_width = kWidth / quad_count;
    int total_shared_quad_state =
        shared_quad_state_count * shared_quad_state_count;
    for (int i = 0; i < total_shared_quad_state; i++) {
      gfx::Rect rect(0, 0, kHeight, kWidth);
      SharedQuadState* new_shared_state(
          CreateSharedQuadState(frame_.render_pass_list.front().get(), rect));
      AppendQuads(new_shared_state, quad_height, quad_width);
    }
  }

  // SharedQuadState are non-overlapped as shown in the figure below.
  // +---+
  // |s1 |
  // +---+---+
  //     |s2 |
  //     +---+---+
  //         |s3 |
  //         +---+
  void IterateIsolatedSharedQuadStates(const std::string& story,
                                       int shared_quad_state_count,
                                       int quad_count) {
    frame_.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    CreateIsolatedSharedQuadStates(shared_quad_state_count, quad_count);
    timer_.Reset();
    do {
      occlusion_culler_->RemoveOverdrawQuads(&frame_, kDeviceScaleFactor);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpRemoveOverdrawQuadReporter(story);
    reporter.AddResult(kMetricIsolatedThroughputRunsPerS,
                       timer_.LapsPerSecond());
    frame_ = AggregatedFrame{};
  }

  void CreateIsolatedSharedQuadStates(int shared_quad_state_count,
                                      int quad_count) {
    int shared_quad_state_height =
        kHeight / (shared_quad_state_count * shared_quad_state_count);
    int shared_quad_state_width =
        kWidth / (shared_quad_state_count * shared_quad_state_count);
    int quad_height = shared_quad_state_height / quad_count;
    int quad_width = shared_quad_state_width / quad_count;
    int i = 0;
    int j = 0;
    while (i + shared_quad_state_height <= kWidth ||
           j + shared_quad_state_height <= kHeight) {
      gfx::Rect rect(i, j, shared_quad_state_height, shared_quad_state_width);
      SharedQuadState* new_shared_state(
          CreateSharedQuadState(frame_.render_pass_list.front().get(), rect));
      AppendQuads(new_shared_state, quad_height, quad_width);
      j += shared_quad_state_height;
      i += shared_quad_state_width;
    }
  }

  // SharedQuadState are overlapped as shown in the figure below.
  //  +----+
  //  | +----+
  //  | |  +----+
  //  +-|  |  +----+
  //    +--|  |    |
  //       +--|    |
  //          +----+
  void IteratePartiallyOverlapSharedQuadStates(const std::string& story,
                                               int shared_quad_state_count,
                                               float percentage_overlap,
                                               int quad_count) {
    frame_.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    CreatePartiallyOverlapSharedQuadStates(shared_quad_state_count,
                                           percentage_overlap, quad_count);
    timer_.Reset();
    do {
      occlusion_culler_->RemoveOverdrawQuads(&frame_, kDeviceScaleFactor);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpRemoveOverdrawQuadReporter(story);
    reporter.AddResult(kMetricPartialOverlapThroughputRunsPerS,
                       timer_.LapsPerSecond());
    frame_ = AggregatedFrame{};
  }

  void CreatePartiallyOverlapSharedQuadStates(int shared_quad_state_count,
                                              float percentage_overlap,
                                              int quad_count) {
    int shared_quad_state_height =
        kHeight / (shared_quad_state_count * shared_quad_state_count);
    int shared_quad_state_width =
        kWidth / (shared_quad_state_count * shared_quad_state_count);
    int quad_height = shared_quad_state_height / quad_count;
    int quad_width = shared_quad_state_width / quad_count;
    int i = 0;
    int j = 0;
    for (int count = 0; count < shared_quad_state_count; count++) {
      gfx::Rect rect(i, j, shared_quad_state_height, shared_quad_state_width);
      SharedQuadState* new_shared_state(
          CreateSharedQuadState(frame_.render_pass_list.front().get(), rect));
      AppendQuads(new_shared_state, quad_height, quad_width);
      i += shared_quad_state_width * percentage_overlap;
      j += shared_quad_state_height * percentage_overlap;
    }
  }

  // SharedQuadState are all adjacent to each other and added as the order shown
  // in the figure below.
  //  +----+----+
  //  | s1 | s3 |
  //  +----+----+
  //  | s2 | s4 |
  //  +----+----+
  void IterateAdjacentSharedQuadStates(const std::string& story,
                                       int shared_quad_state_count,
                                       int quad_count) {
    frame_.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    CreateAdjacentSharedQuadStates(shared_quad_state_count, quad_count);
    timer_.Reset();
    do {
      occlusion_culler_->RemoveOverdrawQuads(&frame_, kDeviceScaleFactor);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpRemoveOverdrawQuadReporter(story);
    reporter.AddResult(kMetricAdjacentThroughputRunsPerS,
                       timer_.LapsPerSecond());
    frame_ = AggregatedFrame{};
  }

  void CreateAdjacentSharedQuadStates(int shared_quad_state_count,
                                      int quad_count) {
    int shared_quad_state_height = kHeight / shared_quad_state_count;
    int shared_quad_state_width = kWidth / shared_quad_state_count;
    int quad_height = shared_quad_state_height / quad_count;
    int quad_width = shared_quad_state_width / quad_count;
    int i = 0;
    int j = 0;
    while (i + shared_quad_state_height <= kWidth) {
      while (j + shared_quad_state_width <= kHeight) {
        gfx::Rect rect(i, j, shared_quad_state_height, shared_quad_state_width);
        SharedQuadState* new_shared_state =
            CreateSharedQuadState(frame_.render_pass_list.front().get(), rect);
        AppendQuads(new_shared_state, quad_height, quad_width);
        j += shared_quad_state_height;
      }
      j = 0;
      i += shared_quad_state_width;
    }
  }

 private:
  AggregatedFrame frame_;
  base::LapTimer timer_;
  std::unique_ptr<OverlayProcessorInterface> overlay_processor_;
  std::unique_ptr<OcclusionCuller> occlusion_culler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

TEST_F(RemoveOverdrawQuadPerfTest, IterateOverlapShareQuadStates) {
  IterateOverlapShareQuadStates("4_sqs_with_4_quads", 2, 2);
  IterateOverlapShareQuadStates("4_sqs_with_100_quads", 2, 10);
  IterateOverlapShareQuadStates("100_sqs_with_4_quads", 10, 2);
  IterateOverlapShareQuadStates("100_sqs_with_100_quads", 10, 10);
}

TEST_F(RemoveOverdrawQuadPerfTest, IterateIsolatedSharedQuadStates) {
  IterateIsolatedSharedQuadStates("2_sqs_with_4_quads", 2, 2);
  IterateIsolatedSharedQuadStates("2_sqs_with_100_quads", 2, 10);
  IterateIsolatedSharedQuadStates("10_sqs_with_4_quads", 10, 2);
  IterateIsolatedSharedQuadStates("10_sqs_with_100_quads", 10, 10);
}

TEST_F(RemoveOverdrawQuadPerfTest, IteratePartiallyOverlapSharedQuadStates) {
  IteratePartiallyOverlapSharedQuadStates("2_sqs_with_4_quads", 2, 0.5, 2);
  IteratePartiallyOverlapSharedQuadStates("2_sqs_with_100_quads", 2, 0.5, 10);
  IteratePartiallyOverlapSharedQuadStates("10_sqs_with_4_quads", 10, 0.5, 2);
  IteratePartiallyOverlapSharedQuadStates("10_sqs_with_100_quads", 10, 0.5, 10);
}

TEST_F(RemoveOverdrawQuadPerfTest, IterateAdjacentSharedQuadStates) {
  IterateAdjacentSharedQuadStates("4_sqs_with_4_quads", 2, 2);
  IterateAdjacentSharedQuadStates("4_sqs_with_100_quads", 2, 10);
  IterateAdjacentSharedQuadStates("100_sqs_with_4_quads", 10, 2);
  IterateAdjacentSharedQuadStates("100_sqs_with_100_quads", 10, 10);
}

}  // namespace
}  // namespace viz
