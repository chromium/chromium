// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

size_t NumVisibleRects(const QuadList& quads) {
  size_t visible_rects = 0;
  for (const auto* q : quads) {
    if (!q->visible_rect.size().IsEmpty())
      visible_rects++;
  }
  return visible_rects;
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
        &shared_bitmap_manager_, settings, &debug_settings_, frame_sink_id,
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
          /*clip=*/absl::nullopt, /*contents_opaque=*/true,
          /*opacity=*/1.0f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
          /*layer_id=*/0u, /*fast_rounded_corner=*/false);
      auto* quad1 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad1->SetNew(shared_quad_state1, /*rect=*/sub_surface_rect,
                    /*visible_rect=*/sub_surface_rect,
                    SurfaceRange(absl::nullopt, sub_surface_id1),
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
          /*clip_rect=*/absl::nullopt,
          /*are_contents_opaque=*/true, /*opacity=*/1.0f, SkBlendMode::kSrcOver,
          /*sorting_context=*/0,
          /*layer_id=*/0u, /*fast_rounded_corner=*/false);
      auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad2->SetNew(shared_quad_state2, /*rect=*/rect1,
                    /*visible_rect=*/rect1,
                    SurfaceRange(absl::nullopt, sub_surface_id2),
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

// Quads that require blending should not be treated as occluders
// regardless of full opacity.
TEST_F(DisplayTest, DrawOcclusionWithBlending) {
  RendererSettings settings;
  settings.minimum_fragments_reduced = 0;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  auto src_rect = gfx::Rect(0, 0, 100, 100);
  auto dest_rect = gfx::Rect(25, 25, 25, 25);

  for (auto& render_pass : frame.render_pass_list) {
    bool is_root_render_pass = render_pass == frame.render_pass_list.back();

    auto* src_sqs = render_pass->CreateAndAppendSharedQuadState();
    src_sqs->SetAll(
        gfx::Transform(), src_rect, src_rect, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        is_root_render_pass ? SkBlendMode::kSrcOver : SkBlendMode::kSrcIn,
        /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* dest_sqs = render_pass->CreateAndAppendSharedQuadState();
    dest_sqs->SetAll(
        gfx::Transform(), dest_rect, dest_rect, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        is_root_render_pass ? SkBlendMode::kSrcOver : SkBlendMode::kDstIn,
        /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* src_quad =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    src_quad->SetNew(src_sqs, src_rect, src_rect, SkColors::kBlack, false);
    auto* dest_quad =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    dest_quad->SetNew(dest_sqs, dest_rect, dest_rect, SkColors::kRed, false);
  }

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.back()->quad_list));

  display_->RemoveOverdrawQuads(&frame);

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.back()->quad_list));
}

// Quads that intersect backdrop filter render pass quads should not be
// split because splitting may affect how the filter applies to an
// underlying quad.
TEST_F(DisplayTest, DrawOcclusionWithIntersectingBackdropFilter) {
  RendererSettings settings;
  settings.minimum_fragments_reduced = 0;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());
  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  // Rects, shared quad states and quads map 1:1:1
  gfx::Rect rects[3] = {
      gfx::Rect(75, 0, 50, 100),
      gfx::Rect(0, 0, 50, 50),
      gfx::Rect(0, 0, 100, 100),
  };
  SharedQuadState* shared_quad_states[3];
  DrawQuad* quads[3];

  // Set up the backdrop filter render pass
  auto& bd_render_pass = frame.render_pass_list.at(0);
  auto& root_render_pass = frame.render_pass_list.at(1);
  auto bd_filter_rect = rects[0];

  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateBlurFilter(5.0));
  bd_render_pass->SetAll(
      AggregatedRenderPassId{2}, bd_filter_rect, gfx::Rect(), gfx::Transform(),
      cc::FilterOperations(), backdrop_filters,
      gfx::RRectF(gfx::RectF(bd_filter_rect), 0), gfx::ContentColorUsage::kSRGB,
      false, false, false, false);

  // Add quads to root render pass
  for (int i = 0; i < 3; i++) {
    shared_quad_states[i] = root_render_pass->CreateAndAppendSharedQuadState();
    shared_quad_states[i]->SetAll(
        gfx::Transform(), rects[i], rects[i], gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    if (i == 0) {  // Backdrop filter quad
      auto* new_quad =
          root_render_pass->quad_list
              .AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
      new_quad->SetNew(shared_quad_states[i], rects[i], rects[i],
                       bd_render_pass->id, ResourceId(2), gfx::RectF(),
                       gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(),
                       gfx::RectF(), false, 1.f);
      quads[i] = new_quad;
    } else {
      auto* new_quad = root_render_pass->quad_list
                           .AllocateAndConstruct<SolidColorDrawQuad>();
      new_quad->SetNew(shared_quad_states[i], rects[i], rects[i],
                       SkColors::kBlack, false);
      quads[i] = new_quad;
    }
  }

  // +---+-+-+-+
  // | 1 | | . |
  // +---+ | 0 |
  // | 2   | . |
  // +-----+---+
  EXPECT_EQ(std::size(rects), root_render_pass->quad_list.size());
  display_->RemoveOverdrawQuads(&frame);
  ASSERT_EQ(std::size(rects), root_render_pass->quad_list.size());

  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(rects[i], root_render_pass->quad_list.ElementAt(i)->visible_rect);
  }
}

// Check if draw occlusion does not remove any DrawQuads when no quad is being
// covered completely.
TEST_F(DisplayTest, DrawOcclusionWithNonCoveringDrawQuad) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(25, 25, 50, 100);
  gfx::Rect rect4(150, 0, 50, 50);
  gfx::Rect rect5(0, 0, 120, 120);
  gfx::Rect rect6(25, 0, 50, 160);
  gfx::Rect rect7(0, 20, 100, 100);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // |    |
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // | +--|-+
  // +----+ |
  //   +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect2 (50, 50, 100x100)), the |quad_list| size remains the
    // same after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect2 - rect1 U rect2 = (100, 50, 50x50 U 50, 100, 100x50),
    // which cannot be represented by a smaller rect (its visible_rect stays
    // the same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  // +------+                                +------+
  // |      |                                |      |
  // | +--+ |          show on screen        |      |
  // +------+                =>              +------+
  //   |  |                                    |  |
  //   +--+                                    +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect3 (25, 25, 50x100)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect3 - rect1 U rect3 = (25, 100, 50x25), which updates its
    // visible_rect accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(25, 100, 50, 25),
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  //  +--+                                        +--+
  // +----+                                      +----+
  // ||  ||             shown on screen          |    |
  // +----+                                      +----+
  //  +--+                                        +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect7, rect7,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect6, rect6,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect7, rect7, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect7 (0, 20, 100x100)) cannot cover |quad2|
    // (define by rect6 (25, 0, 50x160)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect6 - rect7 = (25, 0, 50x20 U 25, 120, 50x40), which
    // cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect7,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect6,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  // +----+   +--+
  // |    |   +--+
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect4 (150, 0, 50x50)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect4 (150, 0, 50x50), its visible_rect stays the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect4,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect5, rect5,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect5 (0, 0, 120x120)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect5 - rect1 = (100, 0, 20x100 U 0, 100, 100x20),
    // which cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if the draw occlusion removes a DrawQuad that is hidden behind
