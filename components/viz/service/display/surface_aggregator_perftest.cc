// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display/viz_perf_test.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace viz {
namespace {

constexpr char kMetricPrefixSurfaceAggregator[] = "SurfaceAggregator.";
constexpr char kMetricSpeedRunsPerS[] = "speed";

perf_test::PerfResultReporter SetUpSurfaceAggregatorReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSurfaceAggregator, story);
  reporter.RegisterImportantMetric(kMetricSpeedRunsPerS, "runs/s");
  return reporter;
}

class ExpectedOutput {
 public:
  ExpectedOutput(size_t expected_render_passes, size_t expected_quads)
      : expected_render_passes_(expected_render_passes),
        expected_quads_(expected_quads) {}

  void VerifyAggregatedFrame(const AggregatedFrame& frame) {
    EXPECT_EQ(expected_render_passes_, frame.render_pass_list.size());
    size_t count_quads = 0;
    for (auto& render_pass : frame.render_pass_list) {
      count_quads += render_pass->quad_list.size();
    }
    EXPECT_EQ(expected_quads_, count_quads);
  }

 private:
  size_t expected_render_passes_;
  size_t expected_quads_;
};

class SurfaceAggregatorPerfTest : public VizPerfTest {
 public:
  SurfaceAggregatorPerfTest() : manager_(&shared_bitmap_manager_) {
    resource_provider_ = std::make_unique<DisplayResourceProviderSoftware>(
        &shared_bitmap_manager_);
  }

  void RunTest(int num_surfaces,
               int num_textures,
               float opacity,
               bool optimize_damage,
               bool full_damage,
               const std::string& story,
               ExpectedOutput expected_output) {
    std::vector<std::unique_ptr<CompositorFrameSinkSupport>> child_supports(
        num_surfaces);
    std::vector<base::UnguessableToken> child_tokens(num_surfaces);
    for (int i = 0; i < num_surfaces; i++) {
      child_supports[i] = std::make_unique<CompositorFrameSinkSupport>(
          nullptr, &manager_, FrameSinkId(1, i + 1), /*is_root=*/false);
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
        const gfx::Size size(1, 2);
        TransferableResource resource = TransferableResource::MakeSoftware(
            SharedBitmap::GenerateId(), size, ResourceFormat::RGBA_8888);
        resource.id = ResourceId(j);
        frame_builder.AddTransferableResource(resource);

        auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
        const gfx::Rect rect(size);
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
        quad->SetAll(sqs, rect, visible_rect, needs_blending, ResourceId(j),
                     gfx::Size(), premultiplied_alpha, uv_top_left,
                     uv_bottom_right, background_color, vertex_opacity, flipped,
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
            SurfaceRange(absl::nullopt,
                         SurfaceId(FrameSinkId(1, i),
                                   LocalSurfaceId(i, child_tokens[i - 1]))),
            SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false);
      }

      frame_builder.AddRenderPass(std::move(pass));
      child_supports[i]->SubmitCompositorFrame(local_surface_id,
                                               frame_builder.Build());
    }

