// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/display.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "cc/test/scheduler_test_common.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"
#include "components/viz/service/display/delegated_ink_trail_data.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/delegated_ink_point_renderer_skia_for_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "components/viz/test/viz_test_suite.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/delegated_ink_point.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"
#include "ui/gfx/overlay_transform.h"

using testing::_;
using testing::AnyNumber;

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(3, 3);
static constexpr FrameSinkId kAnotherFrameSinkId(4, 4);
static constexpr FrameSinkId kAnotherFrameSinkId2(5, 5);

const uint64_t kBeginFrameSourceId = 1337;

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  gfx::Rect damage_rect() const { return damage_rect_; }
  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
};

class TestDisplayScheduler : public DisplayScheduler {
 public:
  explicit TestDisplayScheduler(BeginFrameSource* begin_frame_source,
                                base::SingleThreadTaskRunner* task_runner)
      : DisplayScheduler(begin_frame_source,
                         task_runner,
                         PendingSwapParams(1)) {}

  ~TestDisplayScheduler() override = default;

  void OnDisplayDamaged(SurfaceId surface_id) override {
    damaged_ = true;
    needs_draw_ = true;
  }

  void DidSwapBuffers() override { swapped_ = true; }

  void ResetDamageForTest() { damaged_ = false; }

  bool damaged() const { return damaged_; }
  bool swapped() const { return swapped_; }
  void reset_swapped_for_test() { swapped_ = false; }

 private:
  bool damaged_ = false;
  bool swapped_ = false;
};

class StubDisplayClient : public DisplayClient {
 public:
  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      AggregatedRenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void DisplayAddChildWindowToBrowser(
      gpu::SurfaceHandle child_window) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) override {
    return BeginFrameArgs::MinInterval();
  }
};

void CopyCallback(bool* called,
                  base::OnceClosure finished,
                  std::unique_ptr<CopyOutputResult> result) {
  *called = true;
  std::move(finished).Run();
}

gfx::SwapTimings GetTestSwapTimings() {
  base::TimeTicks now = base::TimeTicks::Now();
  return gfx::SwapTimings{now, now};
}

std::string PostTestCaseName(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "UseMapRect" : "RectExpansion";
}

}  // namespace

class DisplayTest : public testing::Test {
 public:
  DisplayTest()
      : manager_(FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)),
        support_(
            std::make_unique<CompositorFrameSinkSupport>(nullptr,
                                                         &manager_,
                                                         kArbitraryFrameSinkId,
                                                         true /* is_root */)),
        task_runner_(new base::NullTaskRunner),
        client_(std::make_unique<StubDisplayClient>()) {}

  ~DisplayTest() override {}

  void SetUpSoftwareDisplay(const RendererSettings& settings) {
    std::unique_ptr<FakeSoftwareOutputSurface> output_surface;
    auto device = std::make_unique<TestSoftwareOutputDevice>();
    software_output_device_ = device.get();
    output_surface =
        std::make_unique<FakeSoftwareOutputSurface>(std::move(device));
    output_surface_ = output_surface.get();

    CreateDisplaySchedulerAndDisplay(settings, kArbitraryFrameSinkId,
                                     std::move(output_surface));
  }

  void SetUpGpuDisplay(const RendererSettings& settings) {
    scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
    provider->BindToCurrentSequence();
    std::unique_ptr<FakeSkiaOutputSurface> skia_output_surface =
        FakeSkiaOutputSurface::Create3d(std::move(provider));
    skia_output_surface_ = skia_output_surface.get();

    CreateDisplaySchedulerAndDisplay(settings, kArbitraryFrameSinkId,
                                     std::move(skia_output_surface));
  }

  void CreateDisplaySchedulerAndDisplay(
      const RendererSettings& settings,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<OutputSurface> output_surface) {
    begin_frame_source_ = std::make_unique<StubBeginFrameSource>();
    auto scheduler = std::make_unique<TestDisplayScheduler>(
        begin_frame_source_.get(), task_runner_.get());
    scheduler_ = scheduler.get();
    display_ = CreateDisplay(settings, kArbitraryFrameSinkId,
                             std::move(scheduler), std::move(output_surface));
    manager_.RegisterBeginFrameSource(begin_frame_source_.get(),
                                      kArbitraryFrameSinkId);
  }

  std::unique_ptr<Display> CreateDisplay(
      const RendererSettings& settings,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<DisplayScheduler> scheduler,
      std::unique_ptr<OutputSurface> output_surface) {
    auto overlay_processor = std::make_unique<OverlayProcessorStub>();
    // Normally display will need to take ownership of a
    // DisplayCompositorMemoryAndTaskController in order to keep it alive to
    // share between the output surface and the overlay processor. In this case
    // the overlay processor is a stub and the output surface is test only as
    // well, so there is no need to pass in a real
    // DisplayCompositorMemoryAndTaskController.
    auto display = std::make_unique<Display>(
        &shared_bitmap_manager_, &shared_image_manager_, &sync_point_manager_,
        &gpu_scheduler_, settings, &debug_settings_, frame_sink_id,
        nullptr /* DisplayCompositorMemoryAndTaskController */,
        std::move(output_surface), std::move(overlay_processor),
        std::move(scheduler), task_runner_);
    display->SetVisible(true);
    return display;
  }

  bool ShouldSendBeginFrame(CompositorFrameSinkSupport* support,
                            base::TimeTicks frame_time) {
    return support->ShouldSendBeginFrame(frame_time, base::Seconds(0));
  }

  void UpdateBeginFrameTime(CompositorFrameSinkSupport* support,
                            base::TimeTicks frame_time) {
    support->last_frame_time_ = frame_time;
    support->frame_timing_details_.clear();
  }

 protected:
  void TearDown() override {
    // Only call UnregisterBeginFrameSource if SetupDisplay has been called.
    if (begin_frame_source_)
      manager_.UnregisterBeginFrameSource(begin_frame_source_.get());
  }

  void SubmitCompositorFrame(CompositorRenderPassList* pass_list,
                             const LocalSurfaceId& local_surface_id) {
    CompositorFrame frame = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(*pass_list))
                                .Build();
    pass_list->clear();

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  void ResetDamageForTest() { scheduler_->ResetDamageForTest(); }

  void RunUntilIdle() { VizTestSuite::RunUntilIdle(); }

  void LatencyInfoCapTest(bool over_capacity);

  size_t pending_presentation_group_timings_size() {
    return display_->pending_presentation_group_timings_.size();
  }

  DebugRendererSettings debug_settings_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};
  FrameSinkManagerImpl manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  ParentLocalSurfaceIdAllocator id_allocator_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  std::unique_ptr<StubDisplayClient> client_;  // Must outlive `display_`.
  std::unique_ptr<Display> display_;
  raw_ptr<TestSoftwareOutputDevice> software_output_device_ = nullptr;
  raw_ptr<FakeSoftwareOutputSurface> output_surface_ = nullptr;
  raw_ptr<FakeSkiaOutputSurface> skia_output_surface_ = nullptr;
  raw_ptr<TestDisplayScheduler> scheduler_ = nullptr;
};

// Check that frame is damaged and swapped only under correct conditions.
TEST_F(DisplayTest, DisplayDamaged) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  gfx::ColorSpace color_space_1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space_2 = gfx::ColorSpace::CreateSRGBLinear();
  gfx::DisplayColorSpaces color_spaces_1(color_space_1);
  gfx::DisplayColorSpaces color_spaces_2(color_space_2);
  display_->SetDisplayColorSpaces(color_spaces_1);

  EXPECT_FALSE(scheduler_->damaged());
  id_allocator_.GenerateId();
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  display_->Resize(gfx::Size(100, 100));

  // First draw from surface should have full damage.
  CompositorRenderPassList pass_list;
  auto pass = CompositorRenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = CompositorRenderPassId{1u};
  pass_list.push_back(std::move(pass));

  ResetDamageForTest();
  SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
  EXPECT_TRUE(scheduler_->damaged());

  EXPECT_FALSE(scheduler_->swapped());
  EXPECT_EQ(0u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::ColorSpace(), output_surface_->last_reshape_color_space());
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
  EXPECT_TRUE(scheduler_->swapped());
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::Size(100, 100),
            software_output_device_->viewport_pixel_size());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), software_output_device_->damage_rect());

  // Only a small area is damaged but the color space changes which should
  // result in full damage.
  {
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
    display_->SetDisplayColorSpaces(color_spaces_2);
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_EQ(color_space_2, output_surface_->last_reshape_color_space());
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());

    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Same frame as above but no color space change. Only partial area should be
  // drawn.
  {
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    EXPECT_EQ(color_space_2, output_surface_->last_reshape_color_space());
    display_->SetDisplayColorSpaces(color_spaces_2);
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_EQ(color_space_2, output_surface_->last_reshape_color_space());
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(10, 10, 1, 1), software_output_device_->damage_rect());

    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Pass has no damage so shouldn't be swapped.
  {
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = CompositorRenderPassId{1u};

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
  }

  // Pass is wrong size so shouldn't be swapped. However, damage should
  // result in latency info being stored for the next swap.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);

    ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 99, 99);
    constexpr gfx::Rect kDamageRect(10, 10, 10, 10);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .AddLatencyInfo(ui::LatencyInfo())
                                .Build();

    support_->SubmitCompositorFrame(id_allocator_.GetCurrentLocalSurfaceId(),
                                    std::move(frame));
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
  }

  // Previous frame wasn't swapped, so next swap should have full damage.
  {
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = CompositorRenderPassId{1u};

    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());

    EXPECT_EQ(1u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Pass has copy output request so should be swapped.
  {
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    base::RunLoop copy_run_loop;
    bool copy_called = false;
    pass->copy_requests.push_back(std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA,
        CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(&CopyCallback, &copy_called,
                       copy_run_loop.QuitClosure())));
    pass->id = CompositorRenderPassId{1u};

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(5u, output_surface_->num_sent_frames());
    copy_run_loop.Run();
    EXPECT_TRUE(copy_called);
  }

  // Pass has no damage, so shouldn't be swapped and latency info should be
  // discarded.
  {
    ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 100, 100);
    constexpr gfx::Rect kDamageRect(10, 10, 0, 0);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .AddLatencyInfo(ui::LatencyInfo())
                                .Build();
    frame.metadata.latency_info.push_back(ui::LatencyInfo());

    support_->SubmitCompositorFrame(id_allocator_.GetCurrentLocalSurfaceId(),
                                    std::move(frame));
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(5u, output_surface_->num_sent_frames());
  }

  // DisableSwapUntilResize() should cause a swap if no frame was swapped at the
  // previous size.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
    scheduler_->reset_swapped_for_test();
    display_->Resize(gfx::Size(200, 200));
    EXPECT_FALSE(scheduler_->swapped());
    EXPECT_EQ(5u, output_surface_->num_sent_frames());
    ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 200, 200);
    constexpr gfx::Rect kDamageRect(10, 10, 10, 10);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .Build();

    support_->SubmitCompositorFrame(id_allocator_.GetCurrentLocalSurfaceId(),
                                    std::move(frame));
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DisableSwapUntilResize(base::OnceClosure());
    display_->Resize(gfx::Size(100, 100));
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(6u, output_surface_->num_sent_frames());
    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Surface that's damaged completely should be resized and swapped.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.0f);
    pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(0, 0, 99, 99);
    pass->id = CompositorRenderPassId{1u};

    pass_list.push_back(std::move(pass));
    ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    scheduler_->reset_swapped_for_test();
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_TRUE(scheduler_->swapped());
    EXPECT_EQ(7u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }
}

