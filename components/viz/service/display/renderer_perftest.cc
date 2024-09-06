// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This perf test measures the time from when the display compositor starts
// drawing on the compositor thread to when a swap buffers occurs on the
// GPU main thread.
//
// Example usage:
//
// $ out/release/viz_perftests --gtest_filter="RendererPerfTest*" \
//    --use-gpu-in-tests --test-launcher-timeout=300000 \
//    --perf-test-time-ms=240000 --disable_discard_framebuffer=1 \
//    --use_virtualized_gl_contexts=1

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/viz_perftest.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"

namespace viz {

namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(3, 3);
static constexpr gfx::Size kSurfaceSize(1000, 1000);
static constexpr gfx::Rect kSurfaceRect(kSurfaceSize);

constexpr char kMetricPrefixRenderer[] = "Renderer.";
constexpr char kMetricFps[] = "frames_per_second";

perf_test::PerfResultReporter SetUpRendererReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixRenderer, story);
  reporter.RegisterImportantMetric(kMetricFps, "fps");
  return reporter;
}

class WaitForSwapDisplayClient : public DisplayClient {
 public:
  WaitForSwapDisplayClient() = default;

  WaitForSwapDisplayClient(const WaitForSwapDisplayClient&) = delete;
  WaitForSwapDisplayClient& operator=(const WaitForSwapDisplayClient&) = delete;

  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      AggregatedRenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {
    DCHECK(loop_);
    loop_->Quit();
  }
  void DisplayAddChildWindowToBrowser(
      gpu::SurfaceHandle child_window) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) override {
    return BeginFrameArgs::MinInterval();
  }

  void WaitForSwap() {
    DCHECK(!loop_);
    loop_ = std::make_unique<base::RunLoop>();
    loop_->Run();
    loop_.reset();
  }

 private:
  std::unique_ptr<base::RunLoop> loop_;
};

std::unique_ptr<CompositorRenderPass> CreateTestRootRenderPass() {
  const CompositorRenderPassId id{1};
  const gfx::Rect output_rect = kSurfaceRect;
  const gfx::Rect damage_rect = kSurfaceRect;
  const gfx::Transform transform_to_root_target;
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  pass->has_transparent_background = false;
  return pass;
}

SharedQuadState* CreateTestSharedQuadState(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    CompositorRenderPass* render_pass,
    const gfx::MaskFilterInfo& mask_filter_info) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  const int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       mask_filter_info, /*clip_rect=*/std::nullopt,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);
  return shared_state;
}

void DeleteSharedImage(
    scoped_refptr<RasterContextProvider> context_provider,
    scoped_refptr<gpu::ClientSharedImage> client_shared_image,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, std::move(client_shared_image));
}

TransferableResource CreateTestTexture(
    const gfx::Size& size,
    SkColor4f texel_color,
    bool premultiplied_alpha,
    ClientResourceProvider* child_resource_provider,
    scoped_refptr<RasterContextProvider> child_context_provider) {
  using SkPMColor4f = SkRGBA4f<kPremul_SkAlphaType>;
  const SkPMColor4f pixel_color =
      premultiplied_alpha ? texel_color.premul()
                          : SkPMColor4f{texel_color.fR, texel_color.fG,
                                        texel_color.fB, texel_color.fA};

  size_t num_pixels = static_cast<size_t>(size.width()) * size.height();
  std::vector<SkPMColor4f> pixels(num_pixels, pixel_color);

  gpu::SharedImageInterface* sii =
      child_context_provider->SharedImageInterface();
  DCHECK(sii);
  auto client_shared_image = sii->CreateSharedImage(
      {SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, "TestLabel"},
      base::as_byte_span(pixels));
  gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();

  TransferableResource gl_resource = TransferableResource::MakeGpu(
      client_shared_image, GL_TEXTURE_2D, sync_token, size,
      SinglePlaneFormat::kRGBA_8888, false /* is_overlay_candidate */);
  gl_resource.color_space = gfx::ColorSpace();
  auto release_callback =
      base::BindOnce(&DeleteSharedImage, std::move(child_context_provider),
                     std::move(client_shared_image));
  gl_resource.id = child_resource_provider->ImportResource(
      gl_resource, std::move(release_callback));
  return gl_resource;
}

