// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "cc/base/math_util.h"
#include "cc/test/scheduler_test_common.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(3, 3);
static constexpr FrameSinkId kAnotherFrameSinkId(4, 4);
static constexpr FrameSinkId kAnotherFrameSinkId2(5, 5);

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice() {}

  gfx::Rect damage_rect() const { return damage_rect_; }
  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
};

class TestDisplayScheduler : public DisplayScheduler {
 public:
  explicit TestDisplayScheduler(BeginFrameSource* begin_frame_source,
                                base::SingleThreadTaskRunner* task_runner)
      : DisplayScheduler(begin_frame_source, task_runner, 1),
        damaged(false),
        display_resized_(false),
        has_new_root_surface(false),
        swapped(false) {}

  ~TestDisplayScheduler() override {}

  void DisplayResized() override { display_resized_ = true; }

  void SetNewRootSurface(const SurfaceId& root_surface_id) override {
    has_new_root_surface = true;
  }

  void ProcessSurfaceDamage(const SurfaceId& surface_id,
                            const BeginFrameAck& ack,
                            bool display_damaged) override {
    if (display_damaged) {
      damaged = true;
      needs_draw_ = true;
    }
  }

  void DidSwapBuffers() override { swapped = true; }

  void ResetDamageForTest() {
    damaged = false;
    display_resized_ = false;
    has_new_root_surface = false;
  }

  bool damaged;
  bool display_resized_;
  bool has_new_root_surface;
  bool swapped;
};

class StubDisplayClient : public DisplayClient {
 public:
  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              RenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id) override {
    return BeginFrameArgs::MinInterval();
  }
};

void CopyCallback(bool* called, std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

gfx::SwapTimings GetTestSwapTimings() {
  base::TimeTicks now = base::TimeTicks::Now();
  return gfx::SwapTimings{now, now};
}

}  // namespace

class DisplayTest : public testing::Test {
 public:
  DisplayTest()
      : manager_(&shared_bitmap_manager_),
        support_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryFrameSinkId,
            true /* is_root */,
            true /* needs_sync_points */)),
        task_runner_(new base::NullTaskRunner) {}

  ~DisplayTest() override {}

  void SetUpSoftwareDisplay(const RendererSettings& settings) {
    std::unique_ptr<FakeOutputSurface> output_surface;
    auto device = std::make_unique<TestSoftwareOutputDevice>();
    software_output_device_ = device.get();
    output_surface = FakeOutputSurface::CreateSoftware(std::move(device));
    output_surface_ = output_surface.get();

    CreateDisplaySchedulerAndDisplay(settings, kArbitraryFrameSinkId,
                                     std::move(output_surface));
  }

  void SetUpGpuDisplay(const RendererSettings& settings) {
    scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
    provider->BindToCurrentThread();
    std::unique_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::Create3d(std::move(provider));
    output_surface_ = output_surface.get();

    CreateDisplaySchedulerAndDisplay(settings, kArbitraryFrameSinkId,
                                     std::move(output_surface));
  }

  void CreateDisplaySchedulerAndDisplay(
      const RendererSettings& settings,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<OutputSurface> output_surface) {
    begin_frame_source_.reset(new StubBeginFrameSource);
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
    auto display = std::make_unique<Display>(
        &shared_bitmap_manager_, settings, frame_sink_id,
        std::move(output_surface), std::move(scheduler), task_runner_);
    display->SetVisible(true);
    return display;
  }

  void TearDownDisplay() {
    // Only call UnregisterBeginFrameSource if SetupDisplay has been called.
    if (begin_frame_source_)
      manager_.UnregisterBeginFrameSource(begin_frame_source_.get());
  }

  bool ShouldSendBeginFrame(CompositorFrameSinkSupport* support,
                            base::TimeTicks frame_time) {
    return support->ShouldSendBeginFrame(frame_time);
  }

  void UpdateBeginFrameTime(CompositorFrameSinkSupport* support,
                            base::TimeTicks frame_time) {
    support->last_frame_time_ = frame_time;
    support->frame_timing_details_.clear();
  }

 protected:
  void SubmitCompositorFrame(RenderPassList* pass_list,
                             const LocalSurfaceId& local_surface_id) {
    CompositorFrame frame = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(*pass_list))
                                .Build();
    pass_list->clear();

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  void RunAllPendingInMessageLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void LatencyInfoCapTest(bool over_capacity);

  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  ParentLocalSurfaceIdAllocator id_allocator_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  std::unique_ptr<Display> display_;
  TestSoftwareOutputDevice* software_output_device_ = nullptr;
  FakeOutputSurface* output_surface_ = nullptr;
  TestDisplayScheduler* scheduler_ = nullptr;
};

// Check that frame is damaged and swapped only under correct conditions.
TEST_F(DisplayTest, DisplayDamaged) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  SetUpSoftwareDisplay(settings);
  gfx::ColorSpace color_space_1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space_2 = gfx::ColorSpace::CreateSCRGBLinear();

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetColorSpace(color_space_1);

  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->has_new_root_surface);
  id_allocator_.GenerateId();
  display_->SetLocalSurfaceId(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
      1.f);
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_TRUE(scheduler_->has_new_root_surface);

  scheduler_->ResetDamageForTest();
  display_->Resize(gfx::Size(100, 100));
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_TRUE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  // First draw from surface should have full damage.
  RenderPassList pass_list;
  auto pass = RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = 1u;
  pass_list.push_back(std::move(pass));

  scheduler_->ResetDamageForTest();
  SubmitCompositorFrame(
      &pass_list,
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
  EXPECT_TRUE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  EXPECT_FALSE(scheduler_->swapped);
  EXPECT_EQ(0u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::ColorSpace(), output_surface_->last_reshape_color_space());
  display_->DrawAndSwap();
  EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
  EXPECT_TRUE(scheduler_->swapped);
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::Size(100, 100),
            software_output_device_->viewport_pixel_size());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), software_output_device_->damage_rect());

  // Only damaged portion should be swapped.
  {
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
    display_->SetColorSpace(color_space_2);
    display_->DrawAndSwap();
    EXPECT_EQ(color_space_2, output_surface_->last_reshape_color_space());
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(10, 10, 1, 1), software_output_device_->damage_rect());

    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Pass has no damage so shouldn't be swapped.
  {
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  // Pass is wrong size so shouldn't be swapped. However, damage should
  // result in latency info being stored for the next swap.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        1.f);

    scheduler_->ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 99, 99);
    constexpr gfx::Rect kDamageRect(10, 10, 10, 10);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .AddLatencyInfo(ui::LatencyInfo())
                                .Build();

    support_->SubmitCompositorFrame(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        std::move(frame));
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  // Previous frame wasn't swapped, so next swap should have full damage.
  {
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        1.f);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());

    EXPECT_EQ(1u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Pass has copy output request so should be swapped.
  {
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    bool copy_called = false;
    pass->copy_requests.push_back(std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA_BITMAP,
        base::BindOnce(&CopyCallback, &copy_called)));
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
    EXPECT_TRUE(copy_called);
  }

  // Pass has no damage, so shouldn't be swapped and latency info should be
  // discarded.
  {
    scheduler_->ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 100, 100);
    constexpr gfx::Rect kDamageRect(10, 10, 0, 0);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .AddLatencyInfo(ui::LatencyInfo())
                                .Build();

    support_->SubmitCompositorFrame(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        std::move(frame));
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    frame.metadata.latency_info.push_back(ui::LatencyInfo());
    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
  }

  // DisableSwapUntilResize() should cause a swap if no frame was swapped at the
  // previous size.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        1.f);
    scheduler_->swapped = false;
    display_->Resize(gfx::Size(200, 200));
    EXPECT_FALSE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
    scheduler_->ResetDamageForTest();

    constexpr gfx::Rect kOutputRect(0, 0, 200, 200);
    constexpr gfx::Rect kDamageRect(10, 10, 10, 10);
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(kOutputRect, kDamageRect)
                                .Build();

    support_->SubmitCompositorFrame(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        std::move(frame));
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DisableSwapUntilResize(base::OnceClosure());
    display_->Resize(gfx::Size(100, 100));
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(5u, output_surface_->num_sent_frames());
    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }

  // Surface that's damaged completely should be resized and swapped.
  {
    id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
        1.0f);
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(0, 0, 99, 99);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(6u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }
  TearDownDisplay();
}