// a smaller disjointed DrawQuad.
// NOTE: this test will fail if RendererSettings.kMaximumOccluderComplexity is
// reduced to 1, since |rects[1]| will become the only occluder, and the quad
// defined by |rects[2]| will not be occluded (removed).
TEST_F(DisplayTest, DrawOcclusionWithSingleOverlapBehindDisjointedDrawQuads) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  std::vector<gfx::Rect> rects;
  rects.emplace_back(0, 0, 100, 100);
  rects.emplace_back(150, 0, 150, 150);
  rects.emplace_back(25, 25, 50, 50);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  for (const gfx::Rect& rect : rects) {
    SharedQuadState* shared_quad_state =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    auto* quad = frame.render_pass_list.front()
                     ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
  }

  //              +-------+
  //  +-----+     |       |
  //  | +-+ |     |       |
  //  | +-+ |     |       |
  //  +-----+     +-------+
  {
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The third quad (defined by rects[2](25, 25, 50x50)) is completely
    // occluded by the first quad (defined by rects[0](0, 0, 100x100)), so the
    // third quad is removed from the |quad_list|, leaving the first and second
    // (defined by rects[1](150, 0, 150x150); the largest) quads intact.
    ASSERT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rects[0],
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rects[1],
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if the draw occlusion removes DrawQuads that are hidden behind
// two different sized disjointed DrawQuads.
// NOTE: this test will fail if RendererSettings.kMaximumOccluderComplexity is
// reduced to 1, since |rects[1]| will become the only occluder, and the quad
// defined by |rects[2]| will not be occluded (removed).
TEST_F(DisplayTest, DrawOcclusionWithMultipleOverlapBehindDisjointedDrawQuads) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  std::vector<gfx::Rect> rects;
  rects.emplace_back(0, 0, 100, 100);
  rects.emplace_back(150, 0, 150, 150);
  rects.emplace_back(25, 25, 50, 50);
  rects.emplace_back(150, 0, 100, 100);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  for (const gfx::Rect& rect : rects) {
    SharedQuadState* shared_quad_state =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    auto* quad = frame.render_pass_list.front()
                     ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
  }

  //              +-------+
  //  +-----+     +-----+ |
  //  | +-+ |     |     | |
  //  | +-+ |     |     | |
  //  +-----+     +-----+-+
  {
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The third (defined by rects[2](25, 25, 50x50)) and fourth (defined by
    // rects[3](150, 0, 100x100)) quads are completely occluded by the first
    // (defined by rects[0](0, 0, 100x100)) and second (defined by rects[1](150,
    // 0, 150x150)) quads, respectively, so both are removed from the
    // |quad_list|, leaving the first and and second quads intact.
    ASSERT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rects[0],
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rects[1],
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if draw occlusion removes DrawQuads that are not shown on screen.
TEST_F(DisplayTest, CompositorFrameWithOverlapDrawQuad) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 50, 50);
  gfx::Rect rect3(50, 50, 50, 25);
  gfx::Rect rect4(0, 0, 50, 50);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // completely overlapping: +-----+
  //                         |     |
  //                         +-----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| overlaps |quad1|, so |quad2| is removed from the |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
  //  +-----+
  //  | +-+ |
  //  | +-+ |
  //  +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is hiding behind |quad1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  // +-----+
  // |  +--|
  // |  +--|
  // +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is behind |quad1| and aligns with the edge of |quad1|, so |quad2|
    // is removed from the |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is covered by |quad 1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if draw occlusion works well with scale change transformer.
TEST_F(DisplayTest, CompositorFrameWithTransformer) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // Rect 2, 3, 4 are contained in rect 1 only after applying the half scale
  // matrix. They are repetition of CompositorFrameWithOverlapDrawQuad.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(100, 100, 100, 50);
  gfx::Rect rect4(0, 0, 120, 120);

  // Rect 5, 6, 7, 8, 9, 10 are not contained by rect 1 after applying the
  // double scale matrix. They are repetition of
  // DrawOcclusionWithNonCoveringDrawQuad.
  gfx::Rect rect5(25, 25, 60, 60);
  gfx::Rect rect6(12, 12, 25, 50);
  gfx::Rect rect7(75, 0, 25, 25);
  gfx::Rect rect8(0, 0, 60, 60);
  gfx::Rect rect9(12, 0, 25, 80);
  gfx::Rect rect10(0, 10, 50, 50);

  gfx::Transform half_scale;
  half_scale.Scale3d(0.5, 0.5, 0.5);
  gfx::Transform double_scale;
  double_scale.Scale(2, 2);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(half_scale, rect2, rect2, gfx::MaskFilterInfo(),
                               /*clip=*/absl::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| becomes (12, 12, 50x50) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(half_scale, rect3, rect3, gfx::MaskFilterInfo(),
                               /*clip=*/absl::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |rect3| becomes (25, 25, 50x25) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(half_scale, rect4, rect4, gfx::MaskFilterInfo(),
                               /*clip=*/absl::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |rect4| becomes (0, 0, 60x60) after applying half scale transform,
    // |quad2| is now covered by |quad1|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    shared_quad_state->SetAll(double_scale, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/absl::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The compositor frame contains only one quad, so |quad_list| remains 1
    // after calling RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect5, rect5,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect5|) becomes (50, 50, 120x120) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect6, rect6,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect6|) becomes (24, 24, 50x100) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is (12, 50, 25x12), quad2::visible_rect
    // updates accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(12, 50, 25, 12),
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect7, rect7,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect7, rect7, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect7|) becomes (150, 0, 50x50) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect7| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect7,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect8, rect8,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect8, rect8, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect8|) becomes (0, 0, 120x120) after
    // applying double scale transform, it is not covered by |quad1| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect8| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect8,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(double_scale, rect10, rect10,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect9, rect9,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect10, rect10, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect9, rect9, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect9|) becomes (24, 0, 50x160) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect10| (0, 20, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect9| is not a rect, quad2::visible_rect stays
    // the same
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect10,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect9,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }
}

// Check if draw occlusion works with transform at epsilon scale.
TEST_F(DisplayTest, CompositorFrameWithEpsilonScaleTransform) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect(0, 0, 100, 100);

  SkScalar epsilon = 0.000000001f;
  SkScalar larger_than_epsilon = 0.00000001f;
  gfx::Transform zero_scale;
  zero_scale.Scale(0, 0);
  gfx::Transform epsilon_scale;
  epsilon_scale.Scale(epsilon, epsilon);
  gfx::Transform larger_epsilon_scale;
  larger_epsilon_scale.Scale(larger_than_epsilon, larger_than_epsilon);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  gfx::Transform inverted;

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(zero_scale, rect, rect, gfx::MaskFilterInfo(),
                               absl::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // zero matrix transform is non-invertible, so |quad2| is not removed from
    // draw occlusion algorithm.
    EXPECT_FALSE(zero_scale.GetInverse(&inverted));
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(epsilon_scale, rect, rect, gfx::MaskFilterInfo(),
                               /*clip=*/absl::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/1, /*layer_id=*/0u,
                               /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // This test verifies that the draw occlusion algorithm does not break when
    // the scale of the transform is very close to zero. |epsilon_scale|
    // transform has the scale set to 10^-8. the quad is considering to be empty
    // after the transform, so it fails to intersect the occlusion rect.
    // |quad2| is not removed from draw occlusion.
    EXPECT_TRUE(epsilon_scale.GetInverse(&inverted));
    EXPECT_TRUE(cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                    epsilon_scale, rect)
                    .IsEmpty());
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(larger_epsilon_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // This test verifies that the draw occlusion algorithm works well with
    // small scales that is just larger than the epsilon scale in the previous
    // case. |larger_epsilon_scale| transform has the scale set to 10^-7.
    // |quad2| will be transformed to a tiny rect that is covered by the
    // occlusion rect, so |quad2| is removed.
    EXPECT_TRUE(larger_epsilon_scale.GetInverse(&inverted));
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if draw occlusion works with transform at negative scale.
TEST_F(DisplayTest, CompositorFrameWithNegativeScaleTransform) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect(0, 0, 100, 100);

  gfx::Transform negative_scale;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    negative_scale.Scale3d(-1, 1, 1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the x-axis is negated, |quad2| after applying transform does not
    // intersect with |quad| any more, so no quad is removed.
    // In target space:
    //          |
    //  q2 +----|----+ occlusion rect
    //     |    |    |
    // ---------+----------
    //          |
    //          |
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, -1, 1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the y-axis is negated, |quad2| after applying transform does not
    // intersect with |quad| any more, so no quad is removed.
    // In target space:
    //          |
    //          |----+ occlusion rect
    //          |    |
    // ---------+----------
    //          |    |
    //          |----+
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, 1, -1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since z-axis is missing in a 2d plane, negating the z-axis does not cause
    // |q2| to move at all. So |quad2| overlaps with |quad| in target space.
    // In target space:
    //          |
    //          |----+ occlusion rect
    //          |    |   q2
    // ---------+----------
    //          |
    //          |
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if draw occlusion works well with rotation transform.
//
//  +-----+                                  +----+
//  |     |   rotation (by 45 on y-axis) ->  |    |     same height
//  +-----+                                  +----+     reduced weight
TEST_F(DisplayTest, CompositorFrameWithRotation) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(75, 75, 10, 10);

  // rect 3 intersects with rect 1 initially
  gfx::Rect rect3(50, 50, 25, 100);

  gfx::Transform rotate;
  rotate.RotateAboutYAxis(45);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // Apply rotation transform on |rect1| only.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/absl::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0, /*layer_id=*/0u,
                              /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (75, 75 10x10). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    // Apply rotation transform on |rect1| and |rect2|.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              absl::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(rotate, rect2, rect2, gfx::MaskFilterInfo(),
                               absl::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 70x100) and |quad2| becomes
    // (53, 75 8x10) (after applying rotation transform). So |quad2| is behind
    // |quad|. |quad2| is removed from |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/absl::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0, /*layer_id=*/0u,
                              /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (50, 50, 25x100). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    // Since we only support updating |visible_rect| of DrawQuad with scale
    // or translation transform and rotation transform applies to quads,
    // |visible_rect| of |quad2| should not be changed.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              absl::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(rotate, rect3, rect3, gfx::MaskFilterInfo(),
                               absl::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since both |quad| and |quad2| went through the same transform and |rect1|
    // does not cover |rect3| initially, |quad| does not cover |quad2| in target
    // space.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

// Check if draw occlusion is handled correctly if the transform does not
// preserves 2d axis alignment.
TEST_F(DisplayTest, CompositorFrameWithPerspective) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 1, 1);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100);
  perspective.RotateAboutYAxis(45);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(perspective, rect1, rect1, gfx::MaskFilterInfo(),
                              absl::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The transform used on |quad| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. Since it takes too
    // long to define a enclosed rect to describe the occlusion region,
    // occlusion region is not defined and no changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(perspective, rect2, rect2, gfx::MaskFilterInfo(),
                               absl::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The transform used on |quad2| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. it's easy to find
    // an enclosing rect to describe |quad2|. |quad2| is hiding behind |quad|,
    // so it's removed from |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if draw occlusion works with transparent DrawQuads.
TEST_F(DisplayTest, CompositorFrameWithOpacityChange) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool are_contents_opaque = true;
  float opacity1 = 1.f;
  float opacityLess1 = 0.5f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), absl::nullopt,
        are_contents_opaque, opacityLess1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(), absl::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the opacity of |rect2| is less than 1, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), absl::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(), absl::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opacity of |rect1| to 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithOpaquenessChange) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), absl::nullopt,
        transparent_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the opaqueness of |rect2| is false, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opaqueness of |rect2| to true.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Test if draw occlusion skips 3d objects. https://crbug.com/833748