// Verifies latency info is stored only up to a limit if a swap fails.
void DisplayTest::LatencyInfoCapTest(bool over_capacity) {
  SetUpSoftwareDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id(id_allocator_.GetCurrentLocalSurfaceId());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));

  // Start off with a successful swap so output_surface_->last_sent_frame() is
  // valid.
  constexpr gfx::Rect kOutputRect(0, 0, 100, 100);
  constexpr gfx::Rect kDamageRect(10, 10, 1, 1);
  CompositorFrame frame1 =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame1));

  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());

  // Resize so the swap fails even though there's damage, which triggers
  // the case where we store latency info to append to a future swap.
  display_->Resize(gfx::Size(200, 200));

  // This is the same as LatencyInfo::kMaxLatencyInfoNumber.
  const size_t max_latency_info_count = 100;
  size_t latency_count = max_latency_info_count;
  if (over_capacity)
    latency_count++;
  std::vector<ui::LatencyInfo> latency_info(latency_count, ui::LatencyInfo());

  CompositorFrame frame2 = CompositorFrameBuilder()
                               .AddRenderPass(kOutputRect, kDamageRect)
                               .AddLatencyInfos(std::move(latency_info))
                               .Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame2));

  EXPECT_TRUE(
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()}));
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());

  // Run a successful swap and verify whether or not LatencyInfo was discarded.
  display_->Resize(gfx::Size(100, 100));
  CompositorFrame frame3 =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame3));
  EXPECT_TRUE(
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()}));

  // Verify whether or not LatencyInfo was dropped.
  size_t expected_size = 0;
  if (!over_capacity)
    expected_size += max_latency_info_count;
  EXPECT_EQ(2u, output_surface_->num_sent_frames());
  EXPECT_EQ(expected_size,
            output_surface_->last_sent_frame()->latency_info.size());
}

TEST_F(DisplayTest, UnderLatencyInfoCap) {
  LatencyInfoCapTest(false);
}

TEST_F(DisplayTest, OverLatencyInfoCap) {
  LatencyInfoCapTest(true);
}

TEST_F(DisplayTest, DisableSwapUntilResize) {
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id1(id_allocator_.GetCurrentLocalSurfaceId());
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id2(id_allocator_.GetCurrentLocalSurfaceId());

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id1, 1.f);
  display_->Resize(gfx::Size(100, 100));

  {
    CompositorRenderPassList pass_list;
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id1);
  }

  EXPECT_FALSE(scheduler_->swapped());

  // DisableSwapUntilResize() should trigger a swap because we have a frame of
  // the correct size and haven't swapped at that size yet.
  bool swap_callback_run = false;
  display_->DisableSwapUntilResize(base::BindLambdaForTesting(
      [&swap_callback_run]() { swap_callback_run = true; }));
  EXPECT_TRUE(scheduler_->swapped());

  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = GetTestSwapTimings();
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  display_->DidReceiveSwapBuffersAck(params,
                                     /*release_fence=*/gfx::GpuFenceHandle());
  EXPECT_TRUE(swap_callback_run);

  display_->Resize(gfx::Size(150, 150));
  scheduler_->reset_swapped_for_test();

  // DisableSwapUntilResize() won't trigger a swap because there is no frame
  // of the correct size to draw.
  display_->SetLocalSurfaceId(local_surface_id2, 1.f);
  display_->DisableSwapUntilResize(base::OnceClosure());
  EXPECT_FALSE(scheduler_->swapped());
  display_->Resize(gfx::Size(200, 200));

  {
    CompositorRenderPassList pass_list;
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id2);
  }

  // DrawAndSwap() should trigger a swap at current size.
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  EXPECT_TRUE(scheduler_->swapped());
  scheduler_->reset_swapped_for_test();

  // DisableSwapUntilResize() won't trigger another swap because we already
  // swapped a frame at the current size.
  display_->DisableSwapUntilResize(base::OnceClosure());
  EXPECT_FALSE(scheduler_->swapped());
}