// Verifies latency info is stored only up to a limit if a swap fails.
void DisplayTest::LatencyInfoCapTest(bool over_capacity) {
  SetUpSoftwareDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));

  // Start off with a successful swap so output_surface_->last_sent_frame() is
  // valid.
  constexpr gfx::Rect kOutputRect(0, 0, 100, 100);
  constexpr gfx::Rect kDamageRect(10, 10, 1, 1);
  CompositorFrame frame1 =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame1));

  display_->DrawAndSwap();
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

  EXPECT_TRUE(display_->DrawAndSwap());
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());

  // Run a successful swap and verify whether or not LatencyInfo was discarded.
  display_->Resize(gfx::Size(100, 100));
  CompositorFrame frame3 =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame3));
  EXPECT_TRUE(display_->DrawAndSwap());

  // Verify whether or not LatencyInfo was dropped.
  size_t expected_size = 0;
  if (!over_capacity)
    expected_size += max_latency_info_count;
  EXPECT_EQ(2u, output_surface_->num_sent_frames());
  EXPECT_EQ(expected_size,
            output_surface_->last_sent_frame()->latency_info.size());

  TearDownDisplay();
}

TEST_F(DisplayTest, UnderLatencyInfoCap) {
  LatencyInfoCapTest(false);
}

TEST_F(DisplayTest, OverLatencyInfoCap) {
  LatencyInfoCapTest(true);
}

TEST_F(DisplayTest, DisableSwapUntilResize) {
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id1(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id2(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());

  RendererSettings settings;
  settings.partial_swap_enabled = true;

  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  display_->SetLocalSurfaceId(local_surface_id1, 1.f);

  display_->Resize(gfx::Size(100, 100));

  {
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id1);
  }

  EXPECT_FALSE(scheduler_->swapped);

  // DisableSwapUntilResize() should trigger a swap because we have a frame of
  // the correct size and haven't swapped at that size yet.
  bool swap_callback_run = false;
  display_->DisableSwapUntilResize(base::BindLambdaForTesting(
      [&swap_callback_run]() { swap_callback_run = true; }));
  EXPECT_TRUE(scheduler_->swapped);
  EXPECT_TRUE(swap_callback_run);

  display_->Resize(gfx::Size(150, 150));
  scheduler_->swapped = false;

  // DisableSwapUntilResize() won't trigger a swap because there is no frame
  // of the correct size to draw.
  display_->SetLocalSurfaceId(local_surface_id2, 1.f);
  display_->DisableSwapUntilResize(base::OnceClosure());
  EXPECT_FALSE(scheduler_->swapped);
  display_->Resize(gfx::Size(200, 200));

  {
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id2);
  }

  // DrawAndSwap() should trigger a swap at current size.
  display_->DrawAndSwap();
  EXPECT_TRUE(scheduler_->swapped);
  scheduler_->swapped = false;

  // DisableSwapUntilResize() won't trigger another swap because we already
  // swapped a frame at the current size.
  display_->DisableSwapUntilResize(base::OnceClosure());
  EXPECT_FALSE(scheduler_->swapped);

  TearDownDisplay();
}

