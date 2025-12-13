// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/fling_scheduler_android.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/input/features.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/mock_gpu_service_impl.h"
#include "components/viz/service/input/input_manager.h"
#include "components/viz/service/input/mock_input_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/mock_display_client.h"
#include "components/viz/test/test_output_surface_provider.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {
constexpr FrameSinkId kFrameSinkIdRoot(1, 1);
constexpr FrameSinkId kFrameSinkIdA(2, 1);

// Holds the four interface objects needed to create a RootCompositorFrameSink.
struct RootCompositorFrameSinkData {
  mojom::RootCompositorFrameSinkParamsPtr BuildParams(
      const FrameSinkId& frame_sink_id) {
    auto params = mojom::RootCompositorFrameSinkParams::New();
    params->frame_sink_id = frame_sink_id;
    params->widget = gpu::kNullSurfaceHandle;
    params->compositor_frame_sink =
        compositor_frame_sink.BindNewEndpointAndPassReceiver();
    params->compositor_frame_sink_client =
        compositor_frame_sink_client.BindInterfaceRemote();
    params->display_private = display_private.BindNewEndpointAndPassReceiver();
    params->display_client = display_client.BindRemote();
    return params;
  }

  mojo::AssociatedRemote<mojom::CompositorFrameSink> compositor_frame_sink;
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojo::AssociatedRemote<mojom::DisplayPrivate> display_private;
  MockDisplayClient display_client;
};

}  // namespace

class FlingSchedulerTest : public testing::Test,
                           public input::FlingControllerEventSenderClient {
 public:
  FlingSchedulerTest() {
    FrameSinkManagerImpl::InitParams init_params(&output_surface_provider_);
    init_params.gpu_service = &gpu_service_;
    frame_sink_manager_ =
        std::make_unique<FrameSinkManagerImpl>(std::move(init_params));
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {input::features::kInputOnViz},
        /* disabled_features */ {});
  }

  FlingSchedulerTest(const FlingSchedulerTest&) = delete;
  FlingSchedulerTest& operator=(const FlingSchedulerTest&) = delete;

  void SetUp() override {
    frame_sink_manager_->SetInputManagerForTesting(
        std::make_unique<MockInputManager>(frame_sink_manager_.get()));

    RootCompositorFrameSinkData root_data1;
    frame_sink_manager_->CreateRootCompositorFrameSink(
        root_data1.BuildParams(kFrameSinkIdRoot));
    EXPECT_TRUE(CompositorFrameSinkExists(kFrameSinkIdRoot));

    // Create a grouping id.
    base::UnguessableToken grouping_id = base::UnguessableToken::Create();
    // Create a CompositorFrameSinkImpl.
    frame_sink_manager_->RegisterFrameSinkId(kFrameSinkIdA,
                                             true /* report_activation */);
    CreateCompositorFrameSink(kFrameSinkIdA, CreateRIRConfig(grouping_id));

    // Set up initial hierarchy: root -> A.
    frame_sink_manager_->RegisterFrameSinkHierarchy(kFrameSinkIdRoot,
                                                    kFrameSinkIdA);
    ASSERT_EQ(frame_sink_manager_->GetOldestParentByChildFrameId(kFrameSinkIdA),
              kFrameSinkIdRoot);
  }

  void TearDown() override {
    // Cleanup hierarchy.
    frame_sink_manager_->UnregisterFrameSinkHierarchy(kFrameSinkIdRoot,
                                                      kFrameSinkIdA);
    frame_sink_manager_->InvalidateFrameSinkId(kFrameSinkIdRoot, {});
    // Invalidating should destroy the CompositorFrameSinkImpl's.
    frame_sink_manager_->InvalidateFrameSinkId(kFrameSinkIdA, {});

    fling_controller_.reset();
    // Make sure that all FrameSinkSourceMappings have been deleted.
    EXPECT_TRUE(frame_sink_manager_->frame_sink_source_map_.empty());
    // Make sure test cleans up all [Root]CompositorFrameSinkImpls.
    EXPECT_TRUE(frame_sink_manager_->support_map_.empty());
    // Make sure test has invalidated all registered FrameSinkIds.
    EXPECT_TRUE(frame_sink_manager_->frame_sink_data_.empty());
  }

  void SetupFlingController() {
    auto* rir =
        GetInputManager()->GetRenderInputRouterFromFrameSinkId(kFrameSinkIdA);
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
    auto* rir =
        GetInputManager()->GetRenderInputRouterFromFrameSinkId(kFrameSinkIdA);
    return static_cast<FlingSchedulerAndroid*>(
        rir->GetFlingSchedulerForTesting());
  }

  bool InputManagerExists() { return GetInputManager(); }
  bool ExpectedInputManagerCreation() {
    return input::InputUtils::IsTransferInputToVizSupported();
  }

  InputManager* GetInputManager() {
    return frame_sink_manager_->GetInputManager();
  }

  const BeginFrameSource* GetRootBeginFrameSource() {
    auto* support = frame_sink_manager_->GetFrameSinkForId(kFrameSinkIdRoot);
    return support->begin_frame_source();
  }

  void InvalidateRootFrameSinkId() {
    frame_sink_manager_->InvalidateFrameSinkId(kFrameSinkIdRoot, {});
  }

 private:
  input::mojom::RenderInputRouterConfigPtr CreateRIRConfig(
      const base::UnguessableToken& grouping_id) {
    auto config = input::mojom::RenderInputRouterConfig::New();
    mojo::PendingReceiver<blink::mojom::RenderInputRouterClient>
        rir_client_receiver;
    config->rir_client = rir_client_receiver.InitWithNewPipeAndPassRemote();
    config->grouping_id = grouping_id;
    return config;
  }

  // Checks if a [Root]CompositorFrameSinkImpl exists for |frame_sink_id|.
  bool CompositorFrameSinkExists(const FrameSinkId& frame_sink_id) {
    return base::Contains(frame_sink_manager_->sink_map_, frame_sink_id) ||
           base::Contains(frame_sink_manager_->root_sink_map_, frame_sink_id);
  }

  // Creates a CompositorFrameSinkImpl.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      input::mojom::RenderInputRouterConfigPtr config) {
    MockCompositorFrameSinkClient compositor_frame_sink_client;
    mojo::Remote<mojom::CompositorFrameSink> compositor_frame_sink;

    frame_sink_manager_->CreateCompositorFrameSink(
        frame_sink_id, /*bundle_id=*/std::nullopt,
        compositor_frame_sink.BindNewPipeAndPassReceiver(),
        compositor_frame_sink_client.BindInterfaceRemote(), std::move(config));
    EXPECT_TRUE(CompositorFrameSinkExists(frame_sink_id));
  }

  std::unique_ptr<input::FlingController> fling_controller_;
  TestOutputSurfaceProvider output_surface_provider_;
  MockGpuServiceImpl gpu_service_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
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
  ASSERT_EQ(fling_scheduler()->GetBeginFrameSource(),
            GetRootBeginFrameSource());

  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller_.get());
  EXPECT_TRUE(fling_scheduler()->observing_begin_frame_source_);
}

