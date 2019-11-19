// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This perf test measures the time from when the display compositor starts
// drawing on the compositor thread to when a swap buffers occurs on the
// GPU main thread. It tests both GLRenderer and SkiaRenderer under
// simple work loads.
//
// Example usage:
//
// $ out/release/viz_perftests --gtest_filter="*RendererPerfTest*" \
//    --use-gpu-in-tests --test-launcher-timeout=300000 \
//    --perf-test-time-ms=240000 --disable_discard_framebuffer=1 \
//    --use_virtualized_gl_contexts=1

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/lap_timer.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display_embedder/gl_output_surface_offscreen.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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

base::TimeDelta TestTimeLimit() {
  static const char kPerfTestTimeMillis[] = "perf-test-time-ms";
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kPerfTestTimeMillis)) {
    const std::string delay_millis_string(
        command_line->GetSwitchValueASCII(kPerfTestTimeMillis));
    int delay_millis;
    if (base::StringToInt(delay_millis_string, &delay_millis) &&
        delay_millis > 0) {
      return base::TimeDelta::FromMilliseconds(delay_millis);
    }
  }
  return base::TimeDelta::FromSeconds(3);
}

class WaitForSwapDisplayClient : public DisplayClient {
 public:
  WaitForSwapDisplayClient() = default;

  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              RenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {
    DCHECK(loop_);
    loop_->Quit();
  }
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id) override {
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

  DISALLOW_COPY_AND_ASSIGN(WaitForSwapDisplayClient);
};

std::unique_ptr<RenderPass> CreateTestRootRenderPass() {
  const RenderPassId id = 1;
  const gfx::Rect output_rect = kSurfaceRect;
  const gfx::Rect damage_rect = kSurfaceRect;
  const gfx::Transform transform_to_root_target;
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  pass->has_transparent_background = false;
  return pass;
}

SharedQuadState* CreateTestSharedQuadState(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    RenderPass* render_pass,
    const gfx::RRectF& rrect) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const gfx::RRectF rounded_corner_bounds = rrect;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  const int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       rounded_corner_bounds, clip_rect, is_clipped,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id);
  return shared_state;
}

template <typename T>
base::span<const uint8_t> MakePixelSpan(const std::vector<T>& vec) {
  return base::make_span(reinterpret_cast<const uint8_t*>(vec.data()),
                         vec.size() * sizeof(T));
}

void DeleteSharedImage(scoped_refptr<ContextProvider> context_provider,
                       gpu::Mailbox mailbox,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, mailbox);
}

TransferableResource CreateTestTexture(
    const gfx::Rect& rect,
    SkColor texel_color,
    bool premultiplied_alpha,
    ClientResourceProvider* child_resource_provider,
    scoped_refptr<ContextProvider> child_context_provider) {
  SkPMColor pixel_color = premultiplied_alpha
                              ? SkPreMultiplyColor(texel_color)
                              : SkPackARGB32NoCheck(SkColorGetA(texel_color),
                                                    SkColorGetR(texel_color),
                                                    SkColorGetG(texel_color),
                                                    SkColorGetB(texel_color));
  size_t num_pixels = static_cast<size_t>(rect.width()) * rect.height();
  std::vector<uint32_t> pixels(num_pixels, pixel_color);

  gpu::SharedImageInterface* sii =
      child_context_provider->SharedImageInterface();
  DCHECK(sii);
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      RGBA_8888, rect.size(), gfx::ColorSpace(),
      gpu::SHARED_IMAGE_USAGE_DISPLAY, MakePixelSpan(pixels));
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token, rect.size(),
      false /* is_overlay_candidate */);
  gl_resource.format = RGBA_8888;
  gl_resource.color_space = gfx::ColorSpace();
  auto release_callback = SingleReleaseCallback::Create(base::BindOnce(
      &DeleteSharedImage, std::move(child_context_provider), mailbox));
  gl_resource.id = child_resource_provider->ImportResource(
      gl_resource, std::move(release_callback));
  return gl_resource;
}

void CreateTestTextureDrawQuad(ResourceId resource_id,
                               const gfx::Rect& rect,
                               SkColor background_color,
                               bool premultiplied_alpha,
                               const SharedQuadState* shared_state,
                               RenderPass* render_pass) {
  const bool needs_blending = true;
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool flipped = false;
  const bool nearest_neighbor = false;
  const float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, resource_id,
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, vertex_opacity, flipped, nearest_neighbor,
               /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
}