TEST_F(DisplayTest, BackdropFilterTest) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id1(6,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id1(kAnotherFrameSinkId, sub_local_surface_id1);
  auto sub_support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, /*is_root=*/false,
      /*needs_sync_points=*/true);

  // Create frame sink for another sub surface.
  const LocalSurfaceId sub_local_surface_id2(7,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id2(kAnotherFrameSinkId2, sub_local_surface_id2);
  auto sub_support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId2, /*is_root=*/false,
      /*needs_sync_points=*/true);

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

  uint64_t next_render_pass_id = 1;
  for (size_t frame_num = 1; frame_num <= 2; ++frame_num) {
    bool first_frame = frame_num == 1;
    scheduler_->ResetDamageForTest();
    {
      // Sub-surface with backdrop-filter.
      RenderPassList pass_list;
      auto bd_pass = RenderPass::Create();
      cc::FilterOperations backdrop_filters;
      backdrop_filters.Append(cc::FilterOperation::CreateBlurFilter(5.0));
      bd_pass->SetAll(
          next_render_pass_id++, sub_surface_rect, no_damage, gfx::Transform(),
          cc::FilterOperations(), backdrop_filters,
          gfx::RRectF(gfx::RectF(sub_surface_rect), 0),
          gfx::ColorSpace::CreateSRGB(), false, false, false, false);
      pass_list.push_back(std::move(bd_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support1->SubmitCompositorFrame(sub_local_surface_id1,
                                          std::move(frame));
    }

    {
      // Sub-surface with damage.
      RenderPassList pass_list;
      auto other_pass = RenderPass::Create();
      other_pass->output_rect = gfx::Rect(display_size);
      other_pass->damage_rect = damage_rect;
      other_pass->id = next_render_pass_id++;
      pass_list.push_back(std::move(other_pass));

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .Build();
      sub_support2->SubmitCompositorFrame(sub_local_surface_id2,
                                          std::move(frame));
    }

    {
      RenderPassList pass_list;
      auto pass = RenderPass::Create();
      pass->output_rect = gfx::Rect(display_size);
      pass->damage_rect = damage_rect;
      pass->id = next_render_pass_id++;

      // Embed sub surface 1, with backdrop filter.
      auto* shared_quad_state1 = pass->CreateAndAppendSharedQuadState();
      shared_quad_state1->SetAll(
          gfx::Transform(), /*quad_layer_rect=*/sub_surface_rect,
          /*visible_quad_layer_rect=*/sub_surface_rect,
          /*rounded_corner_bounds=*/gfx::RRectF(),
          /*clip_rect=*/sub_surface_rect, /*is_clipped=*/false,
          /*are_contents_opaque=*/true, /*opacity=*/1.0f, SkBlendMode::kSrcOver,
          /*sorting_context_id=*/0);
      auto* quad1 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad1->SetNew(shared_quad_state1, /*rect=*/sub_surface_rect,
                    /*visible_rect=*/sub_surface_rect,
                    SurfaceRange(base::nullopt, sub_surface_id1), SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*has_pointer_events_none=*/false);
      quad1->allow_merge = false;

      // Embed sub surface 2, with damage.
      auto* shared_quad_state2 = pass->CreateAndAppendSharedQuadState();
      gfx::Rect rect1(display_size);
      shared_quad_state2->SetAll(gfx::Transform(), /*quad_layer_rect=*/rect1,
                                 /*visible_quad_layer_rect=*/rect1,
                                 /*rounded_corner_bounds=*/gfx::RRectF(),
                                 /*clip_rect=*/rect1, /*is_clipped=*/false,
                                 /*are_contents_opaque=*/true, /*opacity=*/1.0f,
                                 SkBlendMode::kSrcOver,
                                 /*sorting_context_id=*/0);
      auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
      quad2->SetNew(shared_quad_state2, /*rect=*/rect1,
                    /*visible_rect=*/rect1,
                    SurfaceRange(base::nullopt, sub_surface_id2), SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*has_pointer_events_none=*/false);
      quad2->allow_merge = false;

      pass_list.push_back(std::move(pass));
      SubmitCompositorFrame(&pass_list, local_surface_id);

      scheduler_->swapped = false;
      display_->DrawAndSwap();
      EXPECT_TRUE(scheduler_->swapped);
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
  TearDownDisplay();
}

class CountLossDisplayClient : public StubDisplayClient {
 public:
  CountLossDisplayClient() = default;

  void DisplayOutputSurfaceLost() override { ++loss_count_; }

  int loss_count() const { return loss_count_; }

 private:
  int loss_count_ = 0;
};

TEST_F(DisplayTest, ContextLossInformsClient) {
  SetUpGpuDisplay(RendererSettings());

  CountLossDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_EQ(0, client.loss_count());
  output_surface_->context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface_->context_provider()->ContextGL()->Flush();
  EXPECT_EQ(1, client.loss_count());
  TearDownDisplay();
}

// Regression test for https://crbug.com/727162: Submitting a CompositorFrame to
// a surface should only cause damage on the Display the surface belongs to.
// There should not be a side-effect on other Displays.
TEST_F(DisplayTest, CompositorFrameDamagesCorrectDisplay) {
  RendererSettings settings;
  id_allocator_.GenerateId();
  LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Set up second frame sink + display.
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kAnotherFrameSinkId, true /* is_root */,
      true /* needs_sync_points */);
  auto begin_frame_source2 = std::make_unique<StubBeginFrameSource>();
  auto scheduler_for_display2 = std::make_unique<TestDisplayScheduler>(
      begin_frame_source2.get(), task_runner_.get());
  TestDisplayScheduler* scheduler2 = scheduler_for_display2.get();
  auto display2 = CreateDisplay(
      settings, kAnotherFrameSinkId, std::move(scheduler_for_display2),
      FakeOutputSurface::CreateSoftware(
          std::make_unique<TestSoftwareOutputDevice>()));
  manager_.RegisterBeginFrameSource(begin_frame_source2.get(),
                                    kAnotherFrameSinkId);
  StubDisplayClient client2;
  display2->Initialize(&client2, manager_.surface_manager());
  display2->SetLocalSurfaceId(local_surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));
  display2->Resize(gfx::Size(100, 100));

  scheduler_->ResetDamageForTest();
  scheduler2->ResetDamageForTest();
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler2->damaged);

  // Submit a frame for display_ with full damage.
  RenderPassList pass_list;
  auto pass = RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = 1;
  pass_list.push_back(std::move(pass));

  SubmitCompositorFrame(&pass_list, local_surface_id);

  // Should have damaged only display_ but not display2.
  EXPECT_TRUE(scheduler_->damaged);
  EXPECT_FALSE(scheduler2->damaged);
  manager_.UnregisterBeginFrameSource(begin_frame_source2.get());
  TearDownDisplay();
}

// Check if draw occlusion does not remove any DrawQuads when no quad is being
// covered completely.
TEST_F(DisplayTest, DrawOcclusionWithNonCoveringDrawQuad) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(25, 25, 50, 100);
  gfx::Rect rect4(150, 0, 50, 50);
  gfx::Rect rect5(0, 0, 120, 120);
  gfx::Rect rect6(25, 0, 50, 160);
  gfx::Rect rect7(0, 20, 100, 100);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect2 (50, 50, 100x100)), the |quad_list| size remains the
    // same after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect2 - rect1 U rect2 = (100, 50, 50x50 U 50, 100, 100x50),
    // which cannot be represented by a smaller rect (its visible_rect stays
    // the same).
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  // +------+                                +------+
  // |      |                                |      |
  // | +--+ |          show on screen        |      |
  // +------+                =>              +------+
  //   |  |                                    |  |
  //   +--+                                    +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect3 (25, 25, 50x100)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect3 - rect1 U rect3 = (25, 100, 50x25), which updates its
    // visible_rect accordingly.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(25, 100, 50, 25).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(1)
                  ->visible_rect.ToString());
  }

  //  +--+                                        +--+
  // +----+                                      +----+
  // ||  ||             shown on screen          |    |
  // +----+                                      +----+
  //  +--+                                        +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect7, rect7, gfx::RRectF(),
                              rect7, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect6, rect6, gfx::RRectF(),
                               rect6, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect7, rect7, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect7 (0, 20, 100x100)) cannot cover |quad2|
    // (define by rect6 (25, 0, 50x160)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect6 - rect7 = (25, 0, 50x20 U 25, 120, 50x40), which
    // cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect7.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect6.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  // +----+   +--+
  // |    |   +--+
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4, gfx::RRectF(),
                               rect4, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect4 (150, 0, 50x50)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect4 (150, 0, 50x50), its visible_rect stays the same.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect4.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect5, rect5, gfx::RRectF(),
                               rect5, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect5 (0, 0, 120x120)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect5 - rect1 = (100, 0, 20x100 U 0, 100, 100x20),
    // which cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  TearDownDisplay();
}

// Check if draw occlusion removes DrawQuads that are not shown on screen.
TEST_F(DisplayTest, CompositorFrameWithOverlapDrawQuad) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 50, 50);
  gfx::Rect rect3(50, 50, 50, 25);
  gfx::Rect rect4(0, 0, 50, 50);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| overlaps |quad1|, so |quad2| is removed from the |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  //  +-----+
  //  | +-+ |
  //  | +-+ |
  //  +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is hiding behind |quad1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  // +-----+
  // |  +--|
  // |  +--|
  // +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is behind |quad1| and aligns with the edge of |quad1|, so |quad2|
    // is removed from the |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4, gfx::RRectF(),
                               rect4, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is covered by |quad 1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion is not applied on DrawQuads that are smaller than