TEST_F(DisplayTest, CompositorFrameZTranslate) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(0, 0, 200, 100);

  gfx::Transform translate_back;
  translate_back.Translate3d(0, 0, 100);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // 2 rects inside of 3d object is completely overlapping.
  //                         +-----+
  //                         |     |
  //                         +-----+
  {
    shared_quad_state->SetAll(translate_back, rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/1,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/1,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since both |quad| and |quad2| are inside of a 3d object, DrawOcclusion
    // will not be applied to them.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(rect1,
              frame.render_pass_list.front()->quad_list.ElementAt(0)->rect);
    EXPECT_EQ(rect2,
              frame.render_pass_list.front()->quad_list.ElementAt(1)->rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithTranslateTransformer) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 2 and 3 are outside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(120, 120, 10, 10);
  gfx::Rect rect3(100, 100, 100, 20);

  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  gfx::Transform translate_up;
  translate_up.Translate(50, 50);
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //
    //   +----+
    //   |    |
    //   |    |
    //   +----+
    //           +-+
    //           +-+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), absl::nullopt,
        transparent_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| and |rect1| are disjoined as show in the first image. The size of
    // |quad_list| remains 2.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    //   quad content space:                                      target space:
    //   +----+
    //   |    |               translation transform
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))         +-----+
    //   +----+                       =>                              | +-+ |
    //           +-+                                                  | +-+ |
    //           +-+                                                  +-----+
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/absl::nullopt, opaque_content, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Move |quad| defind by |rect1| over |quad2| defind by |rect2| by applying
    // translation transform. |quad2| will be covered by |quad|, so |quad_list|
    // size is reduced by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    // After applying translation transform on rect1:
    //   before                                                        after
    //   +----+
    //   |    |
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))          +----+
    //   +----+                       =>                               |  +---+
    //           +---+                                                 |  +---+
    //           +---+                                                 +----+
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/absl::nullopt, opaque_content, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Move |quad| defined by |rect1| over |quad2| defined by |rect3| by
    // applying translation transform. In target space, |quad| is (50, 50,
    // 100x100) and |quad2| is (100, 100, 100x20). So the visible region of
    // |quad2| is (150, 100, 50x20).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(150, 100, 50, 20),
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithCombinedSharedQuadState) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);
  gfx::Rect rect3(10, 10, 120, 30);

  // rect 4 and 5 intersect with the combined rect of 1 and 2.
  gfx::Rect rect4(10, 10, 180, 30);
  gfx::Rect rect5(10, 10, 120, 100);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by both |rect1| and |rect2|, so |rect3| is
    // removed from |quad_list|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    //  rect1 & rect2                      rect 4 added
    //   +----+----+                       +----+----+-+
    //   |    |    |                       |____|____|_|
    //   |    |----+           =>          |    |----+
    //   +----+                            +----+
    //
    quad3 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state3->SetAll(
        gfx::Transform(), rect4, rect4, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad3->SetNew(shared_quad_state3, rect4, rect4, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 4 is
    // (160, 10, 30x30), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(160, 10, 30, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    //  rect1 & rect2                      rect 5 added
    //   +----+----+                       +----+----+
    //   |    |    |                       | +--|--+ |
    //   |    |----+           =>          | |  |--|-+
    //   +----+                            +-|--+  |
    //                                       +-----+
    shared_quad_state3->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad3->SetNew(shared_quad_state3, rect5, rect5, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 5 is
    // (10, 60, 120x50), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(10, 60, 120, 50),
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }
}

// Remove overlapping quads in non-root render passes.
TEST_F(DisplayTest, DrawOcclusionWithMultipleRenderPass) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  // rect 4 is identical to rect 3, but in a separate render pass.
  gfx::Rect rects[4] = {
      gfx::Rect(0, 0, 100, 100),
      gfx::Rect(100, 0, 60, 60),
      gfx::Rect(10, 10, 120, 30),
      gfx::Rect(10, 10, 120, 30),
  };

  SharedQuadState* shared_quad_states[4];
  SolidColorDrawQuad* quads[4];
  for (int i = 0; i < 4; i++) {
    // add all but quad 4 into non-root render pass.
    auto& render_pass =
        i == 3 ? frame.render_pass_list.back() : frame.render_pass_list.front();
    shared_quad_states[i] = render_pass->CreateAndAppendSharedQuadState();
    quads[i] =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_states[i]->SetAll(
        gfx::Transform(), rects[i], rects[i], gfx::MaskFilterInfo(),
        /*clip_rect=*/absl::nullopt, /*contents_opaque=*/true,
        /*opacity_f=*/1.f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quads[i]->SetNew(shared_quad_states[i], rects[i], rects[i],
                     SkColors::kBlack, false /*force_anti_aliasing_off*/);
  }

  auto& render_pass = frame.render_pass_list.front();
  auto& root_render_pass = frame.render_pass_list.back();
  EXPECT_EQ(3u, NumVisibleRects(render_pass->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(root_render_pass->quad_list));
  display_->RemoveOverdrawQuads(&frame);
  EXPECT_EQ(2u, NumVisibleRects(render_pass->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(root_render_pass->quad_list));
  EXPECT_EQ(rects[0], render_pass->quad_list.ElementAt(0)->visible_rect);
  EXPECT_EQ(rects[1], render_pass->quad_list.ElementAt(1)->visible_rect);
  EXPECT_EQ(rects[3], root_render_pass->quad_list.ElementAt(0)->visible_rect);
}

// Occlusion tracking should not persist across render passes.
TEST_F(DisplayTest, CompositorFrameWithMultipleRenderPass) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);

  auto render_pass2 = std::make_unique<AggregatedRenderPass>();
  render_pass2->SetNew(AggregatedRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                       gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));
  gfx::Rect rect3(10, 10, 120, 30);

  bool opaque_content = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.at(1)
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // rect1 and rect2 are from first RenderPass and rect 3 is from the second
    // RenderPass.
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by the unioned region of |rect1| and |rect2|.
    // But |rect3| so |rect3| is to be removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.at(1)->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.at(1)->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithCoveredRenderPass) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);

  auto render_pass2 = std::make_unique<AggregatedRenderPass>();
  render_pass2->SetNew(AggregatedRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                       gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));

  bool opaque_content = true;
  float opacity = 1.f;
  AggregatedRenderPassId render_pass_id{1};
  ResourceId mask_resource_id(2);

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();

  {
    // rect1 is a DrawQuad from SQS1 and which is also the RenderPass rect
    // from SQS2. The AggregatedRenderPassDrawQuad should not be occluded.
    //  rect1
    //   +----+
    //   |    |
    //   |    |
    //   +----+
    //

    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad1->SetNew(shared_quad_state2, rect1, rect1, render_pass_id,
                  mask_resource_id, gfx::RectF(), gfx::Size(),
                  gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                  1.0f);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| and |rect2| shares the same region where |rect1| is a draw
    // quad and |rect2| RenderPass. |rect2| will be not removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.at(1)->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithClip) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);
  gfx::Rect clip_rect(0, 0, 60, 60);
  gfx::Rect rect3(50, 50, 20, 10);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //  rect1 & rect2
    //   +------+
    //   |      |
    //   |   +-+|
    //   |   | ||
    //   +------+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| covers |rect2| as shown in the figure above, So the size of
    // |quad_list| is reduced by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    //  rect1 & rect2                             clip_rect & rect2
    //   +------+                                     +----+
    //   |      |                                     |    |
    //   |   +-+|             =>                      +----+ +-+
    //   +------+                                            +-+
    //
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), clip_rect,
        opaque_content, opacity, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // In the target space, a clip is applied on |quad| (defined by |clip_rect|,
    // (0, 0, 60x60) |quad| and |quad2| (50, 50, 25x25) don't intersect in the
    // target space. So no change is applied to quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    //  rect1(non-clip) & rect2                rect1(clip) & rect3
    //   +------+                                     +---+
    //   |   +-+|                                     |  +|+
    //   |   +-+|             =>                      +--+++
    //   +------+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), clip_rect,
        opaque_content, opacity, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // In the target space, a clip is applied on |quad| (defined by |rect3|,
    // (50, 50, 20x10)). |quad| intersects with |quad2| in the target space. The
    // visible region of |quad2| is (60, 50, 10x10). So |quad2| is updated
    // accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(60, 50, 10, 10),
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