TEST_F(DisplayTest, BackdropFilterTest) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceId());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id1(6,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id1(kAnotherFrameSinkId, sub_local_surface_id1);
  auto sub_support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, /*is_root=*/false);

  // Create frame sink for another sub surface.
  const LocalSurfaceId sub_local_surface_id2(7,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id2(kAnotherFrameSinkId2, sub_local_surface_id2);
  auto sub_support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId2, /*is_root=*/false);

  // Main surface M, damage D, sub-surface B with backdrop filter.
  //   +-----------+
  //   | +----+   M|
  //   | |B +-|-+  |
  //   | +--|-+ |  |
  //   |    |  D|  |
  //   |    +---+  |
  //   +-----------+
  const gfx::Size display_size(100, 100);
  const gfx::Rect damage_rect(20, 20, 40, 40);
  display_->Resize(display_size);
  const gfx::Rect sub_surface_rect(5, 5, 25, 25);
  const gfx::Rect no_damage;

  CompositorRenderPassId::Generator render_pass_id_generator;
  for (size_t frame_num = 1; frame_num <= 2; ++frame_num) {
    bool first_frame = frame_num == 1;
    ResetDamageForTest();
    {
      // Sub-surface with backdrop-filter.
      CompositorRenderPassList pass_list;
      auto bd_pass = CompositorRenderPass::Create();
      cc::FilterOperations backdrop_filters;
      backdrop_filters.Append(cc::FilterOperation::CreateBlurFilter(5.0));
      bd_pass->SetAll(
          render_pass_id_generator.GenerateNextId(), sub_surface_rect,
          no_damage, gfx::Transform(), cc::FilterOperations(), backdrop_filters,
          gfx::RRectF(gfx::RectF(sub_surface_rect), 0), SubtreeCaptureId(),
          sub_surface_rect.size(), ViewTransitionElementResourceId(), false,
          false, false, false, false);
      pass_list.push_back(std::move(bd_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support1->SubmitCompositorFrame(sub_local_surface_id1,
                                          std::move(frame));
    }

    {
      // Sub-surface with damage.
      CompositorRenderPassList pass_list;
      auto other_pass = CompositorRenderPass::Create();
      other_pass->output_rect = gfx::Rect(display_size);
      other_pass->damage_rect = damage_rect;
      other_pass->id = render_pass_id_generator.GenerateNextId();
      pass_list.push_back(std::move(other_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support2->SubmitCompositorFrame(sub_local_surface_id2,
                                          std::move(frame));
    }

    {
      CompositorRenderPassList pass_list;
      auto pass = CompositorRenderPass::Create();
      pass->output_rect = gfx::Rect(display_size);
      pass->damage_rect = damage_rect;
      pass->id = render_pass_id_generator.GenerateNextId();

      // Embed sub surface 1, with backdrop filter.
      auto* shared_quad_state1 = pass->CreateAndAppendSharedQuadState();
      shared_quad_state1->SetAll(
          gfx::Transform(), /*quad_layer_rect=*/sub_surface_rect,
          /*visible_quad_layer_rect=*/sub_surface_rect,
          /*mask_filter_info=*/gfx::MaskFilterInfo(),
          /*clip=*/std::nullopt, /*contents_opaque=*/true,
          /*opacity=*/1.0f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
          /*layer_id=*/0u, /*fast_rounded_corner=*/false);
      auto* quad1 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad1->SetNew(shared_quad_state1, /*rect=*/sub_surface_rect,
                    /*visible_rect=*/sub_surface_rect,
                    SurfaceRange(std::nullopt, sub_surface_id1),
                    SkColors::kBlack,
                    /*stretch_content_to_fill_bounds=*/false);
      quad1->allow_merge = false;

      // Embed sub surface 2, with damage.
      auto* shared_quad_state2 = pass->CreateAndAppendSharedQuadState();
      gfx::Rect rect1(display_size);
      shared_quad_state2->SetAll(
          gfx::Transform(), /*quad_layer_rect=*/rect1,
          /*visible_quad_layer_rect=*/rect1,
          /*mask_filter_info=*/gfx::MaskFilterInfo(),
          /*clip_rect=*/std::nullopt,
          /*are_contents_opaque=*/true, /*opacity=*/1.0f, SkBlendMode::kSrcOver,
          /*sorting_context=*/0,
          /*layer_id=*/0u, /*fast_rounded_corner=*/false);
      auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad2->SetNew(shared_quad_state2, /*rect=*/rect1,
                    /*visible_rect=*/rect1,
                    SurfaceRange(std::nullopt, sub_surface_id2),
                    SkColors::kBlack,
                    /*stretch_content_to_fill_bounds=*/false);
      quad2->allow_merge = false;

      pass_list.push_back(std::move(pass));
      SubmitCompositorFrame(&pass_list, local_surface_id);

      scheduler_->reset_swapped_for_test();
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
      EXPECT_TRUE(scheduler_->swapped());
      EXPECT_EQ(frame_num, output_surface_->num_sent_frames());
      EXPECT_EQ(display_size, software_output_device_->viewport_pixel_size());
      // The damage rect produced by surface_aggregator only includes the
      // damaged surface rect, and is not expanded for the backdrop-filter
      // surface.
      auto expected_damage =
          first_frame ? gfx::Rect(display_size) : gfx::Rect(20, 20, 40, 40);
      EXPECT_EQ(expected_damage, software_output_device_->damage_rect());
      // The scissor rect is expanded by direct_renderer to include the
      // overlapping pixel-moving backdrop filter surface.
      auto expected_scissor_rect =
          first_frame ? gfx::Rect(display_size) : gfx::Rect(5, 5, 55, 55);
      EXPECT_EQ(
          expected_scissor_rect,
          display_->renderer_for_testing()->GetLastRootScissorRectForTesting());
    }
  }
}

// Regression test for https://crbug.com/727162: Submitting a CompositorFrame to
// a surface should only cause damage on the Display the surface belongs to.
// There should not be a side-effect on other Displays.
TEST_F(DisplayTest, CompositorFrameDamagesCorrectDisplay) {
  RendererSettings settings;
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id(id_allocator_.GetCurrentLocalSurfaceId());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Set up second frame sink + display.
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, true /* is_root */);
  auto begin_frame_source2 = std::make_unique<StubBeginFrameSource>();
  auto scheduler_for_display2 = std::make_unique<TestDisplayScheduler>(
      begin_frame_source2.get(), task_runner_.get());
  TestDisplayScheduler* scheduler2 = scheduler_for_display2.get();
  StubDisplayClient client2;  // Must outlive `display2`.
  auto display2 = CreateDisplay(
      settings, kAnotherFrameSinkId, std::move(scheduler_for_display2),
      std::make_unique<FakeSoftwareOutputSurface>(
          std::make_unique<TestSoftwareOutputDevice>()));
  manager_.RegisterBeginFrameSource(begin_frame_source2.get(),
                                    kAnotherFrameSinkId);
  display2->Initialize(&client2, manager_.surface_manager());
  display2->SetLocalSurfaceId(local_surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));
  display2->Resize(gfx::Size(100, 100));

  ResetDamageForTest();
  scheduler2->ResetDamageForTest();
  EXPECT_FALSE(scheduler_->damaged());
  EXPECT_FALSE(scheduler2->damaged());

  // Submit a frame for display_ with full damage.
  CompositorRenderPassList pass_list;
  auto pass = CompositorRenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = CompositorRenderPassId{1};
  pass_list.push_back(std::move(pass));

  SubmitCompositorFrame(&pass_list, local_surface_id);

  // Should have damaged only display_ but not display2.
  EXPECT_TRUE(scheduler_->damaged());
  EXPECT_FALSE(scheduler2->damaged());
  manager_.UnregisterBeginFrameSource(begin_frame_source2.get());
}

// Supports testing features::OnBeginFrameAcks, which changes the expectations
// of what IPCs are sent to the CompositorFrameSinkClient. When enabled
// OnBeginFrame also handles ReturnResources as well as
// DidReceiveCompositorFrameAck.
class OnBeginFrameAcksDisplayTest : public DisplayTest,
                                    public testing::WithParamInterface<bool> {
 public:
  OnBeginFrameAcksDisplayTest();
  ~OnBeginFrameAcksDisplayTest() override = default;

  bool BeginFrameAcksEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

OnBeginFrameAcksDisplayTest::OnBeginFrameAcksDisplayTest() {
  if (BeginFrameAcksEnabled()) {
    scoped_feature_list_.InitAndEnableFeature(features::kOnBeginFrameAcks);
  } else {
    scoped_feature_list_.InitAndDisableFeature(features::kOnBeginFrameAcks);
  }
}

TEST_P(OnBeginFrameAcksDisplayTest, CompositorFrameWithPresentationToken) {
  RendererSettings settings;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceId());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id(6,
                                            base::UnguessableToken::Create());
  const SurfaceId sub_surface_id(kAnotherFrameSinkId, sub_local_surface_id);

  MockCompositorFrameSinkClient sub_client;

  auto sub_support = std::make_unique<CompositorFrameSinkSupport>(
      &sub_client, &manager_, kAnotherFrameSinkId, false /* is_root */);
  if (BeginFrameAcksEnabled()) {
    sub_support->SetWantsBeginFrameAcks();
  }

  const gfx::Size display_size(100, 100);
  display_->Resize(display_size);
  const gfx::Size sub_surface_size(32, 32);

  uint32_t frame_token_1 = kInvalidFrameToken;
  uint32_t frame_token_2 = kInvalidFrameToken;
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(sub_surface_size), gfx::Rect())
            .SetBeginFrameSourceId(kBeginFrameSourceId)
            .Build();
    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    frame_token_1 = frame.metadata.frame_token;
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));
  }

  {
    // Submit a frame for display_ with full damage.
    CompositorRenderPassList pass_list;
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(display_size);
    pass->damage_rect = gfx::Rect(display_size);
    pass->id = CompositorRenderPassId{1};

    auto* shared_quad_state1 = pass->CreateAndAppendSharedQuadState();
    gfx::Rect rect1(display_size);
    shared_quad_state1->SetAll(gfx::Transform(), /*quad_layer_rect=*/rect1,
                               /*visible_quad_layer_rect=*/rect1,
                               /*mask_filter_info=*/gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, /*contents_opaque=*/false,
                               /*opacity_f=*/0.5f, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad1 = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad1->SetNew(shared_quad_state1, rect1 /* rect */,
                  rect1 /* visible_rect */, SkColors::kBlack,
                  false /* force_anti_aliasing_off */);

    auto* shared_quad_state2 = pass->CreateAndAppendSharedQuadState();
    gfx::Rect rect2(gfx::Point(20, 20), sub_surface_size);
    shared_quad_state2->SetAll(gfx::Transform(), /*quad_layer_rect=*/rect2,
                               /*visible_quad_layer_rect=*/rect2,
                               /*mask_filter_info=*/gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, /*contents_opaque=*/false,
                               /*opacity_f=*/1.f, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad2->SetNew(shared_quad_state2, rect2 /* rect */,
                  rect2 /* visible_rect */,
                  SurfaceRange(std::nullopt, sub_surface_id), SkColors::kBlack,
                  false /* stretch_content_to_fill_bounds */);

    pass_list.push_back(std::move(pass));
    SubmitCompositorFrame(&pass_list, local_surface_id);
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    RunUntilIdle();
  }

  {
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(gfx::Rect(sub_surface_size),
                                               gfx::Rect(sub_surface_size))
                                .SetBeginFrameSourceId(kBeginFrameSourceId)
                                .Build();
    frame_token_2 = frame.metadata.frame_token;

    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));

    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    RunUntilIdle();

    // Both frames with frame-tokens 1 and 2 requested presentation-feedback.
    ASSERT_EQ(2u, sub_support->timing_details().size());
    EXPECT_EQ(sub_support->timing_details().count(frame_token_1), 1u);
    EXPECT_EQ(sub_support->timing_details().count(frame_token_2), 1u);
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(sub_surface_size), gfx::Rect())
            .SetBeginFrameSourceId(kBeginFrameSourceId)
            .Build();

    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));

    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         OnBeginFrameAcksDisplayTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "BeginFrameAcks"
                                             : "CompositoFrameAcks";
                         });

TEST_F(DisplayTest, BeginFrameThrottling) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  support_->SetNeedsBeginFrame(true);

  // Helper fn to submit a CF.
  auto submit_frame = [this]() {
    CompositorRenderPassList pass_list;
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
  };

  // Submit kUndrawnFrameLimit+1 frames. BeginFrames should be throttled only
  // after the last frame.
  base::TimeTicks frame_time;
  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit + 1;
       ++i) {
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    UpdateBeginFrameTime(support_.get(), frame_time);
    submit_frame();
    // Until we reach throttling we should return true.
    if (i < CompositorFrameSinkSupport::kUndrawnFrameLimit) {
      EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    } else {
      EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
    }
    // Clear the presentation feedbacks.
    UpdateBeginFrameTime(support_.get(), frame_time);
  }
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // Drawing should unthrottle begin-frames.
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  frame_time = base::TimeTicks::Now();
  EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // Verify that throttling starts again after kUndrawnFrameLimit+1 frames.
  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit + 1;
       ++i) {
    // This clears the presentation feedbacks.
    UpdateBeginFrameTime(support_.get(), frame_time);
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    UpdateBeginFrameTime(support_.get(), frame_time);
    submit_frame();
    // Until we reach throttling we should return true.
    if (i < CompositorFrameSinkSupport::kUndrawnFrameLimit) {
      EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    } else {
      EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
    }
    // Clear the presentation feedbacks.
    UpdateBeginFrameTime(support_.get(), frame_time);
  }
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // Instead of doing a draw, forward time by ~1 seconds. That should unthrottle
  // the begin-frame.
  frame_time += base::Seconds(1.1);
  EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
}