// skip_rect size, such that DrawQuads that are smaller than the |skip_rect|
// are drawn on the screen regardless is shown or not.
TEST_F(DisplayTest, DrawOcclusionWithSkipRect) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect more_then_minimum_size(
      RendererSettings().kMinimumDrawOcclusionSize);
  more_then_minimum_size.set_width(more_then_minimum_size.width() + 1);

  gfx::Rect minimum_size(RendererSettings().kMinimumDrawOcclusionSize);

  gfx::Rect less_than_minimum_size(
      RendererSettings().kMinimumDrawOcclusionSize);
  less_than_minimum_size.set_width(more_then_minimum_size.width() - 1);
  less_than_minimum_size.set_height(more_then_minimum_size.height() - 1);

  gfx::Rect rect(0, 0, 100, 100);

  bool is_clipped = false;
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
  // A small rect is hiding behind the bigger rect (|rect|), same picture for
  // the following 3 tests.
  // rects structure:         show on screen:
  // +----+---+               +--------+
  // |    |   |               |        |
  // |----+   |               |        |
  // |        |               |        |
  // +--------+               +--------+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(
        gfx::Transform(), more_then_minimum_size, more_then_minimum_size,
        gfx::RRectF(), more_then_minimum_size, is_clipped, are_contents_opaque,
        opacity, SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, more_then_minimum_size,
                  more_then_minimum_size, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // |more_then_minimum_size| rect is not shown on screen. Since its size is
    // slightly larger than the skip_rect size, draw occlusion is applied on
    // |more_then_minimum_size| and it's removed from the compositor frame.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), minimum_size, minimum_size,
                               gfx::RRectF(), minimum_size, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, minimum_size, minimum_size, SK_ColorBLACK,
                  false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // |minimum_size| rect is not shown on screen. Since its size is the same
    // as skip_rect size, draw occlusion is not applied on this rect.  So it is
    // not removed from compositor frame.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(minimum_size.ToString(), frame.render_pass_list.front()
                                           ->quad_list.ElementAt(1)
                                           ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(
        gfx::Transform(), less_than_minimum_size, less_than_minimum_size,
        gfx::RRectF(), less_than_minimum_size, is_clipped, are_contents_opaque,
        opacity, SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, less_than_minimum_size,
                  less_than_minimum_size, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // |less_than_minimum_size| rect is not shown on screen. Since its size is
    // less than skip_rect size, draw occlusion is not applied on this rect.
    // So it is not removed from compositor frame.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(less_than_minimum_size.ToString(), frame.render_pass_list.front()
                                                     ->quad_list.ElementAt(1)
                                                     ->visible_rect.ToString());
  }

  TearDownDisplay();
}

// Check if draw occlusion is not applied on DrawQuads that are smaller than
// skip_rect size, such that DrawQuads that are smaller than the |skip_rect|
// cannot occlude other quads behind it.
TEST_F(DisplayTest, OcclusionIgnoringSkipRect) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 50, 50);
  gfx::Rect rect2(50, 0, 50, 50);
  gfx::Rect rect3(0, 0, 50, 90);

  bool is_clipped = false;
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
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                            rect1, is_clipped, are_contents_opaque, opacity,
                            SkBlendMode::kSrcOver, 0);
  shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                             rect2, is_clipped, are_contents_opaque, opacity,
                             SkBlendMode::kSrcOver, 0);
  shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                             rect3, is_clipped, are_contents_opaque, opacity,
                             SkBlendMode::kSrcOver, 0);
  quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
  quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
  quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);

  EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
  display_->RemoveOverdrawQuads(&frame);

  // |quad3| is not shown on screen because is hiding behind the occlusion rect
  // formed by |quad1| and |quad2|. Since the |visible_rect| in both |quad1|
  // and |quad2| are smaller than the skip rect, they cannot be used to occlude
  // |quad3|. So no draw quad is removed in compositor frame by draw occlusion.
  EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
  EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                  ->quad_list.ElementAt(0)
                                  ->visible_rect.ToString());
  EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                  ->quad_list.ElementAt(1)
                                  ->visible_rect.ToString());
  EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                  ->quad_list.ElementAt(2)
                                  ->visible_rect.ToString());
  TearDownDisplay();
}
// Check if draw occlusion works well with scale change transformer.
TEST_F(DisplayTest, CompositorFrameWithTransformer) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // Rect 2, 3, 4 are contained in rect 1 only after applying the half scale
  // matrix. They are repetition of CompositorFrameWithOverlapDrawQuad.
  CompositorFrame frame = MakeDefaultCompositorFrame();
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
  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(half_scale, rect2, rect2, gfx::RRectF(), rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| becomes (12, 12, 50x50) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(half_scale, rect3, rect3, gfx::RRectF(), rect3,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect3| becomes (25, 25, 50x25) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(half_scale, rect4, rect4, gfx::RRectF(), rect4,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect4| becomes (0, 0, 60x60) after applying half scale transform,
    // |quad2| is now covered by |quad1|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(double_scale, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The compositor frame contains only one quad, so |quad_list| remains 1
    // after calling RemoveOverdrawQuads.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect5, rect5, gfx::RRectF(), rect5,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect5|) becomes (50, 50, 120x120) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect6, rect6, gfx::RRectF(), rect6,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect6|) becomes (24, 24, 50x100) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is (12, 50, 25x12), quad2::visible_rect
    // updates accordingly.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(12, 50, 25, 12).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(1)
                  ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect7, rect7, gfx::RRectF(), rect7,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect7, rect7, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect7|) becomes (150, 0, 50x50) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect7| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect7.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect8, rect8, gfx::RRectF(), rect8,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect8, rect8, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect8|) becomes (0, 0, 120x120) after
    // applying double scale transform, it is not covered by |quad1| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect8| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect8.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(double_scale, rect10, rect10, gfx::RRectF(),
                              rect10, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect9, rect9, gfx::RRectF(), rect9,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect10, rect10, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect9, rect9, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| (defined by |rect9|) becomes (24, 0, 50x160) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect10| (0, 20, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect9| is not a rect, quad2::visible_rect stays
    // the same
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect10.ToString(), frame.render_pass_list.front()
                                     ->quad_list.ElementAt(0)
                                     ->visible_rect.ToString());
    EXPECT_EQ(rect9.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with transform at epsilon scale.
TEST_F(DisplayTest, CompositorFrameWithEpsilonScaleTransform) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect(0, 0, 100, 100);

  SkMScalar epsilon = float(0.000000001);
  SkMScalar larger_than_epsilon = float(0.00000001);
  gfx::Transform zero_scale;
  zero_scale.Scale(0, 0);
  gfx::Transform epsilon_scale;
  epsilon_scale.Scale(epsilon, epsilon);
  gfx::Transform larger_epsilon_scale;
  larger_epsilon_scale.Scale(larger_than_epsilon, larger_than_epsilon);
  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(zero_scale, rect, rect, gfx::RRectF(), rect,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // zero matrix transform is non-invertible, so |quad2| is not removed from
    // draw occlusion algorithm.
    EXPECT_FALSE(zero_scale.GetInverse(&inverted));
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(1)
                                   ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(epsilon_scale, rect, rect, gfx::RRectF(), rect,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 1);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
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
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(1)
                                   ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(larger_epsilon_scale, rect, rect, gfx::RRectF(),
                               rect, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // This test verifies that the draw occlusion algorithm works well with
    // small scales that is just larger than the epsilon scale in the previous
    // case. |larger_epsilon_scale| transform has the scale set to 10^-7.
    // |quad2| will be transformed to a tiny rect that is covered by the
    // occlusion rect, so |quad2| is removed.
    EXPECT_TRUE(larger_epsilon_scale.GetInverse(&inverted));
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
  }

  TearDownDisplay();
}

// Check if draw occlusion works with transform at negative scale.
TEST_F(DisplayTest, CompositorFrameWithNegativeScaleTransform) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect(0, 0, 100, 100);

  gfx::Transform negative_scale;
  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(negative_scale, rect, rect, gfx::RRectF(), rect,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
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
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(1)
                                   ->visible_rect.ToString());
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, -1, 1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(negative_scale, rect, rect, gfx::RRectF(), rect,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
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
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(1)
                                   ->visible_rect.ToString());
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, 1, -1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(negative_scale, rect, rect, gfx::RRectF(), rect,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
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
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect.ToString(), frame.render_pass_list.front()
                                   ->quad_list.ElementAt(0)
                                   ->visible_rect.ToString());
  }

  TearDownDisplay();
}