// Check if draw occlusion works with copy requests in root RenderPass only.
TEST_F(DisplayTest, CompositorFrameWithCopyRequest) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    frame.render_pass_list.front()->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // root RenderPass contains |rect1|, |rect2| and copy_request (where
    // |rect2| is in |rect1|). Since our current implementation only supports
    // occlusion with copy_request on root RenderPass, |quad_list| reduces its
    // size by 1 after calling remove overdraw.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithRenderPass) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 0, 100, 100);
  gfx::Rect rect3(0, 0, 25, 25);
  gfx::Rect rect4(100, 0, 25, 25);
  gfx::Rect rect5(0, 0, 50, 50);
  gfx::Rect rect6(0, 75, 25, 25);
  gfx::Rect rect7(0, 0, 10, 10);

  bool opaque_content = true;
  AggregatedRenderPassId render_pass_id{1};
  ResourceId mask_resource_id(2);
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R2 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D1 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state4 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D2 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // RenderPass r1 and r2 are intersecting to each other; however, the opaque
    // regions D1 and D2 on R1 and R2 are not intersecting.
    // +-------+---+--------+
    // |_D1_|  |   |_D2_|   |
    // |       |   |        |
    // |   R1  |   |    R2  |
    // +-------+---+--------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect4, rect4, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    R1->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect2, rect2, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect4, rect4, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since AggregatedRenderPassDrawQuad |r1| and |r2|
    // cannot be removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // RenderPass R2 is contained in R1, but the opaque region of the two
    // RenderPasses are separated.
    // +-------+-----------+
    // |_D2_|  |      |_D1_|
    // |       |           |
    // |   R2  |       R1  |
    // +-------+-----------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect6, rect6, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect6, rect6, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since AggregatedRenderPassDrawQuad |r1| and |r2|
    // cannot be removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect6,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // RenderPass R2 is contained in R1, and opaque region of R2 in R1 as well.
    // +-+---------+-------+
    // |-+   |     |       |
    // |-----+     |       |
    // |   R2      |   R1  |
    // +-----------+-------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect7, rect7, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect7, rect7, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d2| is contained in |d1|
    // Since AggregatedRenderPassDrawQuad |r1| and |r2| cannot be removed to
    // reduce overdraw, |quad_list| is reduced by 1.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithMultipleDrawQuadInSharedQuadState) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect1_1(0, 0, 50, 50);
  gfx::Rect rect1_2(50, 0, 50, 50);
  gfx::Rect rect1_3(0, 50, 50, 50);
  gfx::Rect rect1_4(50, 50, 50, 50);
  gfx::Rect rect_in_rect1(0, 0, 60, 40);
  gfx::Rect rect_intersects_rect1(80, 0, 50, 30);

  gfx::Rect rect2(20, 0, 100, 100);
  gfx::Rect rect2_1(20, 0, 50, 50);
  gfx::Rect rect2_2(70, 0, 50, 50);
  gfx::Rect rect2_3(20, 50, 50, 50);
  gfx::Rect rect2_4(70, 50, 50, 50);
  gfx::Rect rect3(0, 0, 140, 60);
  gfx::Rect rect3_1(0, 0, 70, 30);
  gfx::Rect rect3_2(70, 0, 70, 30);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad4 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad5 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    // A Shared quad states contains 4 drawquads and it covers another draw
    // quad from different shared quad state.
    // +--+--+
    // +--|+ |
    // +--+--+
    // |  |  |
    // +--+--+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect_in_rect1, rect_in_rect1, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state, rect1_1, rect1_1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state, rect1_2, rect1_2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state, rect1_3, rect1_3, SkColors::kBlack, false);
    quad4->SetNew(shared_quad_state, rect1_4, rect1_4, SkColors::kBlack, false);
    quad5->SetNew(shared_quad_state2, rect_in_rect1, rect_in_rect1,
                  SkColors::kBlack, false);
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // covers the visible region of |shared_quad_state2|.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect1_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect1_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // A Shared quad states that contains 4 drawquads that intersect with
    // another shared quad state that contains 1 drawquad.
    // +--+-++--+
    // |  | +|--+
    // +--+--+
    // |  |  |
    // +--+--+
    quad5 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state2->SetAll(gfx::Transform(), rect_intersects_rect1,
                               rect_intersects_rect1, gfx::MaskFilterInfo(),
                               /*clip=*/absl::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad5->SetNew(shared_quad_state2, rect_intersects_rect1,
                  rect_intersects_rect1, SkColors::kBlack, false);
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. The
    // |visible_rect| of |quad5| is updated.
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect1_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect1_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(100, 0, 30, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(5)->visible_rect);
  }

  {
    // A Shared quad states that contains 4 DrawQuads that intersects with
    // another shared quad state that contains 2 DrawQuads.
    // +-+--+--+-+
    // +-|--|--|-+
    //   +--+--+
    //   |  |  |
    //   +--+--+

    auto* quad6 = frame.render_pass_list.front()
                      ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state, rect2_1, rect2_1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state, rect2_2, rect2_2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state, rect2_3, rect2_3, SkColors::kBlack, false);
    quad4->SetNew(shared_quad_state, rect2_4, rect2_4, SkColors::kBlack, false);
    quad5->SetNew(shared_quad_state2, rect3_1, rect3_1, SkColors::kBlack,
                  false);
    quad6->SetNew(shared_quad_state2, rect3_2, rect3_2, SkColors::kBlack,
                  false);
    EXPECT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. So the
    // |visible_rect| of DrawQuads in |share_quad_state2| are updated to the
    // region shown on screen.
    EXPECT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect2_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect2_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect2_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(0, 0, 20, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(5)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(120, 0, 20, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(6)->visible_rect);
  }
}