TEST_F(DisplayTest, BeginFrameThrottlingMultipleSurfaces) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  support_->SetNeedsBeginFrame(true);

  // Helper fn to submit a CF.
  auto submit_frame = [this]() {
    CompositorRenderPassList pass_list;
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = CompositorRenderPassId{1u};
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
  };

  // Submit kUndrawnFrameLimit frames. BeginFrames should be throttled only
  // after the last frame.
  base::TimeTicks frame_time;
  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit + 1;
       ++i) {
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    UpdateBeginFrameTime(support_.get(), frame_time);
    submit_frame();
    // Generate a new LocalSurfaceId for the next submission.
    id_allocator_.GenerateId();
  }
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // This only draws the first surface, so we should only be able to send one
  // more BeginFrame.
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  frame_time = base::TimeTicks::Now();
  EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // After this frame submission, we are throttled again.
  submit_frame();
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);

  // Now the last surface is drawn. This should unblock us to submit
  // kUndrawnFrameLimit+1 frames again.
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
  id_allocator_.GenerateId();
  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit + 1;
       ++i) {
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
    UpdateBeginFrameTime(support_.get(), frame_time);
    submit_frame();
    // Generate a new LocalSurfaceId for the next submission.
    id_allocator_.GenerateId();
  }
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(support_.get(), frame_time));
  UpdateBeginFrameTime(support_.get(), frame_time);
}

TEST_F(DisplayTest, DontThrottleWhenParentBlocked) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  support_->SetNeedsBeginFrame(true);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id(6,
                                            base::UnguessableToken::Create());
  const LocalSurfaceId sub_local_surface_id2(7,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id2(kAnotherFrameSinkId, sub_local_surface_id2);

  MockCompositorFrameSinkClient sub_client;

  auto sub_support = std::make_unique<CompositorFrameSinkSupport>(
      &sub_client, &manager_, kAnotherFrameSinkId, false /* is_root */);
  sub_support->SetNeedsBeginFrame(true);

  // Submit kUndrawnFrameLimit+1 frames. BeginFrames should be throttled only
  // after the last frame.
  base::TimeTicks frame_time;
  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit + 1;
       ++i) {
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
    UpdateBeginFrameTime(sub_support.get(), frame_time);
    sub_support->SubmitCompositorFrame(sub_local_surface_id,
                                       MakeDefaultCompositorFrame());
    // Until we reach throttling we should return true.
    if (i < CompositorFrameSinkSupport::kUndrawnFrameLimit) {
      EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
    } else {
      EXPECT_FALSE(ShouldSendBeginFrame(sub_support.get(), frame_time));
    }
    // Clear the presentation feedbacks.
    UpdateBeginFrameTime(sub_support.get(), frame_time);
  }
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(sub_support.get(), frame_time));
  UpdateBeginFrameTime(sub_support.get(), frame_time);

  // Make the display block on |sub_local_surface_id2|.
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddDefaultRenderPass()
          .SetActivationDependencies({sub_surface_id2})
          .SetDeadline(FrameDeadline(base::TimeTicks::Now(),
                                     std::numeric_limits<uint32_t>::max(),
                                     base::Seconds(1), false))
          .Build();
  support_->SubmitCompositorFrame(id_allocator_.GetCurrentLocalSurfaceId(),
                                  std::move(frame));

  for (uint32_t i = 0; i < CompositorFrameSinkSupport::kUndrawnFrameLimit * 3;
       ++i) {
    frame_time = base::TimeTicks::Now();
    EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
    UpdateBeginFrameTime(sub_support.get(), frame_time);
    sub_support->SubmitCompositorFrame(sub_local_surface_id,
                                       MakeDefaultCompositorFrame());
    // Immediately after submitting frame, because there is presentation
    // feedback queued up, ShouldSendBeginFrame should always return true.
    EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
    // Clear the presentation feedbacks.
    UpdateBeginFrameTime(sub_support.get(), frame_time);
  }

  // Now submit to |sub_local_surface_id2|. This should unblock the parent and
  // throttling will resume.
  frame_time = base::TimeTicks::Now();
  EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
  UpdateBeginFrameTime(sub_support.get(), frame_time);
  sub_support->SubmitCompositorFrame(sub_local_surface_id2,
                                     MakeDefaultCompositorFrame());
  frame_time = base::TimeTicks::Now();
  EXPECT_FALSE(ShouldSendBeginFrame(sub_support.get(), frame_time));
  UpdateBeginFrameTime(sub_support.get(), frame_time);
}

TEST_F(DisplayTest, DisplayTransformHint) {
  SetUpSoftwareDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id(id_allocator_.GetCurrentLocalSurfaceId());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  constexpr gfx::Size kSize = gfx::Size(100, 80);
  constexpr gfx::Size kTransposedSize =
      gfx::Size(kSize.height(), kSize.width());

  display_->Resize(kSize);

  const struct {
    bool support_display_transform;
    gfx::OverlayTransform display_transform_hint;
    gfx::Size expected_size;
  } kTestCases[] = {
      // Output size is always the display size when output surface does not
      // support display transform hint.
      {false, gfx::OVERLAY_TRANSFORM_NONE, kSize},
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90, kSize},
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180, kSize},
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270, kSize},

      // Output size is transposed on 90/270 degree rotation when output surface
      // supports display transform hint.
      {true, gfx::OVERLAY_TRANSFORM_NONE, kSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90, kTransposedSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180, kSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270, kTransposedSize},
  };

  size_t expected_frame_sent = 0u;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "support_display_transform="
                 << test.support_display_transform
                 << ", display_transform_hint=" << test.display_transform_hint);

    output_surface_->set_support_display_transform_hint(
        test.support_display_transform);

    constexpr gfx::Rect kOutputRect(gfx::Point(0, 0), kSize);
    constexpr gfx::Rect kDamageRect(10, 10, 1, 1);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .Build();
    frame.metadata.display_transform_hint = test.display_transform_hint;
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    EXPECT_EQ(++expected_frame_sent, output_surface_->num_sent_frames());
    EXPECT_EQ(test.expected_size,
              software_output_device_->viewport_pixel_size());
  }
}

TEST_F(DisplayTest, DisplaySizeMismatch) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.auto_resize_output_surface = false;
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  id_allocator_.GenerateId();
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  display_->Resize(gfx::Size(100, 100));

  // Pass has copy output request but wrong size so it should be drawn, but not
  // swapped.
  {
    auto pass = CompositorRenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    base::RunLoop copy_run_loop;
    bool copy_called = false;
    pass->copy_requests.push_back(std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA,
        CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(&CopyCallback, &copy_called,
                       copy_run_loop.QuitClosure())));
    pass->id = CompositorRenderPassId{1u};

    CompositorRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, id_allocator_.GetCurrentLocalSurfaceId());
    EXPECT_TRUE(scheduler_->damaged());

    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});

    copy_run_loop.Run();

    // Expect no swap happen
    EXPECT_EQ(0u, output_surface_->num_sent_frames());

    // Expect draw and copy output request happen
    EXPECT_TRUE(copy_called);

    // Expect there is no pending
    EXPECT_EQ(pending_presentation_group_timings_size(), 0u);
  }
}

class UseMapRectDisplayTest : public DisplayTest,
                              public testing::WithParamInterface<bool> {
 public:
  UseMapRectDisplayTest();
  ~UseMapRectDisplayTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

UseMapRectDisplayTest::UseMapRectDisplayTest() {
  if (GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kUseMapRectForPixelMovement);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        features::kUseMapRectForPixelMovement);
  }
}