    auto root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, FrameSinkId(1, num_surfaces + 1), /*is_root=*/true);
    auto root_token = base::UnguessableToken::Create();
    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::TimeDelta::FromSeconds(1);

    bool first_lap = true;
    timer_.Reset();
    do {
      auto pass = CompositorRenderPass::Create();

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      surface_quad->SetNew(
          sqs, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 100, 100),
          SurfaceRange(
              absl::nullopt,
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

      if (!timer_.IsWarmedUp()) {
        // Verify the expected number of RenderPasses and DrawQuads are
        // produced for all warmup laps except the first. The first frame will
        // have full damage regardless of what |full_damage| specifies which
        // can impact how many quads are aggregated.
        if (first_lap) {
          first_lap = false;
        } else {
          expected_output.VerifyAggregatedFrame(aggregated);
        }
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpSurfaceAggregatorReporter(story);
    reporter.AddResult(kMetricSpeedRunsPerS, timer_.LapsPerSecond());
  }

  void SetUpRenderPassListResources(
      CompositorRenderPassList* render_pass_list) {
    std::set<ResourceId> created_resources;
    for (auto& render_pass : *render_pass_list) {
      for (auto* quad : render_pass->quad_list) {
        for (ResourceId resource_id : quad->resources) {
          // Don't create multiple resources for the same ResourceId.
          if (created_resources.find(resource_id) != created_resources.end()) {
            continue;
          }
          resource_list_.push_back(TransferableResource::MakeSoftware(
              SharedBitmap::GenerateId(), quad->rect.size(), RGBA_8888));
          resource_list_.back().id = resource_id;
          created_resources.insert(resource_id);
        }
      }
    }
  }

  void RunSingleSurfaceRenderPassListFromJson(const std::string& tag,
                                              const std::string& site,
                                              uint32_t year,
                                              size_t index) {
    CompositorRenderPassList render_pass_list;
    ASSERT_TRUE(CompositorRenderPassListFromJSON(tag, site, year, index,
                                                 &render_pass_list));
    ASSERT_FALSE(render_pass_list.empty());
    this->SetUpRenderPassListResources(&render_pass_list);

    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), true, true);

    constexpr FrameSinkId root_frame_sink_id(1, 1);
    TestSurfaceIdAllocator root_surface_id(root_frame_sink_id);
    auto root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, root_frame_sink_id, /*is_root=*/true);

    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::TimeDelta::FromSeconds(1);

    timer_.Reset();
    do {
      CompositorRenderPassList local_list;
      CompositorRenderPass::CopyAllForTest(render_pass_list, &local_list);
      // Ensure damage encompasses the entire output_rect so everything is
      // aggregated.
      auto& last_render_pass = *local_list.back();
      last_render_pass.damage_rect = last_render_pass.output_rect;

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(local_list))
                                  .SetTransferableResources(resource_list_)
                                  .Build();
      root_support->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                          std::move(frame));
      auto aggregated = aggregator_->Aggregate(
          root_surface_id, next_fake_display_time, gfx::OVERLAY_TRANSFORM_NONE);

      next_fake_display_time += BeginFrameArgs::DefaultInterval();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpSurfaceAggregatorReporter(site + "_json");
    reporter.AddResult(kMetricSpeedRunsPerS, timer_.LapsPerSecond());
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  std::vector<TransferableResource> resource_list_;
};

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque) {
  RunTest(20, 100, 1.f, false, true, "many_surfaces_opaque",
          ExpectedOutput(1, 2000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_100) {
  RunTest(100, 1, 1.f, true, false, "100_surfaces_1_quad_each",
          ExpectedOutput(1, 100));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_300) {
  RunTest(300, 1, 1.f, true, false, "300_surfaces_1_quad_each",
          ExpectedOutput(1, 300));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_100) {
  RunTest(100, 100, 1.f, true, false, "100_surfaces_100_quads_each",
          ExpectedOutput(1, 5000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_300) {
  RunTest(300, 100, 1.f, true, false, "300_surfaces_100_quads_each",
          ExpectedOutput(1, 15000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparent) {
  RunTest(20, 100, .5f, false, true, "many_surfaces_transparent",
          ExpectedOutput(20, 2019));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfaces) {
  RunTest(3, 20, 1.f, false, true, "few_surfaces", ExpectedOutput(1, 60));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaqueDamageCalc) {
  RunTest(20, 100, 1.f, true, true, "many_surfaces_opaque_damage_calc",
          ExpectedOutput(1, 2000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparentDamageCalc) {
  RunTest(20, 100, .5f, true, true, "many_surfaces_transparent_damage_calc",
          ExpectedOutput(20, 2019));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesDamageCalc) {
  RunTest(3, 1000, 1.f, true, true, "few_surfaces_damage_calc",
          ExpectedOutput(1, 3000));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesAggregateDamaged) {
  RunTest(3, 1000, 1.f, true, false, "few_surfaces_aggregate_damaged",
          ExpectedOutput(1, 1500));
}

#define TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(SITE, FRAME)           \
  TEST_F(SurfaceAggregatorPerfTest, SITE##_JsonTest) {                   \
    this->RunSingleSurfaceRenderPassListFromJson(                        \
        /*tag=*/"top_real_world_desktop", /*site=*/#SITE, /*year=*/2018, \
        /*frame_index=*/FRAME);                                          \
  }

TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(accu_weather, 298)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(amazon, 30)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(blogspot, 56)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(ebay, 44)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(espn, 463)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(facebook, 327)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(gmail, 66)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(google_calendar, 53)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(google_docs, 369)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(google_image_search, 44)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(google_plus, 45)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(google_web_search, 89)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(linkedin, 284)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(pinterest, 120)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(techcrunch, 190)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(twitch, 396)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(wikipedia, 48)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(wordpress, 75)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(yahoo_answers, 74)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(yahoo_sports, 269)

#undef TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST

}  // namespace
}  // namespace viz