TEST_F(DisplayTest, CompositorFrameWithNonInvertibleTransform) {
  RendererSettings settings;
  SetUpGpuDisplay(settings);
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 50, 50);
  gfx::Rect rect3(0, 0, 10, 10);

  gfx::Transform invertible;
  auto non_invertible = gfx::Transform::RowMajor(10, 10, 0, 0,  // row 1
                                                 10, 10, 0, 0,  // row 2
                                                 0, 0, 1, 0,    // row 3
                                                 0, 0, 0, 1);   // row 4
  gfx::Transform non_invertible_miss_z;
  non_invertible_miss_z.Scale3d(1, 1, 0);
  bool opaque_content = true;
  float opacity = 1.f;
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state1 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  {
    // in quad content space:        in target space:
    // +-+---------+                 +-----------+----+
    // +-+   q1    |                 |        q1 | q3 |
    // | +----+    |                 | +----+    |    |
    // | | q2 |    |                 | | q2 |    |    |
    // | +----+    |                 | +----+    |    |
    // |           |                 |           |    |
    // +-----------+                 +-----------+    |
    //                               |                |
    //                               +----------------+
    // |quad1| forms an occlusion rect; |quad2| follows a invertible transform
    // and is hiding behind quad1; |quad3| follows a non-invertible transform
    // and it is not covered by the occlusion rect.
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::MaskFilterInfo(),
                               absl::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(invertible, rect2, rect2, gfx::MaskFilterInfo(),
                               absl::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        non_invertible, rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state1, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is removed because it is not shown on screen in the target space.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    // in quad content space:     in target space:
    // +--------+                 +--------+
    // | |      |                 | |      |
    // |-+      |                 |-+      |
    // |        |                 |        |
    // +--------+                 +--------+
    // Verify if draw occlusion can occlude quad with non-invertible
    // transform.
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::MaskFilterInfo(),
                               absl::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        non_invertible_miss_z, rect3, rect3, gfx::MaskFilterInfo(),
        absl::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state1, rect1, rect1, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // |quad3| follows an non-invertible transform and it's covered by the
    // occlusion rect. So |quad3| is removed from the |frame|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if draw occlusion works with very large DrawQuad. crbug.com/824528.
TEST_F(DisplayTest, DrawOcclusionWithLargeDrawQuad) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  // The size of this DrawQuad will be 237790x237790 > 2^32 (uint32_t.max())
  // which caused the integer overflow in the bug.
  gfx::Rect rect1(237790, 237790);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // |    |
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
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

  uint32_t frame_token_1 = kInvalidOrLocalFrameToken;
  uint32_t frame_token_2 = kInvalidOrLocalFrameToken;
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
    shared_quad_state1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect1,
        /*visible_quad_layer_rect=*/rect1,
        /*mask_filter_info=*/gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, /*contents_opaque=*/false,
        /*opacity_f=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad1 = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad1->SetNew(shared_quad_state1, rect1 /* rect */,
                  rect1 /* visible_rect */, SkColors::kBlack,
                  false /* force_anti_aliasing_off */);

    auto* shared_quad_state2 = pass->CreateAndAppendSharedQuadState();
    gfx::Rect rect2(gfx::Point(20, 20), sub_surface_size);
    shared_quad_state2->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect2,
        /*visible_quad_layer_rect=*/rect2,
        /*mask_filter_info=*/gfx::MaskFilterInfo(),
        /*clip=*/absl::nullopt, /*contents_opaque=*/false,
        /*opacity_f=*/1.f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad2->SetNew(shared_quad_state2, rect2 /* rect */,
                  rect2 /* visible_rect */,
                  SurfaceRange(absl::nullopt, sub_surface_id), SkColors::kBlack,
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
    // Immediately after submitting frame, because there is presentation
    // feedback queued up, ShouldSendBeginFrame should always return true.
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
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
    // Immediately after submitting frame, because there is presentation
    // feedback queued up, ShouldSendBeginFrame should always return true.
    EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));
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
    // Immediately after submitting frame, because there is presentation
    // feedback queued up, ShouldSendBeginFrame should always return true.
    EXPECT_TRUE(ShouldSendBeginFrame(sub_support.get(), frame_time));
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

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerDoesNotOcclude) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  // The quad with rounded corner does not completely cover the quad below it.
  // The corners of the below quad are visiblg through the clipped corners.
  gfx::Rect quad_rect(10, 10, 100, 100);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), quad_rect, quad_rect, gfx::MaskFilterInfo(),
        absl::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, quad_rect, quad_rect,
                          SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);

    // Since none of the quads are culled, there should be 2 quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerDoesOcclude) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  // The quad with rounded corner completely covers the quad below it.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::Rect occluded_quad_rect(13, 13, 994, 994);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect,
        gfx::MaskFilterInfo(), absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(DisplayTest, DrawOcclusionSplit) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  // The two partially occluded quads will be split into two additional quads,
  // preserving only the visible regions.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  //  +--------------------------------+
  //  |***+----------------------+ <- Large occluding Rect
  //  +---|-  -   -  - +  -  -  -|-----+
  //  |***|            .         |*****|
  //  |***+----------------------+*****|
  //  |****************|***************|
  //  +----------------+---------------+
  //
  // * -> Visible rect for the quads.

  const gfx::Rect occluding_rect(10, 10, 1000, 490);
  const gfx::Rect quad_rects[3] = {
      gfx::Rect(0, 0, 1200, 20),
      gfx::Rect(0, 20, 600, 490),
      gfx::Rect(600, 20, 600, 490),
  };
  gfx::Rect occluded_sqs_rect(0, 0, 1200, 510);

  const bool are_contents_opaque = true;
  const float opacity = 1.f;
  SharedQuadState* shared_quad_state_occluder =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  SolidColorDrawQuad* quads[4];
  for (auto*& quad : quads) {
    quad = frame.render_pass_list.front()
               ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  }

  {
    shared_quad_state_occluder->SetAll(
        gfx::Transform(), occluding_rect, occluding_rect, gfx::MaskFilterInfo(),
        absl::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quads[0]->SetNew(shared_quad_state_occluder, occluding_rect, occluding_rect,
                     SkColors::kRed, false);

    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_sqs_rect, occluded_sqs_rect,
        gfx::MaskFilterInfo(), absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    for (int i = 1; i < 4; i++) {
      quads[i]->SetNew(shared_quad_state_occluded, quad_rects[i - 1],
                       quad_rects[i - 1], SkColors::kRed, false);
    }

    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    ASSERT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        occluding_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);

    // Computed the expected quads
    //  +--------------------------------+
    //  |                1               |
    //  +---+----------------------+-----+
    //  | 2 |                      |  3  |
    //  +---+------------+---------+-----+
    //  |        4       |        5      |
    //  +----------------+---------------+
    const gfx::Rect expected_visible_rects[5]{
        // The occluded region of rest one is small, so we do not split the
        // quad.
        quad_rects[0],
        gfx::Rect(0, 20, 10, 480),
        gfx::Rect(0, 500, 600, 10),
        gfx::Rect(1010, 20, 190, 480),
        gfx::Rect(600, 500, 600, 10),
    };

    const QuadList& quad_list = frame.render_pass_list.front()->quad_list;
    for (int i = 0; i < 5; i++) {
      EXPECT_EQ(expected_visible_rects[i],
                quad_list.ElementAt(i + 1)->visible_rect);
    }
  }
}