TEST_P(UseMapRectDisplayTest, PixelMovingForegroundFilterTest) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceId());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  TestSurfaceIdAllocator sub_surface_id1(kAnotherFrameSinkId);
  auto sub_support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, /*is_root=*/false);

  // Create frame sink for another sub surface.
  TestSurfaceIdAllocator sub_surface_id2(kAnotherFrameSinkId2);
  auto sub_support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId2, /*is_root=*/false);

  // Main surface M, damage D, sub-surface B with foreground filter.
  //   +-----------+
  //   | +----+   M|
  //   | |B +-|-+  |
  //   | +--|-+ |  |
  //   |    |  D|  |
  //   |    +---+  |
  //   +-----------+
  const gfx::Size display_size(100, 100);
  const gfx::Rect damage_rect(20, 20, 40, 40);
  display_->Resize(display_size);
  const gfx::Rect sub_surface_rect(5, 5, 25, 25);
  const gfx::Rect no_damage;

  CompositorRenderPassId::Generator render_pass_id_generator;
  for (size_t frame_num = 1; frame_num <= 2; ++frame_num) {
    bool first_frame = frame_num == 1;
    ResetDamageForTest();
    {
      // Sub-surface with pixel-moving foreground filter - drop shadow filter
      CompositorRenderPassList pass_list;
      auto bd_pass = CompositorRenderPass::Create();
      cc::FilterOperations foreground_filters;
      foreground_filters.Append(cc::FilterOperation::CreateDropShadowFilter(
          gfx::Point(5, 10), 2.f, SkColors::kTransparent));
      bd_pass->SetAll(
          render_pass_id_generator.GenerateNextId(), sub_surface_rect,
          no_damage, gfx::Transform(), foreground_filters,
          cc::FilterOperations(), gfx::RRectF(gfx::RectF(sub_surface_rect), 0),
          SubtreeCaptureId(), sub_surface_rect.size(),
          ViewTransitionElementResourceId(), false, false, false, false, false);
      pass_list.push_back(std::move(bd_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support1->SubmitCompositorFrame(sub_surface_id1.local_surface_id(),
                                          std::move(frame));
    }

    {
      // Sub-surface with damage.
      CompositorRenderPassList pass_list;
      auto other_pass = CompositorRenderPass::Create();
      other_pass->output_rect = gfx::Rect(display_size);
      other_pass->damage_rect = damage_rect;
      other_pass->id = render_pass_id_generator.GenerateNextId();
      pass_list.push_back(std::move(other_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support2->SubmitCompositorFrame(sub_surface_id2.local_surface_id(),
                                          std::move(frame));
    }

    {
      auto frame = CompositorFrameBuilder()
                       .AddRenderPass(
                           RenderPassBuilder(display_size)
                               .AddSurfaceQuad(
                                   sub_surface_rect,
                                   SurfaceRange(std::nullopt, sub_surface_id1),
                                   {.allow_merge = false})
                               .AddSurfaceQuad(
                                   gfx::Rect(display_size),
                                   SurfaceRange(std::nullopt, sub_surface_id2),
                                   {.allow_merge = false})
                               .SetDamageRect(damage_rect))
                       .Build();
      support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

      scheduler_->reset_swapped_for_test();
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
      EXPECT_TRUE(scheduler_->swapped());
      EXPECT_EQ(frame_num, output_surface_->num_sent_frames());
      EXPECT_EQ(display_size, software_output_device_->viewport_pixel_size());

      auto expected_damage =
          first_frame ? gfx::Rect(display_size) : damage_rect;
      EXPECT_EQ(expected_damage, software_output_device_->damage_rect());
      // The scissor rect is expanded by direct_renderer to include the
      // overlapping pixel-moving foreground filter surface.
      auto expected_scissor_rect = first_frame  ? gfx::Rect(display_size)
                                   : GetParam() ? gfx::Rect(4, 5, 56, 55)
                                                : gfx::Rect(0, 0, 60, 60);
      EXPECT_EQ(
          expected_scissor_rect,
          display_->renderer_for_testing()->GetLastRootScissorRectForTesting());
    }
  }
}

TEST_F(DisplayTest, CanSkipRenderPass) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAllowUndamagedNonrootRenderPassToSkip);

  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceId());

  // Set up first display.
  SetUpSoftwareDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  TestSurfaceIdAllocator sub_surface_id1(kAnotherFrameSinkId);
  auto sub_support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, /*is_root=*/false);

  // generate render pass id for the nonroot render pass.
  CompositorRenderPassId::Generator render_pass_id_generator;
  auto id_1 = render_pass_id_generator.GenerateNextId();

  const gfx::Size display_size(100, 100);
  const gfx::Rect root_damage_rect(20, 20, 40, 40);
  display_->Resize(display_size);
  const gfx::Rect sub_surface_rect(5, 5, 60, 60);
  const gfx::Rect sub_surface_damage_rect(10, 10, 30, 30);

  for (size_t frame_num = 1; frame_num <= 3; ++frame_num) {
    ResetDamageForTest();

    // Nonroot render pass with id_1. No update for frame #3.
    if (frame_num != 3) {
      CompositorRenderPassList pass_list;
      auto bd_pass = CompositorRenderPass::Create();
      bd_pass->output_rect = sub_surface_rect;
      bd_pass->damage_rect = sub_surface_damage_rect;
      bd_pass->has_damage_from_contributing_content = true;
      bd_pass->id = id_1;
      pass_list.push_back(std::move(bd_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();

      sub_support1->SubmitCompositorFrame(sub_surface_id1.local_surface_id(),
                                          std::move(frame));
    }

    // Root render pass
    {
      auto frame =
          CompositorFrameBuilder()
              .AddRenderPass(RenderPassBuilder(display_size)
                                 .AddSurfaceQuad(sub_surface_rect,
                                                 SurfaceRange(std::nullopt,
                                                              sub_surface_id1),
                                                 {.allow_merge = false})
                                 .SetDamageRect(root_damage_rect))
              .Build();
      support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

      scheduler_->reset_swapped_for_test();
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
      EXPECT_TRUE(scheduler_->swapped());

      // Number of skipped non-root render passes.
      auto* skipped = display_->renderer_for_testing()
                          ->GetLastSkippedRenderPassIdsForTesting();

      if (frame_num != 3) {
        // Whether the render pass can be skpped or not depends on the flag
        // pass->has_damage_from_contributing_content and the render pass
        // damage rect.
        EXPECT_EQ(0u, skipped->size());
      } else {
        // No frame update for the sub surface. The nonroot render pass damage
        // rect will be zero. pass->has_damage_from_contributing_content becomes
        // false when there is no frame update. The associated non-render pass
        // can be skipped.
        EXPECT_EQ(1u, skipped->size());
      }
    }
  }
}

class SkiaDelegatedInkRendererTest : public DisplayTest {
 public:
  ~SkiaDelegatedInkRendererTest() override {
    // Reset `client_` in `display_` to avoid accessing DisplayClient after
    // `client_` is destructed. Without this, `display_` which is declared in
    // DisplayTest class is destructed after `client_` which is declared in this
    // class.
    display_->ResetDisplayClientForTesting(&client_);
  }

  void SetUpRenderers() {
    SetUpGpuDisplay(RendererSettings());

    // Initialize the renderer and create an ink renderer.
    display_->Initialize(&client_, manager_.surface_manager());

    auto renderer = std::make_unique<DelegatedInkPointRendererSkiaForTest>();
    ink_renderer_ = renderer.get();
    display_->renderer_for_testing()->SetDelegatedInkPointRendererSkiaForTest(
        std::move(renderer));
  }

  DelegatedInkPointRendererBase* ink_renderer() {
    return display_->renderer_for_testing()
        ->GetDelegatedInkPointRenderer(/*create_if_necessary=*/
                                       false);
  }

  int UniqueStoredPointerIds() {
    return ink_renderer()->GetPointsMapForTest().size();
  }

  int StoredPointsForPointerId(int32_t pointer_id) {
    return GetPointsForPointerId(pointer_id).size();
  }

  const std::map<base::TimeTicks, gfx::DelegatedInkPoint>&
  GetPointsForPointerId(int32_t pointer_id) {
    DCHECK(ink_renderer()->GetPointsMapForTest().find(pointer_id) !=
           ink_renderer()->GetPointsMapForTest().end());
    return ink_renderer()
        ->GetPointsMapForTest()
        .find(pointer_id)
        ->second.GetPoints();
  }

  void CreateAndStoreDelegatedInkPoint(const gfx::PointF& point,
                                       base::TimeTicks timestamp,
                                       int32_t pointer_id) {
    ink_points_[pointer_id].emplace_back(point, timestamp, pointer_id);
    ink_renderer()->StoreDelegatedInkPoint(ink_points_[pointer_id].back());
  }

  void CreateAndStoreDelegatedInkPointFromPreviousPoint(int32_t pointer_id) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());

    gfx::PointF point(ink_points_[pointer_id].back().point());
    point.Offset(10, 10);

    base::TimeTicks timestamp = ink_points_[pointer_id].back().timestamp();
    timestamp += base::Milliseconds(5);

    CreateAndStoreDelegatedInkPoint(point, timestamp, pointer_id);
  }

  void StoreAlreadyCreatedDelegatedInkPoints() {
    DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
    StoreAlreadyCreatedDelegatedInkPoints(ink_points_.begin()->first);
  }

  void StoreAlreadyCreatedDelegatedInkPoints(int32_t pointer_id) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
    for (gfx::DelegatedInkPoint ink_point : ink_points_[pointer_id])
      ink_renderer()->StoreDelegatedInkPoint(ink_point);
  }

  void SendMetadata(gfx::DelegatedInkMetadata metadata) {
    ink_renderer()->SetDelegatedInkMetadata(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }

  gfx::DelegatedInkMetadata MakeAndSendMetadataFromStoredInkPoint(
      int index,
      float diameter,
      SkColor4f color,
      const gfx::RectF& presentation_area) {
    DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
    return MakeAndSendMetadataFromStoredInkPoint(
        ink_points_.begin()->first, index, diameter, color, presentation_area);
  }

  gfx::DelegatedInkMetadata MakeAndSendMetadataFromStoredInkPoint(
      int32_t pointer_id,
      int index,
      float diameter,
      SkColor4f color,
      const gfx::RectF& presentation_area) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
    EXPECT_GE(index, 0);
    EXPECT_LT(index, ink_points_size(pointer_id));

    // TODO(crbug.com/40219248): gfx::DelegatedInkMetadata to SkColor4f
    gfx::DelegatedInkMetadata metadata(
        ink_points_[pointer_id][index].point(), diameter, color.toSkColor(),
        ink_points_[pointer_id][index].timestamp(), presentation_area,
        base::TimeTicks::Now(),
        /*hovering*/ false, /*render_pass_id=*/0);
    SendMetadata(metadata);
    return metadata;
  }

  void HistogramCheck(const base::HistogramTester& histograms,
                      base::TimeDelta expected_bucket,
                      const char* histogram_name) {
    if (expected_bucket == base::TimeDelta::Min()) {
      histograms.ExpectTotalCount(histogram_name, 0);
    } else {
      histograms.ExpectTotalCount(histogram_name, 1);
      histograms.ExpectTimeBucketCount(histogram_name, expected_bucket, 1);
    }
  }

  // Either bucket containing base::TimeDelta::Min() is interpreted to mean that
  // expected total count of the histogram should be 0.
  void FinalizePathAndCheckHistograms(
      base::TimeDelta expected_bucket_without_prediction,
      base::TimeDelta expected_bucket_with_prediction) {
    base::HistogramTester histograms;
    ink_renderer()->FinalizePathForDraw();
    HistogramCheck(
        histograms, expected_bucket_without_prediction,
        "Renderer.DelegatedInkTrail.LatencyImprovement.Skia.WithoutPrediction");
    HistogramCheck(
        histograms, expected_bucket_with_prediction,
        "Renderer.DelegatedInkTrail.LatencyImprovement.Skia.WithPrediction");
  }

  void DrawDelegatedInkTrail() {
    SkCanvas canvas;
    static_cast<DelegatedInkPointRendererSkia*>(ink_renderer())
        ->DrawDelegatedInkTrail(&canvas, gfx::Transform());
  }

  int GetPathPointCount() { return ink_renderer()->GetPathPointCountForTest(); }

  // Explicitly get the metadata that is stored on the renderer.
  const gfx::DelegatedInkMetadata* GetMetadataFromRenderer() {
    return ink_renderer()->GetMetadataForTest();
  }

  const gfx::DelegatedInkPoint& ink_point(int index) {
    DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
    return ink_point(ink_points_.begin()->first, index);
  }

  const gfx::DelegatedInkPoint& ink_point(int32_t pointer_id, int index) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
    EXPECT_GE(index, 0);
    EXPECT_LT(index, ink_points_size(pointer_id));
    return ink_points_[pointer_id][index];
  }

  const gfx::DelegatedInkPoint& last_ink_point(int32_t pointer_id) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
    return ink_points_[pointer_id].back();
  }

  int ink_points_size() {
    DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
    return ink_points_.begin()->second.size();
  }

  int ink_points_size(int32_t pointer_id) {
    DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
    return ink_points_[pointer_id].size();
  }

  int points_to_predict() const { return kPointsToPredict; }

  const base::TimeDelta time_into_the_future() const {
    return base::Milliseconds(
        (kMillisecondsIntoFuturePerPoint - kResampleLatency) *
        kPointsToPredict);
  }

 protected:
  raw_ptr<DelegatedInkPointRendererSkiaForTest> ink_renderer_ = nullptr;

  // Stub client kept in scope to prevent access violations during DrawAndSwap.
  StubDisplayClient client_;

  base::test::ScopedFeatureList feature_list_;

 private:
  std::unordered_map<int32_t, std::vector<gfx::DelegatedInkPoint>> ink_points_;

  // Values used to configure the points predictor. Needs to match the values
  // in `DelegatedInkTrailData`;
  static const int kPointsToPredict = 2;
  static const int kMillisecondsIntoFuturePerPoint = 6;
  static const int kResampleLatency = 5;
};