// Check if draw occlusion works well with rotation transform.
//
//  +-----+                                  +----+
//  |     |   rotation (by 45 on y-axis) ->  |    |     same height
//  +-----+                                  +----+     reduced weight
TEST_F(DisplayTest, CompositorFrameWithRotation) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(75, 75, 10, 10);

  // rect 3 intersects with rect 1 initially
  gfx::Rect rect3(50, 50, 25, 100);

  gfx::Transform rotate;
  rotate.RotateAboutYAxis(45);
  bool is_clipped = false;
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
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (75, 75 10x10). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    // Apply rotation transform on |rect1| and |rect2|.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(rotate, rect2, rect2, gfx::RRectF(), rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 70x100) and |quad2| becomes
    // (53, 75 8x10) (after applying rotation transform). So |quad2| is behind
    // |quad|. |quad2| is removed from |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (50, 50, 25x100). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    // Since we only support updating |visible_rect| of DrawQuad with scale
    // or translation transform and rotation transform applies to quads,
    // |visible_rect| of |quad2| should not be changed.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(rotate, rect3, rect3, gfx::RRectF(), rect3,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since both |quad| and |quad2| went through the same transform and |rect1|
    // does not cover |rect3| initially, |quad| does not cover |quad2| in target
    // space.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion is handled correctly if the transform does not
// preserves 2d axis alignment.
TEST_F(DisplayTest, CompositorFrameWithPerspective) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 1, 1);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100);
  perspective.RotateAboutYAxis(45);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(perspective, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The transform used on |quad| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. Since it takes too
    // long to define a enclosed rect to describe the occlusion region,
    // occlusion region is not defined and no changes in quads.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(perspective, rect2, rect2, gfx::RRectF(), rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The transform used on |quad2| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. it's easy to find
    // an enclosing rect to describe |quad2|. |quad2| is hiding behind |quad|,
    // so it's removed from |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with transparent DrawQuads.
TEST_F(DisplayTest, CompositorFrameWithOpacityChange) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque,
                              opacityLess1, SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, are_contents_opaque, opacity1,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the opacity of |rect2| is less than 1, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity1,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, are_contents_opaque, opacity1,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opacity of |rect1| to 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithOpaquenessChange) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, transparent_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the opaqueness of |rect2| is false, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opaqueness of |rect2| to true.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  TearDownDisplay();
}

// Test if draw occlusion skips 3d objects. https://crbug.com/833748
TEST_F(DisplayTest, CompositorFrameZTranslate) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(0, 0, 200, 100);

  gfx::Transform translate_back;
  translate_back.Translate3d(0, 0, 100);
  bool is_clipped = false;
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
    shared_quad_state->SetAll(translate_back, rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 1);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 1);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since both |quad| and |quad2| are inside of a 3d object, DrawOcclusion
    // will not be applied to them.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithTranslateTransformer) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 and 3 are outside rect 1 initially.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(120, 120, 10, 10);
  gfx::Rect rect3(100, 100, 100, 20);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, transparent_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| and |rect1| are disjoined as show in the first image. The size of
    // |quad_list| remains 2.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    //   quad content space:                                      target space:
    //   +----+
    //   |    |               translation transform
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))         +-----+
    //   +----+                       =>                              | +-+ |
    //           +-+                                                  | +-+ |
    //           +-+                                                  +-----+
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Move |quad| defind by |rect1| over |quad2| defind by |rect2| by applying
    // translation transform. |quad2| will be covered by |quad|, so |quad_list|
    // size is reduced by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
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
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::RRectF(), rect1,
                              is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Move |quad| defind by |rect1| over |quad2| defind by |rect3| by applying
    // translation transform. In target space, |quad| is (50, 50, 100x100) and
    // |quad2| is (100, 100, 100x20). So the visible region of |quad2| is
    // (150, 100, 50x20).
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(150, 100, 50, 20).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(1)
                  ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithCombinedSharedQuadState) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);
  gfx::Rect rect3(10, 10, 120, 30);

  // rect 4 and 5 intersect with the combined rect of 1 and 2.
  gfx::Rect rect4(10, 10, 180, 30);
  gfx::Rect rect5(10, 10, 120, 100);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by both |rect1| and |rect2|, so |rect3| is
    // removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
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
    shared_quad_state3->SetAll(gfx::Transform(), rect4, rect4, gfx::RRectF(),
                               rect4, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad3->SetNew(shared_quad_state3, rect4, rect4, SK_ColorBLACK, false);
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 4 is
    // (160, 10, 30x30), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(160, 10, 30, 30).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(2)
                  ->visible_rect.ToString());
  }

  {
    //  rect1 & rect2                      rect 5 added
    //   +----+----+                       +----+----+
    //   |    |    |                       | +--|--+ |
    //   |    |----+           =>          | |  |--|-+
    //   +----+                            +-|--+  |
    //                                       +-----+
    shared_quad_state3->SetAll(gfx::Transform(), rect5, rect5, gfx::RRectF(),
                               rect5, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad3->SetNew(shared_quad_state3, rect5, rect5, SK_ColorBLACK, false);
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 5 is
    // (10, 60, 120x50), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(10, 60, 120, 50).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(2)
                  ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithMultipleRenderPass) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);

  std::unique_ptr<RenderPass> render_pass2 = RenderPass::Create();
  render_pass2->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));
  gfx::Rect rect3(10, 10, 120, 30);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by the unioned region of |rect1| and |rect2|.
    // But |rect3| so |rect3| is to be removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.at(1)
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.at(1)
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithCoveredRenderPass) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);

  std::unique_ptr<RenderPass> render_pass2 = RenderPass::Create();
  render_pass2->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  RenderPassId render_pass_id = 1;
  ResourceId mask_resource_id = 2;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();

  {
    // rect1 is a DrawQuad from SQS1 and which is also the RenderPass rect
    // from SQS2. The RenderPassDrawQuad should not be occluded.
    //  rect1
    //   +----+
    //   |    |
    //   |    |
    //   +----+
    //

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad1->SetNew(shared_quad_state2, rect1, rect1, render_pass_id,
                  mask_resource_id, gfx::RectF(), gfx::Size(),
                  gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                  1.0f);
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| and |rect2| shares the same region where |rect1| is a draw
    // quad and |rect2| RenderPass. |rect2| will be not removed from the
    // |quad_list|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.at(1)
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithClip) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);
  gfx::Rect clip_rect(0, 0, 60, 60);
  gfx::Rect rect3(50, 50, 20, 10);

  bool clipped = true;
  bool non_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, non_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, non_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| covers |rect2| as shown in the figure above, So the size of
    // |quad_list| is reduced by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              clip_rect, clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, non_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // In the target space, a clip is applied on |quad| (defined by |clip_rect|,
    // (0, 0, 60x60) |quad| and |quad2| (50, 50, 25x25) don't intersect in the
    // target space. So no change is applied to quads.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    //  rect1(non-clip) & rect2                rect1(clip) & rect3
    //   +------+                                     +---+
    //   |   +-+|                                     |  +|+
    //   |   +-+|             =>                      +--+++
    //   +------+
    //
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              clip_rect, clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, non_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // In the target space, a clip is applied on |quad| (defined by |rect3|,
    // (50, 50, 20x10)). |quad| intersects with |quad2| in the target space. The
    // visible region of |quad2| is (60, 50, 10x10). So |quad2| is updated
    // accordingly.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(60, 50, 10, 10).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(1)
                  ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with copy requests in root RenderPass only.
