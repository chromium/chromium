// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_input_state_decorator.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_widget_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using ::testing::_;

class MockFrameInputStateObserver : public FrameInputStateObserver {
 public:
  MOCK_METHOD(void,
              OnInputScenarioChanged,
              (const FrameNode* frame_node, InputScenario previous_scenario),
              (override));
};

template <InputScenario scenario>
void ExpectFrameInputScenario(const FrameNode* frame_node,
                              InputScenario previous_scenario) {
  auto* data = FrameInputStateDecorator::Data::Get(frame_node);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->input_scenario(), scenario);
}

void SimulateKeyPress(content::TestRenderWidgetHost* rwh) {
  // If the key event isn't acked, Android will crash in the
  // RendererUnresponsive handler. TestRenderWidgetHost doesn't have an easy
  // way to simulate an ack from the renderer, so put the renderer in the
  // foreground to send the event and immediately background it to avoid marking
  // it unresponsive.
  rwh->WasShown({});
  rwh->ForwardKeyboardEvent(input::NativeWebKeyboardEvent(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests()));
  rwh->WasHidden();
}

void SimulateTap(content::TestRenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_tap(
      blink::WebGestureEvent::Type::kGestureTap,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_tap.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  rwh->ForwardGestureEvent(gesture_tap);
}

void SimulateScrollBegin(content::TestRenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_scroll_begin.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  rwh->ForwardGestureEvent(gesture_scroll_begin);
}

void SimulateScrollUpdate(content::TestRenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_scroll_update.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  rwh->ForwardGestureEvent(gesture_scroll_update);
}

void SimulateScrollEnd(content::TestRenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_scroll_end(
      blink::WebGestureEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_scroll_end.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  rwh->ForwardGestureEvent(gesture_scroll_end);
}

}  // namespace

class FrameInputStateDecoratorTest : public PerformanceManagerTestHarness {
 protected:
  FrameInputStateDecoratorTest()
      : PerformanceManagerTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());

    // Simulate a committed navigation to create a FrameNode.
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.example.com/"));

    // Start the render widget hidden so that it's in a known state for
    // SimulateKeyPress.
    ASSERT_TRUE(main_render_widget_host());
    main_render_widget_host()->WasHidden();
  }

  void TearDown() override {
    scoped_observation_.Reset();
    PerformanceManagerTestHarness::TearDown();
  }

  void OnGraphCreated(GraphImpl* graph) override {
    auto decorator = std::make_unique<FrameInputStateDecorator>();
    scoped_observation_.Observe(decorator.get());
    graph->PassToGraph(std::move(decorator));
    PerformanceManagerTestHarness::OnGraphCreated(graph);
  }

  MockFrameInputStateObserver& observer() { return observer_; }

  content::TestRenderWidgetHost* main_render_widget_host() {
    return main_rfh() ? static_cast<content::TestRenderWidgetHost*>(
                            main_rfh()->GetRenderWidgetHost())
                      : nullptr;
  }

  base::WeakPtr<FrameNode> main_frame_node() {
    return main_rfh()
               ? PerformanceManager::GetFrameNodeForRenderFrameHost(main_rfh())
               : nullptr;
  }

 protected:
  ::testing::StrictMock<MockFrameInputStateObserver> observer_;
  base::ScopedObservation<FrameInputStateDecorator, FrameInputStateObserver>
      scoped_observation_{&observer_};
};

TEST_F(FrameInputStateDecoratorTest, TypingEvent) {
  ASSERT_TRUE(main_render_widget_host());
  ASSERT_TRUE(main_frame_node());

  // A single keypress isn't considered "typing".
  EXPECT_CALL(observer(), OnInputScenarioChanged(_, _)).Times(0);
  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTyping);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // Two keypresses start typing.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kNoInput))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kTyping>);
  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  SimulateKeyPress(main_render_widget_host());
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // Another keypress before the inactivity timeout maintains the typing state.
  EXPECT_CALL(observer(), OnInputScenarioChanged(_, _)).Times(0);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTyping / 2);
  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTyping / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // The last keypress came halfway through the inactivity timeout, so waiting
  // that long again should reset the typing state.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kTyping))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kNoInput>);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTyping / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(FrameInputStateDecoratorTest, TapEvent) {
  ASSERT_TRUE(main_render_widget_host());
  ASSERT_TRUE(main_frame_node());

  // A tap immediately changes the input state to tap.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kNoInput))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kTap>);
  SimulateTap(main_render_widget_host());
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // Another tap before the inactivity timeout maintains the tap state.
  EXPECT_CALL(observer(), OnInputScenarioChanged(_, _)).Times(0);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTap / 2);
  SimulateTap(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTap / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kTap))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kNoInput>);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTap / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(FrameInputStateDecoratorTest, ScrollEvent) {
  ASSERT_TRUE(main_render_widget_host());
  ASSERT_TRUE(main_frame_node());

  // A scroll begin immediately changes the input state to scroll.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kNoInput))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kScroll>);
  SimulateScrollBegin(main_render_widget_host());
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // Another scroll event before the inactivity timeout maintains the tap state.
  EXPECT_CALL(observer(), OnInputScenarioChanged(_, _)).Times(0);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForScroll / 2);
  SimulateScrollUpdate(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForScroll / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // A scroll end event immediately changes the state to no input.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kScroll))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kNoInput>);
  SimulateScrollEnd(main_render_widget_host());
  ::testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(FrameInputStateDecoratorTest, MixedInput) {
  ASSERT_TRUE(main_render_widget_host());
  ASSERT_TRUE(main_frame_node());

  // Two keypresses start typing.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kNoInput))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kTyping>);
  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  SimulateKeyPress(main_render_widget_host());
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // A tap immediately changes the input state to tap.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kTyping))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kTap>);
  SimulateTap(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTap / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // A scroll begin immediately changes the input state to scroll.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kTap))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kScroll>);
  SimulateScrollBegin(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForScroll / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());

  // A scroll times out - the state should go to no input despite the missing
  // explicit scroll end event.
  EXPECT_CALL(observer(), OnInputScenarioChanged(main_frame_node().get(),
                                                 InputScenario::kScroll))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kNoInput>);
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForScroll / 2);
  ::testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(FrameInputStateDecoratorTest, FrameDestroyed) {
  ASSERT_TRUE(main_render_widget_host());
  base::WeakPtr<FrameNode> frame_node = main_frame_node();
  ASSERT_TRUE(frame_node);

  // Destroying a frame before the inactivity timeout should get a final
  // scenario update before the FrameNode is deleted.
  ::testing::InSequence s;
  EXPECT_CALL(observer(),
              OnInputScenarioChanged(frame_node.get(), InputScenario::kNoInput))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kTyping>);
  EXPECT_CALL(observer(),
              OnInputScenarioChanged(frame_node.get(), InputScenario::kTyping))
      .WillOnce(ExpectFrameInputScenario<InputScenario::kNoInput>);

  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  SimulateKeyPress(main_render_widget_host());
  task_environment()->FastForwardBy(
      FrameInputStateDecorator::kInactivityTimeoutForTyping / 2);

  DeleteContents();
  EXPECT_FALSE(frame_node);
}

}  // namespace performance_manager