// Testing filtering points in the the delegated ink renderer when the skia
// renderer is in use.
TEST_F(SkiaDelegatedInkRendererTest, SkiaDelegatedInkRendererFilteringPoints) {
  SetUpRenderers();

  // First, a sanity check.
  EXPECT_EQ(0, UniqueStoredPointerIds());

  // Insert 3 arbitrary points into the ink renderer to confirm that they go
  // where we expect and are all stored correctly.
  const int kInitialDelegatedPoints = 3;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::PointF point(10, 10);
  const int32_t kPointerId = std::numeric_limits<int32_t>::max();
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  for (int i = 1; i < kInitialDelegatedPoints; ++i)
    CreateAndStoreDelegatedInkPointFromPreviousPoint(kPointerId);

  // They all have the same pointer ID, so there should be exactly one unique
  // element in the map, and that element should itself have all three points.
  EXPECT_EQ(1, UniqueStoredPointerIds());
  EXPECT_EQ(kInitialDelegatedPoints, StoredPointsForPointerId(kPointerId));

  // Now provide metadata with a timestamp matching one of the points to
  // confirm that earlier points are removed and later points remain.
  const int kInkPointForMetadata = 1;
  const float kDiameter = 1.f;
  gfx::DelegatedInkMetadata metadata = MakeAndSendMetadataFromStoredInkPoint(
      kInkPointForMetadata, kDiameter, SkColors::kBlack, gfx::RectF());

  // The histogram should count one in the bucket that is the difference between
  // the latest point stored and the metadata. The *WithoutPrediction histogram
  // should count the difference between the last point and the metadata, while
  // the *WithPrediction* histogram should count 1 in the 7ms bucket because
  // prediction can occer with linear resampling and 2 input points.
  base::TimeDelta bucket_without_prediction =
      last_ink_point(kPointerId).timestamp() - metadata.timestamp();
  FinalizePathAndCheckHistograms(bucket_without_prediction,
                                 base::Milliseconds(7));

  EXPECT_EQ(kInitialDelegatedPoints - kInkPointForMetadata,
            StoredPointsForPointerId(kPointerId));
  EXPECT_EQ(metadata.point(),
            GetPointsForPointerId(kPointerId).begin()->second.point());
  EXPECT_EQ(last_ink_point(kPointerId).point(),
            GetPointsForPointerId(kPointerId).rbegin()->second.point());
  EXPECT_EQ(ink_point(0).pointer_id(), kPointerId);

  // Confirm that the metadata is cleared when DrawDelegatedInkTrail() is
  // called.
  DrawDelegatedInkTrail();
  EXPECT_FALSE(GetMetadataFromRenderer());

  // Add more points than the maximum that will be stored to confirm only the
  // max is stored and the correct ones are removed first.
  const int kPointsBeyondMaxAllowed = 2;
  StoreAlreadyCreatedDelegatedInkPoints();
  while (ink_points_size() <
         gfx::kMaximumNumberOfDelegatedInkPoints + kPointsBeyondMaxAllowed)
    CreateAndStoreDelegatedInkPointFromPreviousPoint(kPointerId);

  EXPECT_EQ(gfx::kMaximumNumberOfDelegatedInkPoints,
            StoredPointsForPointerId(kPointerId));
  EXPECT_EQ(ink_point(kPointsBeyondMaxAllowed).point(),
            GetPointsForPointerId(kPointerId).begin()->second.point());
  EXPECT_EQ(last_ink_point(kPointerId).point(),
            GetPointsForPointerId(kPointerId).rbegin()->second.point());
  EXPECT_EQ(last_ink_point(kPointerId).pointer_id(), kPointerId);

  // Now send metadata with a timestamp before all of the points that are
  // currently stored to confirm that no points are filtered out and the number
  // stored remains the same. The *WithoutPrediction histogram should record 0
  // improvement, but the *WithPrediction* one should not record anything at all
  // due to not finding a matching pointer ID to predict with.
  const int kExpectedPoints = StoredPointsForPointerId(kPointerId);
  SendMetadata(metadata);
  FinalizePathAndCheckHistograms(base::Milliseconds(0), base::TimeDelta::Min());
  EXPECT_EQ(kExpectedPoints, StoredPointsForPointerId(kPointerId));
}