TEST_F(DisplayTest, CompositorFrameWithCopyRequest) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    frame.render_pass_list.front()->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // root RenderPass contains |rect1|, |rect2| and copy_request (where
    // |rect2| is in |rect1|). Since our current implementation only supports
    // occlusion with copy_request on root RenderPass, |quad_list| reduces its
    // size by 1 after calling remove overdraw.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithRenderPass) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 0, 100, 100);
  gfx::Rect rect3(0, 0, 25, 25);
  gfx::Rect rect4(100, 0, 25, 25);
  gfx::Rect rect5(0, 0, 50, 50);
  gfx::Rect rect6(0, 75, 25, 25);
  gfx::Rect rect7(0, 0, 10, 10);

  bool is_clipped = false;
  bool opaque_content = true;
  RenderPassId render_pass_id = 1;
  ResourceId mask_resource_id = 2;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R1 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R2 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                               rect2, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect4, rect4, gfx::RRectF(),
                               rect4, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect2, rect2, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect4, rect4, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since RenderPassDrawQuad |r1| and |r2| cannot be
    // removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect4.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(3)
                                    ->visible_rect.ToString());
  }

  {
    // RenderPass R2 is contained in R1, but the opaque region of the two
    // RenderPasses are separated.
    // +-------+-----------+
    // |_D2_|  |      |_D1_|
    // |       |           |
    // |   R2  |       R1  |
    // +-------+-----------+
    shared_quad_state->SetAll(gfx::Transform(), rect5, rect5, gfx::RRectF(),
                              rect5, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect6, rect6, gfx::RRectF(),
                               rect6, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect6, rect6, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since RenderPassDrawQuad |r1| and |r2| cannot be
    // removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect6.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(3)
                                    ->visible_rect.ToString());
  }

  {
    // RenderPass R2 is contained in R1, and opaque region of R2 in R1 as well.
    // +-+---------+-------+
    // |-+   |     |       |
    // |-----+     |       |
    // |   R2      |   R1  |
    // +-----------+-------+
    shared_quad_state->SetAll(gfx::Transform(), rect5, rect5, gfx::RRectF(),
                              rect5, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                               rect1, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect7, rect7, gfx::RRectF(),
                               rect7, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect7, rect7, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d2| is contained in |d1|
    // Since RenderPassDrawQuad |r1| and |r2| cannot be removed to reduce
    // overdraw, |quad_list| is reduced by 1.
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithMultipleDrawQuadInSharedQuadState) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
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

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect_in_rect1, rect_in_rect1,
                               gfx::RRectF(), rect_in_rect1, is_clipped,
                               opaque_content, opacity, SkBlendMode::kSrcOver,
                               0);
    quad1->SetNew(shared_quad_state, rect1_1, rect1_1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state, rect1_2, rect1_2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state, rect1_3, rect1_3, SK_ColorBLACK, false);
    quad4->SetNew(shared_quad_state, rect1_4, rect1_4, SK_ColorBLACK, false);
    quad5->SetNew(shared_quad_state2, rect_in_rect1, rect_in_rect1,
                  SK_ColorBLACK, false);
    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // covers the visible region of |shared_quad_state2|.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1_1.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(0)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_2.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(1)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_3.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(2)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_4.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(3)
                                      ->visible_rect.ToString());
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
    shared_quad_state2->SetAll(
        gfx::Transform(), rect_intersects_rect1, rect_intersects_rect1,
        gfx::RRectF(), rect_intersects_rect1, is_clipped, opaque_content,
        opacity, SkBlendMode::kSrcOver, 0);
    quad5->SetNew(shared_quad_state2, rect_intersects_rect1,
                  rect_intersects_rect1, SK_ColorBLACK, false);
    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. The
    // |visible_rect| of |quad5| is updated.
    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1_1.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(0)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_2.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(1)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_3.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(2)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect1_4.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(3)
                                      ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(100, 0, 30, 30).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(4)
                  ->visible_rect.ToString());
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
    shared_quad_state->SetAll(gfx::Transform(), rect2, rect2, gfx::RRectF(),
                              rect2, is_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad1->SetNew(shared_quad_state, rect2_1, rect2_1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state, rect2_2, rect2_2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state, rect2_3, rect2_3, SK_ColorBLACK, false);
    quad4->SetNew(shared_quad_state, rect2_4, rect2_4, SK_ColorBLACK, false);
    quad5->SetNew(shared_quad_state2, rect3_1, rect3_1, SK_ColorBLACK, false);
    quad6->SetNew(shared_quad_state2, rect3_2, rect3_2, SK_ColorBLACK, false);
    EXPECT_EQ(6u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. So the
    // |visible_rect| of DrawQuads in |share_quad_state2| are updated to the
    // region shown on screen.
    EXPECT_EQ(6u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect2_1.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(0)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect2_2.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(1)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect2_3.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(2)
                                      ->visible_rect.ToString());
    EXPECT_EQ(rect2_4.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(3)
                                      ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 20, 30).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(4)
                  ->visible_rect.ToString());
    EXPECT_EQ(gfx::Rect(120, 0, 20, 30).ToString(),
              frame.render_pass_list.front()
                  ->quad_list.ElementAt(5)
                  ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithNonInvertibleTransform) {
  RendererSettings settings;
  settings.kMinimumDrawOcclusionSize.set_width(0);
  SetUpGpuDisplay(settings);

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 50, 50);
  gfx::Rect rect3(0, 0, 10, 10);

  gfx::Transform invertible;
  gfx::Transform non_invertible(10, 10, 0, 0,  // row 1
                                10, 10, 0, 0,  // row 2
                                0, 0, 1, 0,    // row 3
                                0, 0, 0, 1);   // row 4
  gfx::Transform non_invertible_miss_z;
  non_invertible_miss_z.Scale3d(1, 1, 0);
  bool is_clipped = false;
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
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::RRectF(), rect1,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(invertible, rect2, rect2, gfx::RRectF(), rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(non_invertible, rect3, rect3, gfx::RRectF(),
                               rect3, is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad1->SetNew(shared_quad_state1, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);

    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is removed because it is not shown on screen in the target space.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->visible_rect.ToString());
  }

  {
    // in quad content space:     in target space:
    // +--------+                 +--------+
    // | |      |                 | |      |
    // |-+      |                 |-+      |
    // |        |                 |        |
    // +--------+                 +--------+
    // Verify if draw occlusion can occlude quad with non-invertible
    // transfrom.
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::RRectF(), rect1,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(non_invertible_miss_z, rect3, rect3,
                               gfx::RRectF(), rect3, is_clipped, opaque_content,
                               opacity, SkBlendMode::kSrcOver, 0);
    quad1->SetNew(shared_quad_state1, rect1, rect1, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad3| follows an non-invertible transform and it's covered by the
    // occlusion rect. So |quad3| is removed from the |frame|.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with very large DrawQuad. crbug.com/824528.
TEST_F(DisplayTest, DrawOcclusionWithLargeDrawQuad) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();
  // The size of this DrawQuad will be 237790x237790 > 2^32 (uint32_t.max())
  // which caused the integer overflow in the bug.
  gfx::Rect rect1(237790, 237790);

  bool is_clipped = false;
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
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, gfx::RRectF(),
                              rect1, is_clipped, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithPresentationToken) {
  RendererSettings settings;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id(6,
                                            base::UnguessableToken::Create());
  const SurfaceId sub_surface_id(kAnotherFrameSinkId, sub_local_surface_id);

  MockCompositorFrameSinkClient sub_client;

  auto sub_support = std::make_unique<CompositorFrameSinkSupport>(
      &sub_client, &manager_, kAnotherFrameSinkId, false /* is_root */,
      true /* needs_sync_points */);

  const gfx::Size display_size(100, 100);
  display_->Resize(display_size);
  const gfx::Size sub_surface_size(32, 32);

  uint32_t frame_token_1 = 0, frame_token_2 = 0;
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(sub_surface_size), gfx::Rect())
            .Build();
    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_)).Times(1);
    frame_token_1 = frame.metadata.frame_token;
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));
  }

  {
    // Submit a frame for display_ with full damage.
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(display_size);
    pass->damage_rect = gfx::Rect(display_size);
    pass->id = 1;

    auto* shared_quad_state1 = pass->CreateAndAppendSharedQuadState();
    gfx::Rect rect1(display_size);
    shared_quad_state1->SetAll(
        gfx::Transform(), rect1 /* quad_layer_rect */,
        rect1 /* visible_quad_layer_rect */,
        gfx::RRectF() /* rounded_corner_bounds*/, rect1 /*clip_rect */,
        false /* is_clipped */, false /* are_contents_opaque */,
        0.5f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
    auto* quad1 = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad1->SetNew(shared_quad_state1, rect1 /* rect */,
                  rect1 /* visible_rect */, SK_ColorBLACK,
                  false /* force_anti_aliasing_off */);

    auto* shared_quad_state2 = pass->CreateAndAppendSharedQuadState();
    gfx::Rect rect2(gfx::Point(20, 20), sub_surface_size);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2 /* quad_layer_rect */,
        rect2 /* visible_quad_layer_rect */,
        gfx::RRectF() /* rounded_corner_bounds */, rect2 /*clip_rect */,
        false /* is_clipped */, true /* are_contents_opaque */,
        1.0f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
    auto* quad2 = pass->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad2->SetNew(shared_quad_state2, rect2 /* rect */,
                  rect2 /* visible_rect */,
                  SurfaceRange(base::nullopt, sub_surface_id), SK_ColorBLACK,
                  false /* stretch_content_to_fill_bounds */,
                  false /* has_pointer_events_none */);

    pass_list.push_back(std::move(pass));
    SubmitCompositorFrame(&pass_list, local_surface_id);
    display_->DrawAndSwap();
    RunAllPendingInMessageLoop();
  }

  {
    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(gfx::Rect(sub_surface_size),
                                               gfx::Rect(sub_surface_size))
                                .Build();
    frame_token_2 = frame.metadata.frame_token;

    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_)).Times(1);
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));

    display_->DrawAndSwap();
    RunAllPendingInMessageLoop();

    // Both frames with frame-tokens 1 and 2 requested presentation-feedback.
    ASSERT_EQ(2u, sub_support->timing_details().size());
    EXPECT_EQ(sub_support->timing_details().count(frame_token_1), 1u);
    EXPECT_EQ(sub_support->timing_details().count(frame_token_2), 1u);
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(sub_surface_size), gfx::Rect())
            .Build();

    EXPECT_CALL(sub_client, DidReceiveCompositorFrameAck(_)).Times(1);
    sub_support->SubmitCompositorFrame(sub_local_surface_id, std::move(frame));

    display_->DrawAndSwap();
    RunAllPendingInMessageLoop();
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, BeginFrameThrottling) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
      1.f);
  support_->SetNeedsBeginFrame(true);

  // Helper fn to submit a CF.
  auto submit_frame = [this]() {
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
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
  display_->DrawAndSwap();
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
  frame_time += base::TimeDelta::FromSecondsD(1.1);
  EXPECT_TRUE(ShouldSendBeginFrame(support_.get(), frame_time));

  TearDownDisplay();
}

