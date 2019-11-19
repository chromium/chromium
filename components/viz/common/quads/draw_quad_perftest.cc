// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace viz {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

constexpr char kMetricPrefixDrawQuad[] = "DrawQuad.";
constexpr char kMetricIterateResourcesRunsPerS[] = "iterate_resources";

perf_test::PerfResultReporter SetUpDrawQuadReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixDrawQuad, story);
  reporter.RegisterImportantMetric(kMetricIterateResourcesRunsPerS, "runs/s");
  return reporter;
}

SharedQuadState* CreateSharedQuadState(RenderPass* render_pass) {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Rect content_rect(26, 28);
  gfx::Rect visible_layer_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = false;
  bool are_contents_opaque = false;
  float opacity = 1.f;
  int sorting_context_id = 65536;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;

  SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
  state->SetAll(quad_transform, content_rect, visible_layer_rect, gfx::RRectF(),
                clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode,
                sorting_context_id);
  return state;
}

class DrawQuadPerfTest : public testing::Test {
 public:
  DrawQuadPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void CreateRenderPass() {
    render_pass_ = RenderPass::Create();
    SharedQuadState* new_shared_state(
        CreateSharedQuadState(render_pass_.get()));
    shared_state_ = render_pass_->CreateAndAppendSharedQuadState();
    *shared_state_ = *new_shared_state;
  }

  void CleanUpRenderPass() {
    render_pass_.reset();
    shared_state_ = nullptr;
  }

  void GenerateTextureDrawQuads(int count, std::vector<DrawQuad*>* quads) {
    for (int i = 0; i < count; ++i) {
      auto* quad = render_pass_->CreateAndAppendDrawQuad<TextureDrawQuad>();
      gfx::Rect rect(0, 0, 100, 100);
      bool needs_blending = false;
      ResourceId resource_id = 1;
      bool premultiplied_alpha = true;
      gfx::PointF uv_top_left(0, 0);
      gfx::PointF uv_bottom_right(1, 1);
      SkColor background_color = SK_ColorRED;
      float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
      bool y_flipped = false;
      bool nearest_neighbor = true;

      quad->SetNew(shared_state_, rect, rect, needs_blending, resource_id,
                   premultiplied_alpha, uv_top_left, uv_bottom_right,
                   background_color, vertex_opacity, y_flipped,
                   nearest_neighbor, /*secure_output_only=*/false,
                   gfx::ProtectedVideoType::kClear);
      quads->push_back(quad);
    }
  }

  void RunIterateResourceTest(const std::string& story, int quad_count) {
    CreateRenderPass();
    std::vector<DrawQuad*> quads;
    GenerateTextureDrawQuads(quad_count, &quads);

    timer_.Reset();
    do {
      for (auto* quad : quads) {
        for (ResourceId& resource_id : quad->resources)
          ++resource_id;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpDrawQuadReporter(story);
    reporter.AddResult(kMetricIterateResourcesRunsPerS, timer_.LapsPerSecond());
    CleanUpRenderPass();
  }

 private:
  std::unique_ptr<RenderPass> render_pass_;
  SharedQuadState* shared_state_;
  base::LapTimer timer_;
};

TEST_F(DrawQuadPerfTest, IterateResources) {
  RunIterateResourceTest("10_quads", 10);
  RunIterateResourceTest("100_quads", 100);
  RunIterateResourceTest("500_quads", 500);
}

}  // namespace
}  // namespace viz