// Test filtering when points arrive with several different pointer IDs.
TEST_F(SkiaDelegatedInkRendererTest,
       SkiaDelegatedInkRendererFilteringPointsWithMultiplePointerIds) {
  SetUpRenderers();

  // Unique pointer IDs used - numbers arbitrary.
  const std::vector<int32_t> kPointerIds = {1, 20, 300};

  // First add just one DelegatedInkPoint for each pointer id to confirm that
  // they all get stored separately.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (uint64_t i = 0; i < kPointerIds.size(); ++i) {
    // Make sure that each pointer id has slightly different points so that when
    // new points are added later that are based on previous points, it doesn't
    // result in multiple pointer ids having identical DelegatedInkPoints
    CreateAndStoreDelegatedInkPoint(gfx::PointF(i * 5, i * 10), timestamp,
                                    kPointerIds[i]);
    timestamp += base::Milliseconds(5);
  }

  EXPECT_EQ(static_cast<int>(kPointerIds.size()), UniqueStoredPointerIds());
  for (int32_t pointer_id : kPointerIds)
    EXPECT_EQ(1, StoredPointsForPointerId(pointer_id));

  // Add more points so that the first pointer ID contains 4 DelegatedInkPoints,
  // and the third pointer id contains 2 DelegatedInkPoints
  const int kNumPointsForPointerId0 = 4;
  while (ink_points_size(kPointerIds[0]) < kNumPointsForPointerId0)
    CreateAndStoreDelegatedInkPointFromPreviousPoint(kPointerIds[0]);
  CreateAndStoreDelegatedInkPointFromPreviousPoint(kPointerIds[2]);

  // Confirm all the points got stored where they should have been.
  for (int32_t pointer_id : kPointerIds) {
    EXPECT_EQ(ink_points_size(pointer_id),
              StoredPointsForPointerId(pointer_id));
  }

  // Now provide metadata with a timestamp matching one of the points in the
  // first pointer id bucket to confirm that earlier points are removed and
  // later points remain.
  const int kInkPointForMetadata = 1;
  const float kDiameter = 1.f;
  gfx::DelegatedInkMetadata metadata = MakeAndSendMetadataFromStoredInkPoint(
      kPointerIds[0], kInkPointForMetadata, kDiameter, SkColors::kBlack,
      gfx::RectF());

  // 3 points should be enough for prediction to work, so the histogram should
  // have one in the *WithoutPrediction bucket that matches the difference
  // between the metadata and the final point, and one in the *WithPrediction
  // bucket that matches the amount of prediction that is being done (plus the
  // difference between the final point and the metadata).
  base::TimeDelta bucket_without_prediction =
      last_ink_point(kPointerIds[0]).timestamp() - metadata.timestamp();
  FinalizePathAndCheckHistograms(
      bucket_without_prediction,
      bucket_without_prediction + time_into_the_future());

  // Confirm the size, first, and last points of the first pointer ID are what
  // we expect.
  EXPECT_EQ(kNumPointsForPointerId0 - kInkPointForMetadata,
            StoredPointsForPointerId(kPointerIds[0]));
  EXPECT_EQ(metadata.point(),
            GetPointsForPointerId(kPointerIds[0]).begin()->second.point());
  EXPECT_EQ(last_ink_point(kPointerIds[0]).point(),
            GetPointsForPointerId(kPointerIds[0]).rbegin()->second.point());

  // Confirm that neither of the other pointer ids were impacted.
  for (uint64_t i = 1; i < kPointerIds.size(); ++i) {
    EXPECT_EQ(ink_points_size(kPointerIds[i]),
              StoredPointsForPointerId(kPointerIds[i]));
  }

  // Send a metadata whose point and timestamp doesn't match any stored
  // DelegatedInkPoint and confirm that it doesn't cause any changes to the
  // stored values. *WithoutPrediction histogram should record 0 improvement,
  // *WithPrediction* shouldn't record anything due to no valid pointer id.
  SendMetadata(gfx::DelegatedInkMetadata(
      gfx::PointF(100, 100), 5.6f, SK_ColorBLACK, base::TimeTicks::Min(),
      gfx::RectF(), base::TimeTicks::Min(), /*hovering*/ false,
      /*render_pass_id=*/0));
  FinalizePathAndCheckHistograms(base::Milliseconds(0), base::TimeDelta::Min());
  EXPECT_EQ(kNumPointsForPointerId0 - kInkPointForMetadata,
            StoredPointsForPointerId(kPointerIds[0]));
  for (uint64_t i = 1; i < kPointerIds.size(); ++i) {
    EXPECT_EQ(ink_points_size(kPointerIds[i]),
              StoredPointsForPointerId(kPointerIds[i]));
  }

  // Finally, send a metadata with a timestamp beyond all of the stored points.
  // This should result in all of the points being erased, but the pointer ids
  // will still exist as they contains the predictors as well.
  SendMetadata(gfx::DelegatedInkMetadata(
      gfx::PointF(100, 100), 5.6f, SK_ColorBLACK,
      base::TimeTicks::Now() + base::Milliseconds(1000), gfx::RectF(),
      base::TimeTicks::Now(), /*hovering*/ false, /*render_pass_id=*/0));
  FinalizePathAndCheckHistograms(base::Milliseconds(0), base::TimeDelta::Min());
  for (int i : kPointerIds)
    EXPECT_EQ(0, StoredPointsForPointerId(i));
}

// Confirm that the delegated ink trail histograms record latency correctly.
TEST_F(SkiaDelegatedInkRendererTest, LatencyHistograms) {
  SetUpRenderers();

  // Confirm that nothing is counted in the histograms when there is no metadata
  // or points to draw.
  FinalizePathAndCheckHistograms(base::TimeDelta::Min(),
                                 base::TimeDelta::Min());

  // Insert 4 arbitrary points into the ink renderer to later draw.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  const int32_t kPointerId = 17;
  CreateAndStoreDelegatedInkPoint(gfx::PointF(20, 19), timestamp, kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(15, 19), timestamp + base::Milliseconds(8), kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(16, 28), timestamp + base::Milliseconds(16), kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(29, 35), timestamp + base::Milliseconds(24), kPointerId);

  // Provide a metadata so that points can be drawn, based on the first ink
  // point that was sent.
  const float kDiameter = 11.99f;
  MakeAndSendMetadataFromStoredInkPoint(/*index*/ 0, kDiameter,
                                        SkColors::kBlack, gfx::RectF());

  // *WithoutPrediction histogram should have one counted in the 24 ms bucket
  // because that's the difference between the latest point and the metadata.
  // *WithPrediction should be able to predict here, so it should contain 1 in
  // the bucket that is |kNumberOfMillisecondsIntoFutureToPredictPerPoint| *
  // |kNumberOfPointsToPredict| into the future from 24 ms bucket.
  base::TimeDelta bucket_without_prediction = base::Milliseconds(24);
  FinalizePathAndCheckHistograms(
      bucket_without_prediction,
      bucket_without_prediction + time_into_the_future());

  // Now provide metadata that matches the final ink point provided, so that
  // everything earlier is filtered out. Then the *WithoutPrediction histogram
  // will count 1 in the 0 ms bucket and the *WithPrediction histogram will
  // still be able to predict points, so it should have counted one.
  MakeAndSendMetadataFromStoredInkPoint(/*index*/ 3, kDiameter,
                                        SkColors::kBlack, gfx::RectF());
  bucket_without_prediction = base::Milliseconds(0);
  FinalizePathAndCheckHistograms(bucket_without_prediction,
                                 time_into_the_future());

  // DrawDelegatedInkTrail should clear the metadata, so finalizing the path
  // shouldn't record anything in the histograms.
  DrawDelegatedInkTrail();
  FinalizePathAndCheckHistograms(base::TimeDelta::Min(),
                                 base::TimeDelta::Min());

  // Send a few more points but no metadata to confirm that nothing is counted.
  timestamp = base::TimeTicks::Now();
  CreateAndStoreDelegatedInkPoint(gfx::PointF(85, 56), timestamp, kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(96, 70), timestamp + base::Milliseconds(2), kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(112, 94), timestamp + base::Milliseconds(10), kPointerId);
  FinalizePathAndCheckHistograms(base::TimeDelta::Min(),
                                 base::TimeDelta::Min());
}

// Confirm that a delegated ink trail will still be drawn if the point and
// metadata are close enough.
TEST_F(SkiaDelegatedInkRendererTest, DrawTrailWhenMetadataIsCloseEnough) {
  SetUpRenderers();

  // Insert 3 points, then create a metadata that is not exactly the same as
  // the first point, but within DelegatedInkPointRendererBase::kEpsilon of
  // the point so that a trail is drawn.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  base::TimeTicks timestamp2 = timestamp + base::Milliseconds(8);
  gfx::PointF point(45.f, 78.f);
  gfx::PointF point2(68.f, 89.f);
  const int32_t kPointerId = 17;
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  CreateAndStoreDelegatedInkPoint(point2, timestamp2, kPointerId);
  CreateAndStoreDelegatedInkPoint(
      gfx::PointF(80.f, 70.f), timestamp2 + base::Milliseconds(8), kPointerId);

  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(point.x() - 1.0f, point.y() - 1.0f), 45.f, SK_ColorBLACK,
      timestamp, gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering*/ false, /*render_pass_id=*/0);
  SendMetadata(metadata);

  // If the metadata was close enough, then a trail should be drawn with all
  // three points.
  ink_renderer()->FinalizePathForDraw();
  EXPECT_EQ(GetPathPointCount(), 3 + points_to_predict());

  // Now send a metadata with a point that is slightly further away from the
  // second point, such that the distance between them is greater than the
  // kEpsilon value to confirm that if it gets too far away we won't use it for
  // drawing.
  metadata = gfx::DelegatedInkMetadata(
      gfx::PointF(point2.x() - 1.01f, point2.y() - 1.0f), 45.f, SK_ColorBLACK,
      timestamp2, gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering*/ false, /*render_pass_id=*/0);
  SendMetadata(metadata);

  ink_renderer()->FinalizePathForDraw();
  EXPECT_EQ(GetPathPointCount(), 0);
}

// Tests that the OutstandingPointsToDraw histogram is fired correctly.
TEST_F(SkiaDelegatedInkRendererTest, SkiaDelegatedInkOutstandingPointsToDraw) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.Skia.OutstandingPointsToDraw";
  const base::HistogramTester histogram_tester;
  const int32_t kPointerId = 17;
  SetUpRenderers();

  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired when there are no points to draw.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Add one point, a histogram with a count of one should be fired.
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::PointF point(45.f, 78.f);
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  SendMetadata(gfx::DelegatedInkMetadata(
      gfx::PointF(point.x(), point.y()), 45.f, SK_ColorBLACK, timestamp,
      gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering=*/false, /*render_pass_id=*/0));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectUniqueSample(kHistogramName, 1, 1);

  // Add two point, a histogram with a count of two and three should be fired.
  CreateAndStoreDelegatedInkPoint(point + gfx::Vector2d(1, 1),
                                  timestamp + base::Milliseconds(10),
                                  kPointerId);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
  histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  CreateAndStoreDelegatedInkPoint(point + gfx::Vector2d(2, 2),
                                  timestamp + base::Milliseconds(20),
                                  kPointerId);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 3);
}