void CreateTestTileDrawQuad(ResourceId resource_id,
                            const gfx::Rect& rect,
                            const gfx::Size& texture_size,
                            bool premultiplied_alpha,
                            const SharedQuadState* shared_state,
                            RenderPass* render_pass) {
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

template <typename RendererType>
class RendererPerfTest : public testing::Test {
 public:
  RendererPerfTest()
      : manager_(&shared_bitmap_manager_),
        support_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryFrameSinkId,
            true /* is_root */,
            true /* needs_sync_points */)),
        timer_(/*warmup_laps=*/100,
               /*time_limit=*/TestTimeLimit(),
               /*check_interval=*/10) {}

  // Overloaded for concrete RendererType below.
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      GpuServiceImpl* gpu_service);

  void SetUp() override {
    renderer_settings_.use_skia_renderer =
        std::is_base_of<SkiaRenderer, RendererType>::value;
    if (renderer_settings_.use_skia_renderer)
      printf("Using SkiaRenderer\n");
    else
      printf("Using GLRenderer\n");

    auto* gpu_service = TestGpuServiceHolder::GetInstance()->gpu_service();

    gpu_memory_buffer_manager_ =
        std::make_unique<InProcessGpuMemoryBufferManager>(
            gpu_service->gpu_memory_buffer_factory(),
            gpu_service->sync_point_manager());
    gpu::ImageFactory* image_factory = gpu_service->gpu_image_factory();
    auto* gpu_channel_manager_delegate =
        gpu_service->gpu_channel_manager()->delegate();
    child_context_provider_ = base::MakeRefCounted<VizProcessContextProvider>(
        TestGpuServiceHolder::GetInstance()->task_executor(),
        gpu::kNullSurfaceHandle, gpu_memory_buffer_manager_.get(),
        image_factory, gpu_channel_manager_delegate, renderer_settings_);
    child_context_provider_->BindToCurrentThread();
    constexpr bool sync_token_verification = false;
    child_resource_provider_ =
        std::make_unique<ClientResourceProvider>(sync_token_verification);

    auto output_surface = CreateOutputSurface(gpu_service);
    // WaitForSwapDisplayClient depends on this.
    output_surface->SetNeedsSwapSizeNotifications(true);

    display_ = std::make_unique<Display>(
        &shared_bitmap_manager_, renderer_settings_, kArbitraryFrameSinkId,
        std::move(output_surface),
        /*display_scheduler=*/nullptr, base::ThreadTaskRunnerHandle::Get());
    display_->SetVisible(true);
    display_->Initialize(&client_, manager_.surface_manager());
    display_->Resize(kSurfaceSize);

    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        1.f);
  }

  void TearDown() override {
    std::string story =
        renderer_settings_.use_skia_renderer ? "SkiaRenderer_" : "GLRenderer_";
    story += ::testing::UnitTest::GetInstance()->current_test_info()->name();
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
        "%s.%s.DrawToSwapUs", info->test_case_name(), info->name());
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
    gpu_memory_buffer_manager_.reset();

    display_.reset();
  }

  void DrawFrame(RenderPassList pass_list) {
    CompositorFrame frame = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(pass_list))
                                .SetTransferableResources(resource_list_)
                                .Build();
    support_->SubmitCompositorFrame(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        std::move(frame));
    ASSERT_TRUE(display_->DrawAndSwap());
  }

  void RunSingleTextureQuad() {
    resource_list_.push_back(CreateTestTexture(
        gfx::Rect(kSurfaceSize),
        /*texel_color=*/SkColorSetARGB(128, 0, 255, 0),
        /*premultiplied_alpha=*/false, child_resource_provider_.get(),
        child_context_provider_));

    timer_.Reset();
    do {
      std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass();

      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), kSurfaceRect, pass.get(), gfx::RRectF());

      CreateTestTextureDrawQuad(resource_list_.back().id, kSurfaceRect,
                                /*background_color=*/SK_ColorTRANSPARENT,
                                /*premultiplied_alpha=*/false, shared_state,
                                pass.get());

      RenderPassList pass_list;
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
            gfx::Rect(kTextureSize),
            /*texel_color=*/SkColorSetARGB(128, 0, 255, 0),
            /*premultiplied_alpha=*/false, child_resource_provider_.get(),
            child_context_provider_));
        resource_ids[i][j] = resource_list_.back().id;
      }
    }

    timer_.Reset();
    do {
      std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass();
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), kSurfaceRect, pass.get(), gfx::RRectF());

      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
          CreateTestTextureDrawQuad(
              resource_ids[i][j],
              gfx::Rect(i * kTextureSize.width(), j * kTextureSize.height(),
                        kTextureSize.width(), kTextureSize.height()),
              /*background_color=*/SK_ColorTRANSPARENT,
              /*premultiplied_alpha=*/false, shared_state, pass.get());
        }
      }

      RenderPassList pass_list;
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
          gfx::Rect(kTextureSize),
          /*texel_color=*/SkColorSetARGB(128, 0, 255, 0),
          /*premultiplied_alpha=*/false, child_resource_provider_.get(),
          child_context_provider_));
    } else {
      // Each TileDrawQuad gets its own resource
      for (int i = 0; i < tile_count; ++i) {
        resource_list_.push_back(CreateTestTexture(
            gfx::Rect(kTextureSize),
            /*texel_color=*/SkColorSetARGB(128, 0, 255, 0),
            /*premultiplied_alpha=*/false, child_resource_provider_.get(),
            child_context_provider_));
      }
    }

    timer_.Reset();
    do {
      std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass();
      gfx::Transform current_transform = starting_transform;
      for (int i = 0; i < tile_count; ++i) {
        // Every TileDrawQuad is at at different transform, so always need a new
        // SharedQuadState
        SharedQuadState* shared_state = CreateTestSharedQuadState(
            current_transform, gfx::Rect(kSurfaceSize), pass.get(),
            gfx::RRectF());
        ResourceId resource_id =
            share_resources ? resource_list_[0].id : resource_list_[i].id;
        CreateTestTileDrawQuad(resource_id, gfx::Rect(kTileSize), kTextureSize,
                               /*premultiplied_alpha=*/false, shared_state,
                               pass.get());

        current_transform.ConcatTransform(transform_step);
      }

      RenderPassList pass_list;
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

 protected:
  WaitForSwapDisplayClient client_;
  ParentLocalSurfaceIdAllocator id_allocator_;
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  RendererSettings renderer_settings_;
  std::unique_ptr<Display> display_;
  scoped_refptr<ContextProvider> child_context_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::vector<TransferableResource> resource_list_;
  base::LapTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(RendererPerfTest);
};