// Tests cases in which occlusion culling splits are performed due to first pass
// complexity reduction in visible regions. For more details, see:
// https://tinyurl.com/RegionComplexityReduction#heading=h.fg95k5w5t791
TEST_F(DisplayTest, FirstPassVisibleComplexityReduction) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  const bool are_contents_opaque = true;
  const float opacity = 1.f;

  //  +---------+-------+--------------+
  //  |*********|       |**************|
  //  |*********|       +------+*******|
  //  |*********|       |      |*******|
  //  |*********|       +------+*******|
  //  |*********|       |**************|
  //  +---------+-------+--------------+
  //
  //  *--> occluded quad
  //
  // This configuration will produce the following visible region for the
  // occluded quad.
  //  +---------+       +--------------+
  //  |    1    |       |      2       |
  //  |---------+       +------+-------|
  //  |    3    |              |   4   |
  //  |---------+       +------+-------|
  //  |    5    |       |      6       |
  //  +---------+       +--------------+
  //
  // The above split is unnecessarily complex. Rectangles 1, 3, and 5 should be
  // merged:
  //  +---------+       +--------------+
  //  |         |       |      2       |
  //  |         |       +------+-------|
  //  |    1    |              |   3   |
  //  |         |       +------+-------|
  //  |         |       |      4       |
  //  +---------+       +--------------+
  //
  // If the merge is not done, this visible region will be discarded and the
  // quad will not be split.

  const gfx::Rect occluding_rects[2] = {
      gfx::Rect(300, 0, 550, 270),
      gfx::Rect(850, 50, 150, 150),
  };
  for (const auto& r : occluding_rects) {
    SharedQuadState* shared_quad_state_occluder =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state_occluder->SetAll(
        gfx::Transform(), r, r, gfx::MaskFilterInfo(), /*clip=*/absl::nullopt,
        are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    SolidColorDrawQuad* quad =
        frame.render_pass_list.front()
            ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state_occluder, r, r, SkColors::kRed, false);
  }

  const gfx::Rect occluded_rect(0, 0, 1350, 270);
  {
    SharedQuadState* shared_quad_state_occluded =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_rect, occluded_rect, gfx::MaskFilterInfo(),
        absl::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    SolidColorDrawQuad* occluded_quad =
        frame.render_pass_list.front()
            ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_rect,
                          occluded_rect, SkColors::kRed, false);
  }

  EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  display_->RemoveOverdrawQuads(&frame);
  ASSERT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

  //  Expected visible quads:
  //  +---------+-------+--------------+
  //  |*********|       |******4*******|
  //  |*********|       +------+-------|
  //  |****3****|   1   |   2  |***5***|
  //  |*********|       +------+-------|
  //  |*********|       |******6*******|
  //  +---------+-------+--------------+
  //
  // * -> Visible rect for the quads.

  const gfx::Rect expected_visible_rects[6] = {
      occluding_rects[0],
      occluding_rects[1],
      gfx::Rect(0, 0, 300, 270),
      gfx::Rect(850, 0, 500, 50),
      gfx::Rect(1000, 50, 350, 150),
      gfx::Rect(850, 200, 500, 70),
  };

  for (size_t i = 0; i < std::size(expected_visible_rects); ++i) {
    EXPECT_EQ(
        expected_visible_rects[i],
        frame.render_pass_list.front()->quad_list.ElementAt(i)->visible_rect);
  }
}