TEST_F(DisplayTest, BeginFrameThrottlingMultipleSurfaces) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
      1.f);
  support_->SetNeedsBeginFrame(true);

  // Helper fn to submit a CF.
  auto submit_frame = [this]() {
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(
        &pass_list,
        id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());
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
  display_->DrawAndSwap();
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
  display_->SetLocalSurfaceId(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
      1.f);
  display_->DrawAndSwap();
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

  TearDownDisplay();
}

TEST_F(DisplayTest, DontThrottleWhenParentBlocked) {
  id_allocator_.GenerateId();
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
      1.f);
  support_->SetNeedsBeginFrame(true);

  // Create frame sink for a sub surface.
  const LocalSurfaceId sub_local_surface_id(6,
                                            base::UnguessableToken::Create());
  const LocalSurfaceId sub_local_surface_id2(7,
                                             base::UnguessableToken::Create());
  const SurfaceId sub_surface_id2(kAnotherFrameSinkId, sub_local_surface_id2);

  MockCompositorFrameSinkClient sub_client;

  auto sub_support = std::make_unique<CompositorFrameSinkSupport>(
      &sub_client, &manager_, kAnotherFrameSinkId, false /* is_root */,
      true /* needs_sync_points */);
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
                                     base::TimeDelta::FromSeconds(1), false))
          .Build();
  support_->SubmitCompositorFrame(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id(),
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

  TearDownDisplay();
}