template <>
std::unique_ptr<OutputSurface>
RendererPerfTest<SkiaRenderer>::CreateOutputSurface(
    GpuServiceImpl* gpu_service) {
  return SkiaOutputSurfaceImpl::Create(
      std::make_unique<SkiaOutputSurfaceDependencyImpl>(
          gpu_service, gpu::kNullSurfaceHandle),
      renderer_settings_);
}

template <>
std::unique_ptr<OutputSurface>
RendererPerfTest<GLRenderer>::CreateOutputSurface(GpuServiceImpl* gpu_service) {
  gpu::ImageFactory* image_factory = gpu_service->gpu_image_factory();
  auto* gpu_channel_manager_delegate =
      gpu_service->gpu_channel_manager()->delegate();
  auto context_provider = base::MakeRefCounted<VizProcessContextProvider>(
      TestGpuServiceHolder::GetInstance()->task_executor(),
      gpu::kNullSurfaceHandle, gpu_memory_buffer_manager_.get(), image_factory,
      gpu_channel_manager_delegate, renderer_settings_);
  context_provider->BindToCurrentThread();
  return std::make_unique<GLOutputSurfaceOffscreen>(
      std::move(context_provider));
}

using RendererTypes = ::testing::Types<GLRenderer, SkiaRenderer>;
TYPED_TEST_SUITE(RendererPerfTest, RendererTypes);

TYPED_TEST(RendererPerfTest, SingleTextureQuad) {
  this->RunSingleTextureQuad();
}

TYPED_TEST(RendererPerfTest, TextureQuads5x5) {
  this->RunTextureQuads5x5();
}

TYPED_TEST(RendererPerfTest, RotatedTileQuadsShared) {
  this->RunRotatedTileQuadsShared();
}

TYPED_TEST(RendererPerfTest, RotatedTileQuads) {
  this->RunRotatedTileQuads();
}

}  // namespace viz