// Test that the threshold we use to determine if it's worth splitting a quad or
// not takes into account the device scale factor. In particular, this test
// would not pass if we had a display scale factor equal to 1.f instead of 1.5f
// since the number of saved fragments would only be 100x100 which is lower than
// our threshold 128x128.
TEST_F(DisplayTest, DrawOcclusionSplitDeviceScaleFactorFractional) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());
  display_->SetLocalSurfaceId(id_allocator_.GetCurrentLocalSurfaceId(), 1.5f);
  display_->Resize(gfx::Size(1000, 1000));

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  const bool are_contents_opaque = true;
  const float opacity = 1.f;

  // Occluder quad.
  const gfx::Rect occluding_rect(10, 10, 100, 100);
  SharedQuadState* shared_quad_state_occluding =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SolidColorDrawQuad* occluding_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  shared_quad_state_occluding->SetAll(
      gfx::Transform(), occluding_rect, occluding_rect, gfx::MaskFilterInfo(),
      absl::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
      /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  occluding_quad->SetNew(shared_quad_state_occluding, occluding_rect,
                         occluding_rect, SkColors::kRed, false);
  // Occluded quad.
  const gfx::Rect occluded_rect = gfx::Rect(0, 0, 1000, 1000);
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SolidColorDrawQuad* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  shared_quad_state_occluded->SetAll(
      gfx::Transform(), occluded_rect, occluded_rect, gfx::MaskFilterInfo(),
      absl::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
      /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  occluded_quad->SetNew(shared_quad_state_occluded, occluded_rect,
                        occluded_rect, SkColors::kRed, false);

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  display_->RemoveOverdrawQuads(&frame);
  EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
}

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerPartialOcclude) {
  SetUpGpuDisplay(RendererSettings());
  display_->Initialize(client_.get(), manager_.surface_manager());

  // The quad with rounded corner completely covers the quad below it.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  //      +----------------------+
  //      |                      | <- Large occluding Rect
  //  +---|-  -  -  -  +  -  -  -|-------+
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  +---|-  -  -  -  +  -  -  -|-------+
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  +---|-  -  -  -  +  -  -  -|-------+
  //      |                      |
  //      +----------------------+
  //
  // * -> Visible rect for the quads.
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));
  gfx::Rect occluded_quad_rect_1(0, 20, 600, 490);
  gfx::Rect occluded_quad_rect_2(600, 20, 600, 490);
  gfx::Rect occluded_quad_rect_3(0, 510, 600, 490);
  gfx::Rect occluded_quad_rect_4(600, 510, 600, 490);
  gfx::Rect occluded_sqs_rect;
  occluded_sqs_rect.Union(occluded_quad_rect_1);
  occluded_sqs_rect.Union(occluded_quad_rect_2);
  occluded_sqs_rect.Union(occluded_quad_rect_3);
  occluded_sqs_rect.Union(occluded_quad_rect_4);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_2 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_3 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_4 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_sqs_rect, occluded_sqs_rect,
        gfx::MaskFilterInfo(), absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    occluded_quad_1->SetNew(shared_quad_state_occluded, occluded_quad_rect_1,
                            occluded_quad_rect_1, SkColors::kRed, false);
    occluded_quad_2->SetNew(shared_quad_state_occluded, occluded_quad_rect_2,
                            occluded_quad_rect_2, SkColors::kRed, false);
    occluded_quad_3->SetNew(shared_quad_state_occluded, occluded_quad_rect_3,
                            occluded_quad_rect_3, SkColors::kRed, false);
    occluded_quad_4->SetNew(shared_quad_state_occluded, occluded_quad_rect_4,
                            occluded_quad_rect_4, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/absl::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    display_->RemoveOverdrawQuads(&frame);
    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);

    // For rounded rect of bounds (10, 10, 1000, 1000) and corner radius of 10,
    // the occluding rect for it would be (13, 13, 994, 994).
    const gfx::Rect occluding_rect(13, 13, 994, 994);

    // Computed the expe
    gfx::Rect expected_visible_rect_1 = occluded_quad_rect_1;
    expected_visible_rect_1.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_2 = occluded_quad_rect_2;
    expected_visible_rect_2.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_3 = occluded_quad_rect_3;
    expected_visible_rect_3.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_4 = occluded_quad_rect_4;
    expected_visible_rect_4.Subtract(occluding_rect);

    const QuadList& quad_list = frame.render_pass_list.front()->quad_list;

    EXPECT_EQ(expected_visible_rect_1, quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(expected_visible_rect_2, quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(expected_visible_rect_3, quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(expected_visible_rect_4, quad_list.ElementAt(4)->visible_rect);
  }
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
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_90, kSize},
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_180, kSize},
      {false, gfx::OVERLAY_TRANSFORM_ROTATE_270, kSize},

      // Output size is transposed on 90/270 degree rotation when output surface
      // supports display transform hint.
      {true, gfx::OVERLAY_TRANSFORM_NONE, kSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_90, kTransposedSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_180, kSize},
      {true, gfx::OVERLAY_TRANSFORM_ROTATE_270, kTransposedSize},
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
                                   SurfaceRange(absl::nullopt, sub_surface_id1),
                                   {.allow_merge = false})
                               .AddSurfaceQuad(
                                   gfx::Rect(display_size),
                                   SurfaceRange(absl::nullopt, sub_surface_id2),
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
                                                 SurfaceRange(absl::nullopt,
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

  void SetUp() override { EnablePrediction(); }

  void SetUpRenderers() {
    SetUpGpuDisplay(RendererSettings());

    // Initialize the renderer and create an ink renderer.
    display_->Initialize(&client_, manager_.surface_manager());

    auto renderer = std::make_unique<DelegatedInkPointRendererSkiaForTest>();
    ink_renderer_ = renderer.get();
    display_->renderer_for_testing()->SetDelegatedInkPointRendererSkiaForTest(
        std::move(renderer));
  }

  void EnablePrediction() {
    base::FieldTrialParams params;
    params["predicted_points"] = ::features::kDraw1Point12Ms;
    base::test::FeatureRefAndParams prediction_params = {
        features::kDrawPredictedInkPoint, params};

    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters({prediction_params}, {});
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

  const std::map<base::TimeTicks, gfx::PointF>& GetPointsForPointerId(
      int32_t pointer_id) {
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

    // TODO(crbug.com/1308932): gfx::DelegatedInkMetadata to SkColor4f
    gfx::DelegatedInkMetadata metadata(
        ink_points_[pointer_id][index].point(), diameter, color.toSkColor(),
        ink_points_[pointer_id][index].timestamp(), presentation_area,
        base::TimeTicks::Now(),
        /*hovering*/ false);
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
        base::StrCat({"Renderer.DelegatedInkTrail."
                      "LatencyImprovementWithPrediction.Experiment",
                      base::NumberToString(PredictionConfig::k1Point12Ms)})
            .c_str());
  }

  void DrawDelegatedInkTrail() {
    SkCanvas canvas;
    static_cast<DelegatedInkPointRendererSkia*>(ink_renderer())
        ->DrawDelegatedInkTrail(&canvas);
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

 protected:
  raw_ptr<DelegatedInkPointRendererSkiaForTest> ink_renderer_ = nullptr;

  // Stub client kept in scope to prevent access violations during DrawAndSwap.
  StubDisplayClient client_;

  base::test::ScopedFeatureList feature_list_;

 private:
  std::unordered_map<int32_t, std::vector<gfx::DelegatedInkPoint>> ink_points_;
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

  // No metadata has been provided yet, so filtering shouldn't occur and all
  // points should still exist after a FinalizePath() call.
  FinalizePathAndCheckHistograms(base::TimeDelta::Min(),
                                 base::TimeDelta::Min());

  EXPECT_EQ(1, UniqueStoredPointerIds());
  EXPECT_EQ(kInitialDelegatedPoints, StoredPointsForPointerId(kPointerId));

  // Now provide metadata with a timestamp matching one of the points to
  // confirm that earlier points are removed and later points remain.
  const int kInkPointForMetadata = 1;
  const float kDiameter = 1.f;
  gfx::DelegatedInkMetadata metadata = MakeAndSendMetadataFromStoredInkPoint(
      kInkPointForMetadata, kDiameter, SkColors::kBlack, gfx::RectF());

  // The histogram should count one in the bucket that is the difference between
  // the latest point stored and the metadata. No prediction should occur with
  // 3 provided points, so the *WithoutPrediction histogram should count
  // the difference between the last point and the metadata, while the
  // *WithPrediction* histogram should count 1 in the 0 bucket.
  base::TimeDelta bucket_without_prediction =
      last_ink_point(kPointerId).timestamp() - metadata.timestamp();
  FinalizePathAndCheckHistograms(bucket_without_prediction,
                                 base::Milliseconds(0));

  EXPECT_EQ(kInitialDelegatedPoints - kInkPointForMetadata,
            StoredPointsForPointerId(kPointerId));
  EXPECT_EQ(metadata.point(),
            GetPointsForPointerId(kPointerId).begin()->second);
  EXPECT_EQ(last_ink_point(kPointerId).point(),
            GetPointsForPointerId(kPointerId).rbegin()->second);
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
            GetPointsForPointerId(kPointerId).begin()->second);
  EXPECT_EQ(last_ink_point(kPointerId).point(),
            GetPointsForPointerId(kPointerId).rbegin()->second);
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
      bucket_without_prediction +
          base::Milliseconds(kPredictionConfigs[PredictionConfig::k1Point12Ms]
                                 .milliseconds_into_future_per_point *
                             kPredictionConfigs[PredictionConfig::k1Point12Ms]
                                 .points_to_predict));

  // Confirm the size, first, and last points of the first pointer ID are what
  // we expect.
  EXPECT_EQ(kNumPointsForPointerId0 - kInkPointForMetadata,
            StoredPointsForPointerId(kPointerIds[0]));
  EXPECT_EQ(metadata.point(),
            GetPointsForPointerId(kPointerIds[0]).begin()->second);
  EXPECT_EQ(last_ink_point(kPointerIds[0]).point(),
            GetPointsForPointerId(kPointerIds[0]).rbegin()->second);

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
      gfx::RectF(), base::TimeTicks::Min(), /*hovering*/ false));
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
      base::TimeTicks::Now(), /*hovering*/ false));
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
      bucket_without_prediction +
          base::Milliseconds(kPredictionConfigs[PredictionConfig::k1Point12Ms]
                                 .milliseconds_into_future_per_point *
                             kPredictionConfigs[PredictionConfig::k1Point12Ms]
                                 .points_to_predict));

  // Now provide metadata that matches the final ink point provided, so that
  // everything earlier is filtered out. Then the *WithoutPrediction histogram
  // will count 1 in the 0 ms bucket and the *WithPrediction histogram will
  // still be able to predict points, so it should have counted one.
  MakeAndSendMetadataFromStoredInkPoint(/*index*/ 3, kDiameter,
                                        SkColors::kBlack, gfx::RectF());
  bucket_without_prediction = base::Milliseconds(0);
  FinalizePathAndCheckHistograms(
      bucket_without_prediction,
      base::Milliseconds(
          kPredictionConfigs[PredictionConfig::k1Point12Ms]
              .milliseconds_into_future_per_point *
          kPredictionConfigs[PredictionConfig::k1Point12Ms].points_to_predict));

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
      gfx::PointF(point.x() - 0.03f, point.y() + 0.03f), 45.f, SK_ColorBLACK,
      timestamp, gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering*/ false);
  SendMetadata(metadata);

  // If the metadata was close enough, then a trail should be drawn with all
  // three points.
  ink_renderer()->FinalizePathForDraw();
  EXPECT_EQ(GetPathPointCount(), 3);

  // Now send a metadata with a point that is slightly further away from the
  // second point, such that the distance between them is greater than the
  // kEpsilon value to confirm that if it gets too far away we won't use it for
  // drawing.
  metadata = gfx::DelegatedInkMetadata(
      gfx::PointF(point2.x() - 0.03f, point2.y() + 0.04f), 45.f, SK_ColorBLACK,
      timestamp2, gfx::RectF(0, 0, 100, 100), base::TimeTicks::Now(),
      /*hovering*/ false);
  SendMetadata(metadata);

  ink_renderer()->FinalizePathForDraw();
  EXPECT_EQ(GetPathPointCount(), 0);
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
      scoped_feature_list_.InitAndEnableFeature(
          features::kUsePlatformDelegatedInk);

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
      gfx::RectF(0, 0, 20, 20), base::TimeTicks::Now(), false);

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