TEST_F(DisplayTest, InvalidPresentationTimestamps) {
  RendererSettings settings;
  id_allocator_.GenerateId();
  const LocalSurfaceId local_surface_id(
      id_allocator_.GetCurrentLocalSurfaceIdAllocation().local_surface_id());

  // Set up first display.
  SetUpSoftwareDisplay(settings);
  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);
  display_->Resize(gfx::Size(25, 25));

  {
    // A regular presentation timestamp.
    base::HistogramTester histograms;
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(25, 25), gfx::Rect(25, 25))
            .Build();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    display_->DrawAndSwap();
    display_->DidReceiveSwapBuffersAck(GetTestSwapTimings());
    display_->DidReceivePresentationFeedback({base::TimeTicks::Now(), {}, 0});
    EXPECT_THAT(histograms.GetAllSamples(
                    "Graphics.PresentationTimestamp.InvalidBeforeSwap"),
                testing::IsEmpty());
    EXPECT_THAT(histograms.GetAllSamples(
                    "Graphics.PresentationTimestamp.InvalidFromFuture"),
                testing::IsEmpty());
  }

  {
    // A presentation-timestamp that is earlier than the swap time.
    base::HistogramTester histograms;
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(25, 25), gfx::Rect(25, 25))
            .Build();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    display_->DrawAndSwap();
    display_->DidReceiveSwapBuffersAck(GetTestSwapTimings());
    display_->DidReceivePresentationFeedback(
        {base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1), {}, 0});
    EXPECT_THAT(histograms.GetAllSamples(
                    "Graphics.PresentationTimestamp.InvalidFromFuture"),
                testing::IsEmpty());
    auto buckets = histograms.GetAllSamples(
        "Graphics.PresentationTimestamp.InvalidBeforeSwap");
    ASSERT_EQ(buckets.size(), 1u);
    EXPECT_GT(buckets[0].min, 0);
    EXPECT_LE(buckets[0].min, 1000);
    EXPECT_EQ(buckets[0].count, 1);
  }

  {
    // A presentation-timestamp that is in the future.
    base::HistogramTester histograms;
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(25, 25), gfx::Rect(25, 25))
            .Build();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    display_->DrawAndSwap();
    display_->DidReceiveSwapBuffersAck(GetTestSwapTimings());
    display_->DidReceivePresentationFeedback(
        {base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1), {}, 0});
    EXPECT_THAT(histograms.GetAllSamples(
                    "Graphics.PresentationTimestamp.InvalidBeforeSwap"),
                testing::IsEmpty());

    auto buckets = histograms.GetAllSamples(
        "Graphics.PresentationTimestamp.InvalidFromFuture");
    ASSERT_EQ(buckets.size(), 1u);
    EXPECT_GT(buckets[0].min, 0);
    EXPECT_LE(buckets[0].min, 1000);
    EXPECT_EQ(buckets[0].count, 1);
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerDoesNotOcclude) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = MakeDefaultCompositorFrame();

  // The quad with rounded corner does not completely cover the quad below it.
  // The corners of the below quad are visiblg through the clipped corners.
  gfx::Rect quad_rect(10, 10, 100, 100);
  gfx::RRectF rounded_corner_bounds(gfx::RectF(quad_rect), 10.f);

  bool is_clipped = false;
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
        gfx::Transform(), quad_rect, quad_rect, gfx::RRectF(), quad_rect,
        is_clipped, are_contents_opaque, opacity, SkBlendMode::kSrcOver, 0);
    occluded_quad->SetNew(shared_quad_state_occluded, quad_rect, quad_rect,
                          SK_ColorRED, false);

    shared_quad_state_with_rrect->SetAll(gfx::Transform(), quad_rect, quad_rect,
                                         rounded_corner_bounds, quad_rect,
                                         is_clipped, are_contents_opaque,
                                         opacity, SkBlendMode::kSrcOver, 0);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SK_ColorBLUE, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);

    // Since none of the quads are culled, there should be 2 quads.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(quad_rect.ToString(), frame.render_pass_list.front()
                                        ->quad_list.ElementAt(0)
                                        ->visible_rect.ToString());
    EXPECT_EQ(quad_rect.ToString(), frame.render_pass_list.front()
                                        ->quad_list.ElementAt(1)
                                        ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerDoesOcclude) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // The quad with rounded corner completely covers the quad below it.
  CompositorFrame frame = MakeDefaultCompositorFrame();
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::RRectF rounded_corner_bounds(gfx::RectF(quad_rect), 10.f);
  gfx::Rect occluded_quad_rect(13, 13, 994, 994);

  bool is_clipped = false;
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
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect, gfx::RRectF(),
        occluded_quad_rect, is_clipped, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, 0);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SK_ColorRED, false);

    shared_quad_state_with_rrect->SetAll(gfx::Transform(), quad_rect, quad_rect,
                                         rounded_corner_bounds, quad_rect,
                                         is_clipped, are_contents_opaque,
                                         opacity, SkBlendMode::kSrcOver, 0);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SK_ColorBLUE, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(quad_rect.ToString(), frame.render_pass_list.front()
                                        ->quad_list.ElementAt(0)
                                        ->visible_rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, DrawOcclusionWithRoundedCornerPartialOcclude) {
  SetUpGpuDisplay(RendererSettings());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // The quad with rounded corner completely covers the quad below it.
  CompositorFrame frame = MakeDefaultCompositorFrame();

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
  // * -> Visiblg rect for the quads.
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::RRectF rounded_corner_bounds(gfx::RectF(quad_rect), 10.f);
  gfx::Rect occluded_quad_rect_1(0, 20, 600, 490);
  gfx::Rect occluded_quad_rect_2(600, 20, 600, 490);
  gfx::Rect occluded_quad_rect_3(0, 510, 600, 490);
  gfx::Rect occluded_quad_rect_4(600, 510, 600, 490);
  gfx::Rect occluded_sqs_rect;
  occluded_sqs_rect.Union(occluded_quad_rect_1);
  occluded_sqs_rect.Union(occluded_quad_rect_2);
  occluded_sqs_rect.Union(occluded_quad_rect_3);
  occluded_sqs_rect.Union(occluded_quad_rect_4);

  bool is_clipped = false;
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
        gfx::Transform(), occluded_sqs_rect, occluded_sqs_rect, gfx::RRectF(),
        occluded_sqs_rect, is_clipped, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, 0);
    occluded_quad_1->SetNew(shared_quad_state_occluded, occluded_quad_rect_1,
                            occluded_quad_rect_1, SK_ColorRED, false);
    occluded_quad_2->SetNew(shared_quad_state_occluded, occluded_quad_rect_2,
                            occluded_quad_rect_2, SK_ColorRED, false);
    occluded_quad_3->SetNew(shared_quad_state_occluded, occluded_quad_rect_3,
                            occluded_quad_rect_3, SK_ColorRED, false);
    occluded_quad_4->SetNew(shared_quad_state_occluded, occluded_quad_rect_4,
                            occluded_quad_rect_4, SK_ColorRED, false);

    shared_quad_state_with_rrect->SetAll(gfx::Transform(), quad_rect, quad_rect,
                                         rounded_corner_bounds, quad_rect,
                                         is_clipped, are_contents_opaque,
                                         opacity, SkBlendMode::kSrcOver, 0);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SK_ColorBLUE, false);

    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(quad_rect.ToString(), frame.render_pass_list.front()
                                        ->quad_list.ElementAt(0)
                                        ->visible_rect.ToString());

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
  TearDownDisplay();
}

}  // namespace viz
