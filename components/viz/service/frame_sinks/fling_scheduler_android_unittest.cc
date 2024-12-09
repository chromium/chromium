// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/fling_scheduler_android.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/input/features.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/input/input_manager.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_output_surface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {
constexpr FrameSinkId kFrameSinkIdA(1, 1);
}

class FakeInputManager : public InputManager {
 public:
  explicit FakeInputManager(FrameSinkManagerImpl* frame_sink_manager)
      : InputManager(frame_sink_manager) {}

  FakeInputManager(const FakeInputManager&) = delete;
  FakeInputManager& operator=(const FakeInputManager&) = delete;

  ~FakeInputManager() override = default;

  BeginFrameSource* GetBeginFrameSourceForFrameSink(
      const FrameSinkId& id) override {
    return &begin_frame_source_;
  }

 private:
  StubBeginFrameSource begin_frame_source_;
};

class FlingSchedulerTest : public testing::Test,
                           public input::FlingControllerEventSenderClient {
 public:
  FlingSchedulerTest()
      : frame_sink_manager_(
            FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_,
                                             &output_surface_provider_)) {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {input::features::kInputOnViz},
        /* disabled_features */ {});
  }

  FlingSchedulerTest(const FlingSchedulerTest&) = delete;
  FlingSchedulerTest& operator=(const FlingSchedulerTest&) = delete;

  void SetUp() override {
    frame_sink_manager_.SetInputManagerForTesting(
        std::make_unique<FakeInputManager>(&frame_sink_manager_));

    // Create a CompositorFrameSinkImpl.
    frame_sink_manager_.RegisterFrameSinkId(kFrameSinkIdA,
                                            true /* report_activation */);
    CreateCompositorFrameSink(kFrameSinkIdA,
                              CreateRIRConfig(/*grouping_id=*/1));
  }

  void TearDown() override {
    frame_sink_manager_.InvalidateFrameSinkId(kFrameSinkIdA);
    fling_controller_.reset();
  }

  void SetupFlingController() {
    auto* rir = GetFakeInputManager()->GetRenderInputRouterFromFrameSinkId(
        kFrameSinkIdA);
    fling_controller_ = std::make_unique<input::FlingController>(
        this, rir->GetFlingSchedulerForTesting(),
        input::FlingController::Config());
  }

  void SimulateFlingStart(const gfx::Vector2dF& velocity) {
    blink::WebGestureEvent fling_start(
        blink::WebInputEvent::Type::kGestureFlingStart, 0,
        base::TimeTicks::Now(), blink::WebGestureDevice::kTouchscreen);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    input::GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_start_with_latency);
  }

  void SimulateFlingCancel() {
    blink::WebGestureEvent fling_cancel(
        blink::WebInputEvent::Type::kGestureFlingCancel, 0,
        base::TimeTicks::Now(), blink::WebGestureDevice::kTouchscreen);
    fling_cancel.data.fling_cancel.prevent_boosting = true;
    input::GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_cancel_with_latency);
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const input::MouseWheelEventWithLatencyInfo& wheel_event) override {}
  void SendGeneratedGestureScrollEvents(
      const input::GestureEventWithLatencyInfo& gesture_event) override {}
  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  input::FlingController* fling_controller() { return fling_controller_.get(); }
  FlingSchedulerAndroid* fling_scheduler() {
    auto* rir = GetFakeInputManager()->GetRenderInputRouterFromFrameSinkId(
        kFrameSinkIdA);
    return static_cast<FlingSchedulerAndroid*>(
        rir->GetFlingSchedulerForTesting());
  }

  bool InputManagerExists() { return GetFakeInputManager(); }
  bool ExpectedInputManagerCreation() {
    return input::IsTransferInputToVizSupported();
  }

  FakeInputManager* GetFakeInputManager() {
    return static_cast<FakeInputManager*>(
        frame_sink_manager_.GetInputManager());
  }

 private:
  input::mojom::RenderInputRouterConfigPtr CreateRIRConfig(int grouping_id) {
    auto config = input::mojom::RenderInputRouterConfig::New();
    mojo::PendingRemote<blink::mojom::RenderInputRouterClient> rir_client;
    config->rir_client = std::move(rir_client);
    config->grouping_id = grouping_id;
    return config;
  }

  // Checks if a [Root]CompositorFrameSinkImpl exists for |frame_sink_id|.
  bool CompositorFrameSinkExists(const FrameSinkId& frame_sink_id) {
    return base::Contains(frame_sink_manager_.sink_map_, frame_sink_id) ||
           base::Contains(frame_sink_manager_.root_sink_map_, frame_sink_id);
  }

  // Creates a CompositorFrameSinkImpl.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      input::mojom::RenderInputRouterConfigPtr config) {
    MockCompositorFrameSinkClient compositor_frame_sink_client;
    mojo::Remote<mojom::CompositorFrameSink> compositor_frame_sink;

    frame_sink_manager_.CreateCompositorFrameSink(
        frame_sink_id, /*bundle_id=*/std::nullopt,
        compositor_frame_sink.BindNewPipeAndPassReceiver(),
        compositor_frame_sink_client.BindInterfaceRemote(), std::move(config));
    EXPECT_TRUE(CompositorFrameSinkExists(frame_sink_id));
  }

  std::unique_ptr<input::FlingController> fling_controller_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  TestOutputSurfaceProvider output_surface_provider_;
  FrameSinkManagerImpl frame_sink_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FlingSchedulerTest, ScheduleNextFlingProgress) {
  EXPECT_EQ(InputManagerExists(), ExpectedInputManagerCreation());
  if (!InputManagerExists()) {
    // FlingSchedulerAndroid's implementation depends on InputManager creation.
    // Exit if InputManager is not present.
    return;
  }

  SetupFlingController();
  ASSERT_EQ(
      fling_scheduler()->GetBeginFrameSource(),
      GetFakeInputManager()->GetBeginFrameSourceForFrameSink(kFrameSinkIdA));

  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller_.get());
  EXPECT_EQ(fling_scheduler()->GetBeginFrameSource(),
            fling_scheduler()->observed_begin_frame_source_);
}

TEST_F(FlingSchedulerTest, FlingCancelled) {
  EXPECT_EQ(InputManagerExists(), ExpectedInputManagerCreation());
  if (!InputManagerExists()) {
    // FlingSchedulerAndroid's implementation depends on InputManager creation.
    // Exit if InputManager is not present.
    return;
  }

  SetupFlingController();
  ASSERT_EQ(
      fling_scheduler()->GetBeginFrameSource(),
      GetFakeInputManager()->GetBeginFrameSourceForFrameSink(kFrameSinkIdA));

  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller_.get());
  EXPECT_EQ(fling_scheduler()->GetBeginFrameSource(),
            fling_scheduler()->observed_begin_frame_source_);

  SimulateFlingCancel();
  EXPECT_EQ(nullptr, fling_scheduler()->fling_controller_.get());
  EXPECT_EQ(nullptr, fling_scheduler()->observed_begin_frame_source_);
}

}  // namespace viz
