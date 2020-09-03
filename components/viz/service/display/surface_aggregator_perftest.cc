// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/lap_timer.h"
#include "cc/test/fake_output_surface_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace viz {
namespace {

constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;

constexpr char kMetricPrefixSurfaceAggregator[] = "SurfaceAggregator.";
constexpr char kMetricSpeedRunsPerS[] = "speed";

perf_test::PerfResultReporter SetUpSurfaceAggregatorReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSurfaceAggregator, story);
  reporter.RegisterImportantMetric(kMetricSpeedRunsPerS, "runs/s");
  return reporter;
}

class SurfaceAggregatorPerfTest : public testing::Test {
 public:
  SurfaceAggregatorPerfTest() : manager_(&shared_bitmap_manager_) {
    context_provider_ = TestContextProvider::Create();
    context_provider_->BindToCurrentThread();

    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kGpu, context_provider_.get(),
        &shared_bitmap_manager_);
  }

  void RunTest(int num_surfaces,
               int num_textures,
               float opacity,
               bool optimize_damage,
               bool full_damage,
               const std::string& story) {
    std::vector<std::unique_ptr<CompositorFrameSinkSupport>> child_supports(
        num_surfaces);
    std::vector<base::UnguessableToken> child_tokens(num_surfaces);
    for (int i = 0; i < num_surfaces; i++) {
      child_supports[i] = std::make_unique<CompositorFrameSinkSupport>(
          nullptr, &manager_, FrameSinkId(1, i + 1), kIsChildRoot);
      child_tokens[i] = base::UnguessableToken::Create();
    }
    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), optimize_damage,
        true);
    for (int i = 0; i < num_surfaces; i++) {
      LocalSurfaceId local_surface_id(i + 1, child_tokens[i]);

      auto pass = CompositorRenderPass::Create();
      pass->output_rect = gfx::Rect(0, 0, 1, 2);

      CompositorFrameBuilder frame_builder;

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      for (int j = 0; j < num_textures; j++) {
        TransferableResource resource;
        resource.id = j;
        resource.is_software = true;
        frame_builder.AddTransferableResource(resource);

        auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
        const gfx::Rect rect(0, 0, 1, 2);
        // Half of rects should be visible with partial damage.
        gfx::Rect visible_rect =
            j % 2 == 0 ? gfx::Rect(0, 0, 1, 2) : gfx::Rect(0, 1, 1, 1);
        bool needs_blending = false;
        bool premultiplied_alpha = false;
        const gfx::PointF uv_top_left;
        const gfx::PointF uv_bottom_right;
        SkColor background_color = SK_ColorGREEN;
        const float vertex_opacity[4] = {0.f, 0.f, 1.f, 1.f};
        bool flipped = false;
        bool nearest_neighbor = false;
        quad->SetAll(sqs, rect, visible_rect, needs_blending, j, gfx::Size(),
                     premultiplied_alpha, uv_top_left, uv_bottom_right,
                     background_color, vertex_opacity, flipped,
                     nearest_neighbor, /*secure_output_only=*/false,
                     gfx::ProtectedVideoType::kClear);
      }
      sqs = pass->CreateAndAppendSharedQuadState();
      sqs->opacity = opacity;
      if (i >= 1) {
        auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
        surface_quad->SetNew(
            sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
            // Surface at index i embeds surface at index i - 1.
            SurfaceRange(base::nullopt,
                         SurfaceId(FrameSinkId(1, i),
                                   LocalSurfaceId(i, child_tokens[i - 1]))),
            SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false);
      }

      frame_builder.AddRenderPass(std::move(pass));
      child_supports[i]->SubmitCompositorFrame(local_surface_id,
                                               frame_builder.Build());
    }

    auto root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, FrameSinkId(1, num_surfaces + 1), kIsRoot);
    auto root_token = base::UnguessableToken::Create();
    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    timer_.Reset();
    do {
      auto pass = CompositorRenderPass::Create();

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      surface_quad->SetNew(
          sqs, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 100, 100),
          SurfaceRange(
              base::nullopt,
              // Root surface embeds surface at index num_surfaces - 1.
              SurfaceId(FrameSinkId(1, num_surfaces),
                        LocalSurfaceId(num_surfaces,
                                       child_tokens[num_surfaces - 1]))),
          SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false);

      pass->output_rect = gfx::Rect(0, 0, 100, 100);

      if (full_damage)
        pass->damage_rect = gfx::Rect(0, 0, 100, 100);
      else
        pass->damage_rect = gfx::Rect(0, 0, 1, 1);

      CompositorFrame frame =
          CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

      root_support->SubmitCompositorFrame(
          LocalSurfaceId(num_surfaces + 1, root_token), std::move(frame));

      auto aggregated = aggregator_->Aggregate(
          SurfaceId(FrameSinkId(1, num_surfaces + 1),
                    LocalSurfaceId(num_surfaces + 1, root_token)),
          next_fake_display_time, gfx::OVERLAY_TRANSFORM_NONE);
      next_fake_display_time += BeginFrameArgs::DefaultInterval();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpSurfaceAggregatorReporter(story);
    reporter.AddResult(kMetricSpeedRunsPerS, timer_.LapsPerSecond());
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  scoped_refptr<TestContextProvider> context_provider_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  base::LapTimer timer_;
};

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque) {
  RunTest(20, 100, 1.f, false, true, "many_surfaces_opaque");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_100) {
  RunTest(100, 1, 1.f, true, false, "100_surfaces_1_quad_each");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_300) {
  RunTest(300, 1, 1.f, true, false, "300_surfaces_1_quad_each");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_100) {
  RunTest(100, 100, 1.f, true, false, "100_surfaces_100_quads_each");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_300) {
  RunTest(300, 100, 1.f, true, false, "300_surfaces_100_quads_each");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparent) {
  RunTest(20, 100, .5f, false, true, "many_surfaces_transparent");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfaces) {
  RunTest(3, 1000, 1.f, false, true, "few_surfaces");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaqueDamageCalc) {
  RunTest(20, 100, 1.f, true, true, "many_surfaces_opaque_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparentDamageCalc) {
  RunTest(20, 100, .5f, true, true, "many_surfaces_transparent_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesDamageCalc) {
  RunTest(3, 1000, 1.f, true, true, "few_surfaces_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesAggregateDamaged) {
  RunTest(3, 1000, 1.f, true, false, "few_surfaces_aggregate_damaged");
}

}  // namespace
}  // namespace viz