// Tests that the TimeToDrawMillis histogram is fired correctly.
TEST_F(SkiaDelegatedInkRendererTest, SkiaDelegatedInkTimeToDrawMillis) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.Skia.TimeToDrawPointsMillis";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;
  SetUpRenderers();

  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired when there are no points to draw.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Add one point to the trail and ensure one histogram instance is fired.
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::PointF point(45.f, 78.f);
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  SendMetadata(gfx::DelegatedInkMetadata(
      gfx::PointF(point.x(), point.y()), 45.f, SK_ColorBLACK, timestamp,
      gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering=*/false, /*render_pass_id=*/0));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Add two points to the trail and ensure that the histogram is fired three
  // times (three points, four total histogram fires accounting for the
  // previous).
  CreateAndStoreDelegatedInkPoint(point + gfx::Vector2d(1, 1),
                                  timestamp + base::Milliseconds(1),
                                  kPointerId);
  CreateAndStoreDelegatedInkPoint(point + gfx::Vector2d(2, 2),
                                  timestamp + base::Milliseconds(2),
                                  kPointerId);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 4);
}

TEST_F(SkiaDelegatedInkRendererTest,
       SkiaDelegatedInkTimeFromDelegatedInkToApiPaint) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.Skia.TimeFromDelegatedInkToApiPaint";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;
  const auto create_metadata = [](gfx::PointF& p, base::TimeTicks& t) {
    return gfx::DelegatedInkMetadata(p, /*diameter=*/45.f, SK_ColorBLACK, t,
                                     gfx::RectF(0, 0, 100, 100), t,
                                     /*hovering=*/false, /*render_pass_id=*/0);
  };
  SetUpRenderers();

  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired when `metadata_paint_time_` is not set.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Original timestamp and coordinates to be advanced for subsequent points
  // sent.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::PointF point(45.f, 78.f);
  const auto advance_point = [&]() {
    timestamp += base::Milliseconds(10);
    point += gfx::Vector2d(3.f, 3.f);
  };

  // Set up a trail, create a point and a metadata and call ReportPointsDrawn.
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  SendMetadata(create_metadata(point, timestamp));
  ink_renderer()->ReportPointsDrawn();
  EXPECT_NE(std::nullopt, GetPointsForPointerId(kPointerId)
                              .find(timestamp)
                              ->second.paint_timestamp());
  ink_renderer()->ReportPointsDrawn();

  // Add two delegated ink points to the trail and paint them.
  advance_point();
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  advance_point();
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  ink_renderer()->ReportPointsDrawn();
  // After drawing the points, all of them should have a `paint_timestamp_` set.
  for (auto& [_, p] : GetPointsForPointerId(kPointerId)) {
    EXPECT_NE(std::nullopt, p.paint_timestamp());
  }

  // Send a metadata that matches the last painted point.
  SendMetadata(create_metadata(point, timestamp));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // The histogram should not be fired when the metadata has not been updated.
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  // Send same metadata as before and report drawing. The histogram should not
  // be fired.
  SendMetadata(create_metadata(point, timestamp));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Send a new point, draw, a new metadata that matches the new point, draw
  // again and ensure that a new histogram is fired.
  advance_point();
  CreateAndStoreDelegatedInkPoint(point, timestamp, kPointerId);
  ink_renderer()->ReportPointsDrawn();
  SendMetadata(create_metadata(point, timestamp));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
}

enum class DelegatedInkType { kPlatformInk, kSkiaInk };

class DelegatedInkDisplayTest
    : public SkiaDelegatedInkRendererTest,
      public testing::WithParamInterface<DelegatedInkType> {
 public:
  void SetUpGpuDisplaySkiaWithPlatformInk(const RendererSettings& settings) {
    scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
    provider->BindToCurrentSequence();
    std::unique_ptr<FakeSkiaOutputSurface> skia_output_surface =
        FakeSkiaOutputSurface::Create3d(std::move(provider));
    // Set the delegated ink capability on the output surface to true so that
    // path can be tested in Display::DrawAndSwap
    skia_output_surface->UsePlatformDelegatedInkForTesting();
    skia_output_surface_ = skia_output_surface.get();

    CreateDisplaySchedulerAndDisplay(settings, kArbitraryFrameSinkId,
                                     std::move(skia_output_surface));
  }

  void SetUpGpuDisplay() {
    if (GetParam() == DelegatedInkType::kSkiaInk) {
      SetUpRenderers();
    } else {
      // Set up the display to use the Skia renderer.
      SetUpGpuDisplaySkiaWithPlatformInk(RendererSettings());

      display_->Initialize(&client_, manager_.surface_manager());
    }
  }

  void SubmitCompositorFrameWithInkMetadata(
      CompositorRenderPassList* pass_list,
      const LocalSurfaceId& local_surface_id,
      const gfx::DelegatedInkMetadata& metadata) {
    CompositorFrame frame = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(*pass_list))
                                .AddDelegatedInkMetadata(metadata)
                                .Build();
    pass_list->clear();

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  const gfx::DelegatedInkMetadata* GetMetadataFromTestRenderer() {
    return ink_renderer_->last_metadata();
  }
};

struct DelegatedInkDisplayTestPassToString {
  std::string operator()(
      const testing::TestParamInfo<DelegatedInkType> type) const {
    return type.param == DelegatedInkType::kPlatformInk ? "PlatformInk"
                                                        : "SkiaInk";
  }
};

INSTANTIATE_TEST_SUITE_P(DelegatedInkTrails,
                         DelegatedInkDisplayTest,
                         testing::Values(DelegatedInkType::kPlatformInk,
                                         DelegatedInkType::kSkiaInk),
                         DelegatedInkDisplayTestPassToString());

// Confirm that delegated ink metadata is not ever sent to both the delegated
// ink renderer and the output surface (for platform delegated ink), only one
// or the other.
TEST_P(DelegatedInkDisplayTest, MetadataOnlySentToSkiaRendererOrOutputSurface) {
  SetUpGpuDisplay();

  id_allocator_.GenerateId();
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.f);
  display_->Resize(gfx::Size(100, 100));

  CompositorRenderPassList pass_list;
  auto pass = CompositorRenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = CompositorRenderPassId{1u};
  pass_list.push_back(std::move(pass));

  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(5, 5), 3.5f, SK_ColorBLACK, base::TimeTicks::Now(),
      gfx::RectF(0, 0, 20, 20), base::TimeTicks::Now(), false,
      /*render_pass_id=*/0);

  SubmitCompositorFrameWithInkMetadata(
      &pass_list, id_allocator_.GetCurrentLocalSurfaceId(), metadata);
  display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});

  // Confirm that the metadata correctly made it to either the skia output
  // surface, or the delegated ink renderer.
  const gfx::DelegatedInkMetadata* retrieved_metadata =
      GetParam() == DelegatedInkType::kPlatformInk
          ? skia_output_surface_->last_delegated_ink_metadata()
          : GetMetadataFromTestRenderer();
  EXPECT_TRUE(retrieved_metadata);
  EXPECT_EQ(retrieved_metadata->point(), metadata.point());
  EXPECT_EQ(retrieved_metadata->diameter(), metadata.diameter());
  EXPECT_EQ(retrieved_metadata->color(), metadata.color());
  EXPECT_EQ(retrieved_metadata->timestamp(), metadata.timestamp());
  EXPECT_EQ(retrieved_metadata->presentation_area(),
            metadata.presentation_area());
  EXPECT_EQ(retrieved_metadata->is_hovering(), metadata.is_hovering());

  // Confirm that metadata wasn't sent to the SkiaOutputSurface if Skia was
  // used for drawing, or confirm that the DelegatedInkPointRenderer wasn't
  // created if platform ink is being used.
  if (GetParam() == DelegatedInkType::kPlatformInk)
    EXPECT_FALSE(ink_renderer());
  else
    EXPECT_FALSE(skia_output_surface_->last_delegated_ink_metadata());
}

// Check that a pending delegated ink point renderer sent to the display
// correctly goes to either the renderer or the output surface depending on if
// the platform supports delegated ink and the feature flag is enabled or not.
TEST_P(DelegatedInkDisplayTest,
       InkRendererRemoteGoesToSkiaRendererOrOutputSurface) {
  SetUpGpuDisplay();

  mojo::Remote<gfx::mojom::DelegatedInkPointRenderer> ink_renderer_remote;
  display_->InitDelegatedInkPointRendererReceiver(
      ink_renderer_remote.BindNewPipeAndPassReceiver());

  if (GetParam() == DelegatedInkType::kPlatformInk) {
    EXPECT_TRUE(skia_output_surface_
                    ->ContainsDelegatedInkPointRendererReceiverForTesting());
    EXPECT_FALSE(ink_renderer());
  } else {
    EXPECT_FALSE(skia_output_surface_
                     ->ContainsDelegatedInkPointRendererReceiverForTesting());
    EXPECT_TRUE(ink_renderer());
    EXPECT_TRUE(ink_renderer_remote.is_bound());
  }
}

using UnsupportedRendererDelegatedInkTest = DisplayTest;

// Confirm that trying to use delegated ink trails on SoftwareRenderer silently
// fails.
TEST_F(UnsupportedRendererDelegatedInkTest,
       DelegatedInkSilentlyFailsOnSoftwareRenderer) {
  SetUpSoftwareDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  // Should silently bail early from here. Test will crash if we actually try to
  // initialize the delegated ink point renderer.
  mojo::Remote<gfx::mojom::DelegatedInkPointRenderer> ink_renderer_remote;
  display_->InitDelegatedInkPointRendererReceiver(
      ink_renderer_remote.BindNewPipeAndPassReceiver());
}

INSTANTIATE_TEST_SUITE_P(,
                         UseMapRectDisplayTest,
                         testing::Bool(),
                         &PostTestCaseName);

}  // namespace viz
