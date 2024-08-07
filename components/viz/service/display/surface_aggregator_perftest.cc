// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "cc/test/fake_output_surface_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass_io.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display/viz_perftest.h"
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
constexpr char kMetricAggregateUs[] = "aggregate";
constexpr char kMetricPrewalkUs[] = "prewalk";
constexpr char kMetricDeclareResourcesUs[] = "declare_resources";
constexpr char kMetricCopyUs[] = "copy";

perf_test::PerfResultReporter SetUpSurfaceAggregatorReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSurfaceAggregator, story);
  reporter.RegisterImportantMetric(kMetricSpeedRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricAggregateUs, "μs");
  reporter.RegisterImportantMetric(kMetricPrewalkUs, "μs");
  reporter.RegisterImportantMetric(kMetricDeclareResourcesUs, "μs");
  reporter.RegisterImportantMetric(kMetricCopyUs, "μs");
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
  SurfaceAggregatorPerfTest()
      : manager_(FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)) {
    resource_provider_ = std::make_unique<DisplayResourceProviderSoftware>(
        &shared_bitmap_manager_, /*shared_image_manager=*/nullptr,
        /*sync_point_manager=*/nullptr, /*gpu_scheduler=*/nullptr);
  }

  void RunTest(int num_surfaces,
               int num_textures,
               float opacity,
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
        manager_.surface_manager(), resource_provider_.get(), true);
    for (int i = 0; i < num_surfaces; i++) {
      LocalSurfaceId local_surface_id(i + 1, child_tokens[i]);

      auto pass = CompositorRenderPass::Create();
      pass->output_rect = gfx::Rect(0, 0, 1, 2);

      CompositorFrameBuilder frame_builder;

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      for (int j = 0; j < num_textures; j++) {
        const gfx::Size size(1, 2);
        TransferableResource resource =
            TransferableResource::MakeSoftwareSharedBitmap(
                SharedBitmap::GenerateId(), gpu::SyncToken(), size,
                SinglePlaneFormat::kRGBA_8888);
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
        SkColor4f background_color = SkColors::kGreen;
        bool flipped = false;
        bool nearest_neighbor = false;
        quad->SetAll(sqs, rect, visible_rect, needs_blending, ResourceId(j),
                     gfx::Size(), premultiplied_alpha, uv_top_left,
                     uv_bottom_right, background_color, flipped,
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
            SurfaceRange(std::nullopt,
                         SurfaceId(FrameSinkId(1, i),
                                   LocalSurfaceId(i, child_tokens[i - 1]))),
            SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false);
      }

      frame_builder.AddRenderPass(std::move(pass));
      child_supports[i]->SubmitCompositorFrame(local_surface_id,
                                               frame_builder.Build());
    }

    auto root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, FrameSinkId(1, num_surfaces + 1), /*is_root=*/true);
    auto root_token = base::UnguessableToken::Create();
    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::Seconds(1);

    bool first_lap = true;
    timer_.Reset();
    do {
      auto pass = CompositorRenderPass::Create();

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      surface_quad->SetNew(
          sqs, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 100, 100),
          SurfaceRange(
              std::nullopt,
              // Root surface embeds surface at index num_surfaces - 1.
              SurfaceId(FrameSinkId(1, num_surfaces),
                        LocalSurfaceId(num_surfaces,
                                       child_tokens[num_surfaces - 1]))),
          SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false);

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
      FrameSinkId frame_sink_id,
      uint64_t frame_index,
      CompositorRenderPassList* render_pass_list) {
    std::set<ResourceId> resources_added;
    for (auto& render_pass : *render_pass_list) {
      for (auto* quad : render_pass->quad_list) {
        for (ResourceId resource_id : quad->resources) {
          // Only add resources to the resource list once.
          if (resources_added.find(resource_id) != resources_added.end()) {
            continue;
          }
          auto& created_resources =
              resource_data_map_[frame_sink_id].created_resources;
          // Create the resource if we haven't yet.
          if (created_resources.find(resource_id) == created_resources.end()) {
            created_resources[resource_id] =
                TransferableResource::MakeSoftwareSharedBitmap(
                    SharedBitmap::GenerateId(), gpu::SyncToken(),
                    quad->rect.size(), SinglePlaneFormat::kRGBA_8888);
            created_resources[resource_id].id = resource_id;
          }
          resource_data_map_[frame_sink_id]
              .resource_lists[frame_index]
              .push_back(created_resources[resource_id]);
          resources_added.insert(resource_id);
        }
      }
    }
  }

  void SubmitCompositorFrame(CompositorFrameSinkSupport* frame_sink,
                             SurfaceId surface_id,
                             uint64_t frame_index,
                             const CompositorRenderPassList& render_pass_list,
                             std::vector<SurfaceRange> referenced_surfaces,
                             bool set_full_damage) {
    CompositorRenderPassList local_list;
    CompositorRenderPass::CopyAllForTest(render_pass_list, &local_list);
    if (set_full_damage) {
      // Ensure damage encompasses the entire output_rect so everything is
      // aggregated.
      auto& last_render_pass = *local_list.back();
      last_render_pass.damage_rect = last_render_pass.output_rect;
    }

    CompositorFrameBuilder frame_builder;
    frame_builder.SetRenderPassList(std::move(local_list))
        .SetTransferableResources(resource_data_map_[surface_id.frame_sink_id()]
                                      .resource_lists[frame_index])
        .SetReferencedSurfaces(std::move(referenced_surfaces));
    frame_sink->SubmitCompositorFrame(surface_id.local_surface_id(),
                                      frame_builder.Build());
  }

  void RunSingleSurfaceRenderPassListFromJson(const std::string& tag,
                                              const std::string& site,
                                              uint32_t year,
                                              size_t index) {
    CompositorRenderPassList render_pass_list;
    ASSERT_TRUE(CompositorRenderPassListFromJSON(tag, site, year, index,
                                                 &render_pass_list));
    ASSERT_FALSE(render_pass_list.empty());

    constexpr FrameSinkId root_frame_sink_id(1, 1);
    TestSurfaceIdAllocator root_surface_id(root_frame_sink_id);
    this->SetUpRenderPassListResources(root_frame_sink_id, /*frame_index=*/1,
                                       &render_pass_list);

    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), true);

    auto root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, root_frame_sink_id, /*is_root=*/true);

    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::Seconds(1);

    timer_.Reset();
    do {
      SubmitCompositorFrame(root_support.get(), root_surface_id,
                            /*frame_index=*/1, render_pass_list,
                            /*referenced_surfaces=*/{},
                            /*set_full_damage=*/true);
      auto aggregated = aggregator_->Aggregate(
          root_surface_id, next_fake_display_time, gfx::OVERLAY_TRANSFORM_NONE);

      next_fake_display_time += BeginFrameArgs::DefaultInterval();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter =
        SetUpSurfaceAggregatorReporter(site + "_single_surface_json");
    reporter.AddResult(kMetricSpeedRunsPerS, timer_.LapsPerSecond());
  }

  // Loads FrameData arrays from JSON file(s), submits CompositorFrames, and
  // aggregates the result repeatedly for a specified duration. The number of
  // laps completed in that time is a proxy for SurfaceAggregator's performance.
  //
  // For multi-frame tests:
  //   - Frame sinks are created for all surfaces listed in *any* of the JSON
  //     files before the timer starts.
  //   - Every CompositorFrame listed in each frame's JSON file will be
  //     submitted, in order, for that frame before calling Aggregate.
  //   - One lap is completed when every frame in the sequence is aggregated.
  //   - Full damage is set for every CompositorFrame in the first frame of the
  //     sequence, each lap.
  // For single-frame tests (frame_start = frame_end):
  //   - Non-root surfaces have their CompositorFrames submitted once before the
  //     timer starts.
  //   - One loop is complete when the root surface's CompositorFrame is
  //     submitted and aggregated.
  //   - Full damage is set for every CompositorFrame submitted.
  void RunPerfTestFromJson(const std::string& group,
                           const std::string& name,
                           size_t frame_start,
                           size_t frame_end) {
    DCHECK_LE(frame_start, frame_end);
    bool single_frame = frame_start == frame_end;

    std::optional<base::FilePath> unzipped_folder = UnzipFrameData(group, name);
    ASSERT_TRUE(unzipped_folder);

    std::vector<std::vector<FrameData>> frames;
    for (size_t i = frame_start; i <= frame_end; ++i) {
      std::vector<FrameData> frame;
      base::FilePath json_path = unzipped_folder->AppendASCII(
          base::StringPrintf("%04d.json", static_cast<int>(i)));
      ASSERT_TRUE(FrameDataFromJson(json_path, &frame));
      ASSERT_FALSE(frame.empty());
      frames.push_back(std::move(frame));
    }

    // Setup all of the frame sinks.
    std::map<FrameSinkId, std::unique_ptr<CompositorFrameSinkSupport>>
        frame_sinks;
    for (auto& frame : frames) {
      for (auto& frame_data : frame) {
        auto frame_sink_id = frame_data.surface_id.frame_sink_id();
        if (frame_sinks.find(frame_sink_id) == frame_sinks.end()) {
          // The first surface in the first frame is the root surface.
          frame_sinks[frame_sink_id] =
              std::make_unique<CompositorFrameSinkSupport>(
                  nullptr, &manager_, frame_sink_id,
                  /*is_root=*/frame_sinks.empty());
        }

        this->SetUpRenderPassListResources(
            frame_sink_id, frame_data.frame_index,
            &frame_data.compositor_frame.render_pass_list);
      }
    }

    if (single_frame) {
      // We only need to submit the non-root surfaces (i = [1, N-1]) once, but
      // we'll submit the root surface every lap.
      // Loop in reverse order so surfaces are always submitted before their
      // parents are.
      auto& frame = frames[0];
      for (int i = frame.size() - 1; i >= 1; --i) {
        auto& surface_id = frame[i].surface_id;
        auto frame_index = frame[i].frame_index;
        auto& render_pass_list = frame[i].compositor_frame.render_pass_list;
        auto& referenced_surfaces =
            frame[i].compositor_frame.metadata.referenced_surfaces;
        SubmitCompositorFrame(frame_sinks[surface_id.frame_sink_id()].get(),
                              surface_id, frame_index, render_pass_list,
                              referenced_surfaces,
                              /*set_full_damage=*/true);
      }
    }

    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), true);

    base::HistogramBase* aggregate_histogram =
        base::Histogram::FactoryMicrosecondsTimeGet(
            "AggregateUs", SurfaceAggregator::kHistogramMinTime,
            SurfaceAggregator::kHistogramMaxTime,
            SurfaceAggregator::kHistogramTimeBuckets,
            base::Histogram::kNoFlags);

    base::TimeTicks next_fake_display_time =
        base::TimeTicks() + base::Seconds(1);
    timer_.Reset();
    int laps = 0;
    do {
      // Reset the frame sinks between laps (not before the first lap).
      if (laps++ > 0) {
        for (auto& entry : frame_sinks) {
          const FrameSinkId& frame_sink_id = entry.first;
          auto& frame_sink = entry.second;
          bool is_root = frame_sink->is_root();
          frame_sink.reset();
          manager_.InvalidateFrameSinkId(frame_sink_id);
          frame_sink = std::make_unique<CompositorFrameSinkSupport>(
              nullptr, &manager_, frame_sink_id, is_root);
        }
      }

      int frame_num = 0;
      for (auto& frame : frames) {
        size_t surface_index = 0;
        for (auto& frame_data : frame) {
          // For single-frame tests, only submit the root surface's
          // CompositorFrame.
          if (single_frame && surface_index != 0) {
            continue;
          }
          auto& surface_id = frame_data.surface_id;
          auto frame_index = frame_data.frame_index;
          auto& render_pass_list = frame_data.compositor_frame.render_pass_list;
          auto& referenced_surfaces =
              frame_data.compositor_frame.metadata.referenced_surfaces;
          // For multi-frame tests, only set the full damage for the first frame
          // in the sequence.
          bool set_full_damage = single_frame || frame_num == 0;

          SubmitCompositorFrame(frame_sinks[surface_id.frame_sink_id()].get(),
                                surface_id, frame_index, render_pass_list,
                                referenced_surfaces, set_full_damage);
          surface_index++;
        }

        base::ElapsedTimer aggregate_timer;
        // Always aggregate the root surface.
        auto aggregated = aggregator_->Aggregate(frames[0][0].surface_id,
                                                 next_fake_display_time,
                                                 gfx::OVERLAY_TRANSFORM_NONE);
        aggregate_histogram->AddTimeMicrosecondsGranularity(
            aggregate_timer.Elapsed());

        frame_num++;
      }

      next_fake_display_time += BeginFrameArgs::DefaultInterval();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpSurfaceAggregatorReporter(name);
    reporter.AddResult(kMetricSpeedRunsPerS, timer_.LapsPerSecond());

    reporter.AddResult(kMetricAggregateUs,
                       GetHistogramStats(aggregate_histogram));
    reporter.AddResult(kMetricPrewalkUs, GetHistogramStats("PrewalkUs"));
    reporter.AddResult(kMetricDeclareResourcesUs,
                       GetHistogramStats("DeclareResourcesUs"));
    reporter.AddResult(kMetricCopyUs, GetHistogramStats("CopyUs"));
  }

 private:
  std::string GetHistogramStats(std::string histogram_name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(
        "Compositing.SurfaceAggregator." + histogram_name);
    CHECK(histogram);

    // To separate histogram results from different test runs, we sample the
    // delta between successive runs and import the sample into a new unique
    // histogram that can be graphed.
    auto samples = histogram->SnapshotDelta();
    base::HistogramBase* temp_histogram =
        base::Histogram::FactoryMicrosecondsTimeGet(
            histogram_name, SurfaceAggregator::kHistogramMinTime,
            SurfaceAggregator::kHistogramMaxTime,
            SurfaceAggregator::kHistogramTimeBuckets,
            base::Histogram::kNoFlags);
    temp_histogram->AddSamples(*samples);
    return GetHistogramStats(temp_histogram);
  }

  std::string GetHistogramStats(base::HistogramBase* histogram) {
    base::Value::Dict graph_dict = histogram->ToGraphDict();
    // The header contains the sample count and the mean.
    return *graph_dict.FindString("header");
  }

 protected:
  struct ResourceData {
    // Resource list for each frame_index.
    std::map<uint64_t, std::vector<TransferableResource>> resource_lists;
    std::map<ResourceId, TransferableResource> created_resources;
  };

  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  std::map<FrameSinkId, ResourceData> resource_data_map_;
};

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque) {
  RunTest(20, 100, 1.f, true, "many_surfaces_opaque", ExpectedOutput(1, 2000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_100) {
  RunTest(100, 1, 1.f, false, "100_surfaces_1_quad_each",
          ExpectedOutput(1, 100));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque_300) {
  RunTest(300, 1, 1.f, false, "300_surfaces_1_quad_each",
          ExpectedOutput(1, 300));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_100) {
  RunTest(100, 100, 1.f, false, "100_surfaces_100_quads_each",
          ExpectedOutput(1, 5000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesManyQuadsOpaque_300) {
  RunTest(300, 100, 1.f, false, "300_surfaces_100_quads_each",
          ExpectedOutput(1, 15000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparent) {
  RunTest(20, 100, .5f, true, "many_surfaces_transparent",
          ExpectedOutput(20, 2019));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfaces) {
  RunTest(3, 20, 1.f, true, "few_surfaces", ExpectedOutput(1, 60));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaqueDamageCalc) {
  RunTest(20, 100, 1.f, true, "many_surfaces_opaque_damage_calc",
          ExpectedOutput(1, 2000));
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparentDamageCalc) {
  RunTest(20, 100, .5f, true, "many_surfaces_transparent_damage_calc",
          ExpectedOutput(20, 2019));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesDamageCalc) {
  RunTest(3, 1000, 1.f, true, "few_surfaces_damage_calc",
          ExpectedOutput(1, 3000));
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesAggregateDamaged) {
  RunTest(3, 1000, 1.f, false, "few_surfaces_aggregate_damaged",
          ExpectedOutput(1, 1500));
}

#define TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(SITE, FRAME)           \
  TEST_F(SurfaceAggregatorPerfTest, SITE##_SingleSurfaceTest) {          \
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

#define MULTI_SURFACE_PERF_TEST(TESTNAME, NAME, FRAME_START, FRAME_END) \
  TEST_F(SurfaceAggregatorPerfTest, TESTNAME##_MultiSurfaceTest) {      \
    this->RunPerfTestFromJson("multi_surface_test", #NAME, FRAME_START, \
                              FRAME_END);                               \
  }

MULTI_SURFACE_PERF_TEST(youtube_single_frame, youtube_tab_focused, 1641, 1641)
MULTI_SURFACE_PERF_TEST(youtube_5_frames, youtube_tab_focused, 1641, 1645)

#define TOP_REAL_WORLD_MOBILE_PERF_TEST(NAME, FRAME_START, FRAME_END)      \
  TEST_F(SurfaceAggregatorPerfTest, NAME##_MultiSurfaceTest) {             \
    this->RunPerfTestFromJson("top_real_world_mobile", #NAME, FRAME_START, \
                              FRAME_END);                                  \
  }

TOP_REAL_WORLD_MOBILE_PERF_TEST(amazon_mobile_2018, 0, 224)
TOP_REAL_WORLD_MOBILE_PERF_TEST(youtube_mobile_2018, 0, 122)

#undef TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST
#undef MULTI_SURFACE_PERF_TEST
#undef TOP_REAL_WORLD_MOBILE_PERF_TEST

}  // namespace
}  // namespace viz