TEST_F(FlingSchedulerTest, FlingCancelled) {
  EXPECT_EQ(InputManagerExists(), ExpectedInputManagerCreation());
  if (!InputManagerExists()) {
    // FlingSchedulerAndroid's implementation depends on InputManager creation.
    // Exit if InputManager is not present.
    return;
  }

  SetupFlingController();
  ASSERT_EQ(fling_scheduler()->GetBeginFrameSource(),
            GetRootBeginFrameSource());

  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller_.get());
  EXPECT_TRUE(fling_scheduler()->observing_begin_frame_source_);

  SimulateFlingCancel();
  EXPECT_EQ(nullptr, fling_scheduler()->fling_controller_.get());
  EXPECT_FALSE(fling_scheduler()->observing_begin_frame_source_);
  EXPECT_EQ(fling_scheduler()->GetBeginFrameSource(),
            GetRootBeginFrameSource());
}

// This tests that FlingSchedulerAndroid stops observing BeginFrameSource
// when the RootCompositorFrameSinkImpl gets destroyed, addressing the UAF bug
// described in crbug.com/401501206.
TEST_F(FlingSchedulerTest, ResetStateOnBeginFrameSourceChange) {
  EXPECT_EQ(InputManagerExists(), ExpectedInputManagerCreation());
  if (!InputManagerExists()) {
    // FlingSchedulerAndroid's implementation depends on InputManager creation.
    // Exit if InputManager is not present.
    return;
  }

  SetupFlingController();
  ASSERT_EQ(fling_scheduler()->GetBeginFrameSource(),
            GetRootBeginFrameSource());

  base::TimeTicks progress_time = base::TimeTicks::Now();

  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller_.get());
  EXPECT_TRUE(fling_scheduler()->observing_begin_frame_source_);

  progress_time += base::Milliseconds(17);
  fling_controller()->ProgressFling(progress_time);
  EXPECT_TRUE(fling_controller()->fling_in_progress());

  // RootCompositorFrameSink gets invalidated.
  InvalidateRootFrameSinkId();

  EXPECT_FALSE(fling_scheduler()->observing_begin_frame_source_);
  EXPECT_EQ(nullptr, fling_scheduler()->GetBeginFrameSource());
}

}  // namespace viz