void CreateTestTextureDrawQuad(ResourceId resource_id,
                               const gfx::Rect& rect,
                               SkColor4f background_color,
                               bool premultiplied_alpha,
                               const SharedQuadState* shared_state,
                               CompositorRenderPass* render_pass) {
  const bool needs_blending = true;
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool flipped = false;
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();

  quad->SetNew(shared_state, rect, rect, needs_blending, resource_id,
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, flipped, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
}

void CreateTestTileDrawQuad(ResourceId resource_id,
                            const gfx::Rect& rect,
                            const gfx::Size& texture_size,
                            bool premultiplied_alpha,
                            const SharedQuadState* shared_state,
                            CompositorRenderPass* render_pass) {
  // TileDrawQuads are non-normalized texture coords, so assume it's 1-1 with
  // the visible rect.
  const gfx::RectF tex_coord_rect(rect);
  const bool needs_blending = true;
  const bool nearest_neighbor = false;
  const bool force_anti_aliasing_off = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, resource_id,
               tex_coord_rect, texture_size, premultiplied_alpha,
               nearest_neighbor, force_anti_aliasing_off);
}

}  // namespace

class RendererPerfTest : public VizPerfTest {
 public:
  RendererPerfTest()
      : manager_(FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)),
        support_(
            std::make_unique<CompositorFrameSinkSupport>(nullptr,
                                                         &manager_,
                                                         kArbitraryFrameSinkId,
                                                         true /* is_root */)) {}

  RendererPerfTest(const RendererPerfTest&) = delete;
  RendererPerfTest& operator=(const RendererPerfTest&) = delete;

  std::unique_ptr<SkiaOutputSurface> CreateOutputSurface(
      GpuServiceImpl* gpu_service,
      DisplayCompositorMemoryAndTaskController* display_controller) {
    return SkiaOutputSurfaceImpl::Create(display_controller, renderer_settings_,
                                         &debug_settings_);
  }

  void SetUp() override {
    enable_pixel_output_ = std::make_unique<gl::DisableNullDrawGLBindings>();

#if BUILDFLAG(IS_ANDROID)
    renderer_settings_.color_space = gfx::ColorSpace::CreateSRGB();
    renderer_settings_.initial_screen_size = kSurfaceSize;
#endif

    auto* gpu_service = TestGpuServiceHolder::GetInstance()->gpu_service();

    child_context_provider_ =
        base::MakeRefCounted<TestInProcessContextProvider>(
            TestContextType::kSoftwareRaster, /*support_locking=*/false);
    child_context_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    auto skia_deps = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
        gpu_service, gpu::kNullSurfaceHandle);
    auto display_controller =
        std::make_unique<DisplayCompositorMemoryAndTaskController>(
            std::move(skia_deps));

    auto output_surface =
        CreateOutputSurface(gpu_service, display_controller.get());
    // WaitForSwapDisplayClient depends on this.
    output_surface->SetNeedsSwapSizeNotifications(true);
    auto overlay_processor = std::make_unique<OverlayProcessorStub>();
    display_ = std::make_unique<Display>(
        &shared_bitmap_manager_, /*shared_image_manager=*/nullptr,
        /*sync_point_manager=*/nullptr, /*gpu_scheduler=*/nullptr,
        renderer_settings_, &debug_settings_, kArbitraryFrameSinkId,
        std::move(display_controller), std::move(output_surface),
        std::move(overlay_processor),
        /*display_scheduler=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    display_->SetVisible(true);
    display_->Initialize(&client_, manager_.surface_manager());
    display_->Resize(kSurfaceSize);

    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  }

  void TearDown() override {
    std::string story =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    auto reporter = SetUpRendererReporter(story);
    reporter.AddResult(kMetricFps, timer_.LapsPerSecond());

    auto* histogram = base::StatisticsRecorder::FindHistogram(
        "Compositing.Display.DrawToSwapUs");
    ASSERT_TRUE(histogram) << "Likely no swap_start time was returned to "
                              "Display::DidReceiveSwapBuffersAck.";

    // There is no way to clear a histogram. Part of the reason for this is that
    // histogram lookups are cached in a pointer once per process for
    // efficiency.
    //
    // To separate histogram results from different test runs, we sample the
    // delta between successive runs and import the sample into a new unique
    // histogram that can be graphed.
    auto* info = testing::UnitTest::GetInstance()->current_test_info();
    std::string temp_name = base::StringPrintf(
        "%s.%s.DrawToSwapUs", info->test_suite_name(), info->name());
    auto samples = histogram->SnapshotDelta();
    base::HistogramBase* temp_histogram =
        base::Histogram::FactoryMicrosecondsTimeGet(
            temp_name, Display::kDrawToSwapMin, Display::kDrawToSwapMax,
            Display::kDrawToSwapUsBuckets, base::Histogram::kNoFlags);
    temp_histogram->AddSamples(*samples);
    std::string output;
    temp_histogram->WriteAscii(&output);
    printf("%s\n", output.c_str());

    // Tear down the client side context provider, etc.
    for (const auto& transferable_resource : resource_list_) {
      child_resource_provider_->RemoveImportedResource(
          transferable_resource.id);
    }
    child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_.reset();
    child_context_provider_.reset();

    display_.reset();
  }

  void DrawFrame(CompositorRenderPassList pass_list) {
    CompositorFrame frame = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(pass_list))
                                .SetTransferableResources(resource_list_)
                                .Build();
    support_->SubmitCompositorFrame(id_allocator_.GetCurrentLocalSurfaceId(),
                                    std::move(frame));
    ASSERT_TRUE(display_->DrawAndSwap(
        {base::TimeTicks::Now(), base::TimeTicks::Now()}));
  }

  ResourceId MapResourceId(base::flat_map<ResourceId, ResourceId>* resource_map,
                           ResourceId recorded_id,
                           const gfx::Size& texture_size,
                           SkColor4f texel_color,
                           bool premultiplied_alpha) {
    DCHECK(resource_map);
    ResourceId actual_id;
    if (resource_map->find(recorded_id) == resource_map->end()) {
      resource_list_.push_back(CreateTestTexture(
          texture_size, texel_color, premultiplied_alpha,
          child_resource_provider_.get(), child_context_provider_));
      actual_id = resource_list_.back().id;
      (*resource_map)[recorded_id] = actual_id;
    } else {
      actual_id = (*resource_map)[recorded_id];
    }
    return actual_id;
  }

  void SetUpRenderPassListResources(
      CompositorRenderPassList* render_pass_list) {
    base::flat_map<ResourceId, ResourceId> resource_map;
    for (auto& render_pass : *render_pass_list) {
      for (auto* quad : render_pass->quad_list) {
        if (quad->resources.count == 0)
          continue;
        switch (quad->material) {
          case DrawQuad::Material::kTiledContent: {
            TileDrawQuad* tile_quad = reinterpret_cast<TileDrawQuad*>(quad);
            ResourceId recorded_id = tile_quad->resource_id();
            ResourceId actual_id = this->MapResourceId(
                &resource_map, recorded_id, tile_quad->texture_size,
                SkColor4f{0.0f, 1.0f, 0.0f, 0.5f}, tile_quad->is_premultiplied);
            tile_quad->resources.ids[TileDrawQuad::kResourceIdIndex] =
                actual_id;
          } break;
          case DrawQuad::Material::kTextureContent: {
            TextureDrawQuad* texture_quad =
                reinterpret_cast<TextureDrawQuad*>(quad);
            ResourceId recorded_id = texture_quad->resource_id();
            ResourceId actual_id = this->MapResourceId(
                &resource_map, recorded_id, texture_quad->rect.size(),
                SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
                texture_quad->premultiplied_alpha);
            texture_quad->resources.ids[TextureDrawQuad::kResourceIdIndex] =
                actual_id;
          } break;
          default:
            ASSERT_TRUE(false);
        }
      }
    }
  }

  void RunSingleTextureQuad() {
    resource_list_.push_back(CreateTestTexture(
        kSurfaceSize,
        /*texel_color=*/SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
        /*premultiplied_alpha=*/false, child_resource_provider_.get(),
        child_context_provider_));

    timer_.Reset();
    do {
      std::unique_ptr<CompositorRenderPass> pass = CreateTestRootRenderPass();

      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), kSurfaceRect, pass.get(), gfx::MaskFilterInfo());

      CreateTestTextureDrawQuad(resource_list_.back().id, kSurfaceRect,
                                /*background_color=*/SkColors::kTransparent,
                                /*premultiplied_alpha=*/false, shared_state,
                                pass.get());

      CompositorRenderPassList pass_list;
      pass_list.push_back(std::move(pass));

      DrawFrame(std::move(pass_list));
      client_.WaitForSwap();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
  }

  void RunTextureQuads5x5() {
    const gfx::Size kTextureSize =
        ScaleToCeiledSize(kSurfaceSize, /*x_scale=*/0.2, /*y_scale=*/0.2);
    ResourceId resource_ids[5][5];
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++) {
        resource_list_.push_back(CreateTestTexture(
            kTextureSize,
            /*texel_color=*/SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
            /*premultiplied_alpha=*/false, child_resource_provider_.get(),
            child_context_provider_));
        resource_ids[i][j] = resource_list_.back().id;
      }
    }

    timer_.Reset();
    do {
      std::unique_ptr<CompositorRenderPass> pass = CreateTestRootRenderPass();
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), kSurfaceRect, pass.get(), gfx::MaskFilterInfo());

      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
          CreateTestTextureDrawQuad(
              resource_ids[i][j],
              gfx::Rect(i * kTextureSize.width(), j * kTextureSize.height(),
                        kTextureSize.width(), kTextureSize.height()),
              /*background_color=*/SkColors::kTransparent,
              /*premultiplied_alpha=*/false, shared_state, pass.get());
        }
      }

      CompositorRenderPassList pass_list;
      pass_list.push_back(std::move(pass));
      DrawFrame(std::move(pass_list));

      client_.WaitForSwap();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
  }

  void RunTextureQuads5x5SameTex() {
    const gfx::Size kTextureSize =
        ScaleToCeiledSize(kSurfaceSize, /*x_scale=*/0.2, /*y_scale=*/0.2);
    ResourceId resource_id;
    resource_list_.push_back(CreateTestTexture(
        kTextureSize,
        /*texel_color=*/SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
        /*premultiplied_alpha=*/false, child_resource_provider_.get(),
        child_context_provider_));
    resource_id = resource_list_.back().id;

    timer_.Reset();
    do {
      std::unique_ptr<CompositorRenderPass> pass = CreateTestRootRenderPass();
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), kSurfaceRect, pass.get(), gfx::MaskFilterInfo());

      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
          CreateTestTextureDrawQuad(
              resource_id,
              gfx::Rect(i * kTextureSize.width(), j * kTextureSize.height(),
                        kTextureSize.width(), kTextureSize.height()),
              /*background_color=*/SkColors::kTransparent,
              /*premultiplied_alpha=*/false, shared_state, pass.get());
        }
      }

      CompositorRenderPassList pass_list;
      pass_list.push_back(std::move(pass));
      DrawFrame(std::move(pass_list));

      client_.WaitForSwap();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
  }

  void RunTileQuads(int tile_count,
                    const gfx::Transform& starting_transform,
                    const gfx::Transform& transform_step,
                    bool share_resources) {
    const gfx::Size kTextureSize =
        ScaleToCeiledSize(kSurfaceSize, /*x_scale=*/0.2, /*y_scale=*/0.2);
    // Make the tile size slightly smaller than the backing texture to simulate
    // undefined bottom/right edges
    const gfx::Rect kTileSize(kTextureSize.width() - 4,
                              kTextureSize.height() - 4);
    if (share_resources) {
      // A single tiled resource referenced by each TileDrawQuad
      resource_list_.push_back(CreateTestTexture(
          kTextureSize,
          /*texel_color=*/SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
          /*premultiplied_alpha=*/false, child_resource_provider_.get(),
          child_context_provider_));
    } else {
      // Each TileDrawQuad gets its own resource
      for (int i = 0; i < tile_count; ++i) {
        resource_list_.push_back(CreateTestTexture(
            kTextureSize,
            /*texel_color=*/SkColor4f{0.0f, 1.0f, 0.0f, 0.5f},
            /*premultiplied_alpha=*/false, child_resource_provider_.get(),
            child_context_provider_));
      }
    }

    timer_.Reset();
    do {
      std::unique_ptr<CompositorRenderPass> pass = CreateTestRootRenderPass();
      gfx::Transform current_transform = starting_transform;
      for (int i = 0; i < tile_count; ++i) {
        // Every TileDrawQuad is at at different transform, so always need a new
        // SharedQuadState
        SharedQuadState* shared_state = CreateTestSharedQuadState(
            current_transform, gfx::Rect(kSurfaceSize), pass.get(),
            gfx::MaskFilterInfo());
        ResourceId resource_id =
            share_resources ? resource_list_[0].id : resource_list_[i].id;
        CreateTestTileDrawQuad(resource_id, gfx::Rect(kTileSize), kTextureSize,
                               /*premultiplied_alpha=*/false, shared_state,
                               pass.get());

        current_transform.PostConcat(transform_step);
      }

      CompositorRenderPassList pass_list;
      pass_list.push_back(std::move(pass));
      DrawFrame(std::move(pass_list));

      client_.WaitForSwap();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
  }

  void RunRotatedTileQuads(bool share_resources) {
    gfx::Transform start;
    start.Translate(-300.f, -300.f);
    gfx::Transform inc;
    inc.Rotate(1.f);
    this->RunTileQuads(350, start, inc, share_resources);
  }

  void RunRotatedTileQuadsShared() {
    this->RunRotatedTileQuads(/*share_resources=*/true);
  }

  void RunRotatedTileQuads() {
    this->RunRotatedTileQuads(/*share_resources=*/false);
  }

  void RunSingleRenderPassListFromJSON(const std::string& tag,
                                       const std::string& site,
                                       uint32_t year,
                                       size_t index) {
    CompositorRenderPassList render_pass_list;
    ASSERT_TRUE(CompositorRenderPassListFromJSON(tag, site, year, index,
                                                 &render_pass_list));
    ASSERT_FALSE(render_pass_list.empty());
    // Root render pass damage needs to match the output surface size.
    auto& last_render_pass = *render_pass_list.back();
    last_render_pass.damage_rect = last_render_pass.output_rect;

    this->SetUpRenderPassListResources(&render_pass_list);

    timer_.Reset();
    do {
      CompositorRenderPassList local_list;
      CompositorRenderPass::CopyAllForTest(render_pass_list, &local_list);
      DrawFrame(std::move(local_list));
      client_.WaitForSwap();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
  }

 protected:
  WaitForSwapDisplayClient client_;
  ParentLocalSurfaceIdAllocator id_allocator_;
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  RendererSettings renderer_settings_;
  DebugRendererSettings debug_settings_;
  std::unique_ptr<Display> display_;
  scoped_refptr<RasterContextProvider> child_context_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::vector<TransferableResource> resource_list_;
  std::unique_ptr<gl::DisableNullDrawGLBindings> enable_pixel_output_;
};

TEST_F(RendererPerfTest, SingleTextureQuad) {
  this->RunSingleTextureQuad();
}

TEST_F(RendererPerfTest, TextureQuads5x5) {
  this->RunTextureQuads5x5();
}

TEST_F(RendererPerfTest, TextureQuads5x5SameTex) {
  this->RunTextureQuads5x5SameTex();
}

TEST_F(RendererPerfTest, RotatedTileQuadsShared) {
  this->RunRotatedTileQuadsShared();
}

TEST_F(RendererPerfTest, RotatedTileQuads) {
  this->RunRotatedTileQuads();
}

#define TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(SITE, FRAME)              \
  TEST_F(RendererPerfTest, SITE) {                                          \
    this->RunSingleRenderPassListFromJSON(/*tag=*/"top_real_world_desktop", \
                                          /*site=*/#SITE, /*year=*/2018,    \
                                          /*frame_index=*/FRAME);           \
  }

TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(accu_weather, 298)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(amazon, 30)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(blogspot, 56)
// TODO(zmo): Fix the crash and enable cnn test.
// TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(cnn, 479)
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
// TODO(zmo): Fix the crash and enable twitter test.
// TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(twitter, 352)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(wikipedia, 48)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(wordpress, 75)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(yahoo_answers, 74)
TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST(yahoo_sports, 269)

#undef TOP_REAL_WORLD_DESKTOP_RENDERER_PERF_TEST

}  // namespace viz
