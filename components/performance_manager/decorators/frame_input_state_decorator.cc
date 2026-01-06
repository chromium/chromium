// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_input_state_decorator.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/render_frame_host.h"

namespace performance_manager {

namespace {

std::string GetInputScenarioSuffix(InputScenario scenario) {
  // LINT.IfChange(InputScenarioSuffix)
  switch (scenario) {
    case InputScenario::kScroll:
      return "Scroll";
    case InputScenario::kTap:
      return "Tap";
    case InputScenario::kTyping:
      return "Typing";
    case InputScenario::kNoInput:
      return "NoInput";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/performance_manager/histograms.xml:InputScenarioSuffix)
  NOTREACHED();
}

}  // namespace

// InputObserver receives input events from content::RenderWidgetHost and
// determines the current InputScenario for a frame.
class FrameInputStateDecorator::InputObserver
    : public content::RenderWidgetHost::InputEventObserver,
      public content::RenderWidgetHostObserver {
 public:
  InputObserver(FrameInputStateDecorator* decorator,
                const FrameNode* frame_node,
                RenderFrameHostProxy proxy);
  ~InputObserver() override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const content::RenderWidgetHost& rwh,
                    const blink::WebInputEvent& event,
                    input::InputEventSource source) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(content::RenderWidgetHost* rwh) override;

 private:
  void OnInputInactiveTimer();
  void OnKeyEvent(const blink::WebInputEvent& event);
  void OnScrollEvent(const blink::WebInputEvent& event);
  void OnTapEvent(const blink::WebInputEvent& event);

  raw_ptr<FrameInputStateDecorator> decorator_;
  raw_ptr<const FrameNode> frame_node_;
  base::OneShotTimer timer_;

  // Input detection.
  base::TimeTicks last_key_event_time_;
  InputScenario input_scenario_ = InputScenario::kNoInput;

  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHost::InputEventObserver>
      input_observation_{this};
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      rwh_observation_{this};
};

FrameInputStateDecorator::Data::Data(const FrameNode* frame_node) {
  input_observer_ = std::make_unique<InputObserver>(
      FrameInputStateDecorator::GetFromGraph(frame_node->GetGraph()),
      frame_node, frame_node->GetRenderFrameHostProxy());
}

FrameInputStateDecorator::Data::~Data() = default;

FrameInputStateDecorator::FrameInputStateDecorator() = default;
FrameInputStateDecorator::~FrameInputStateDecorator() = default;

void FrameInputStateDecorator::OnPassedToGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
}

void FrameInputStateDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

void FrameInputStateDecorator::OnFrameNodeAdded(const FrameNode* frame_node) {
  Data::GetOrCreate(frame_node);
}

void FrameInputStateDecorator::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  UpdateInputScenario(frame_node, InputScenario::kNoInput,
                      InputScenarioUpdateReason::kNodeRemoved);
  Data::Destroy(frame_node);
}

void FrameInputStateDecorator::UpdateInputScenario(
    const FrameNode* frame_node,
    InputScenario input_scenario,
    InputScenarioUpdateReason update_reason) {
  auto* data = Data::Get(frame_node);
  if (!data) {
    return;
  }
  InputScenario previous_scenario = data->input_scenario();
  if (previous_scenario == input_scenario) {
    return;
  }
  data->set_input_scenario(input_scenario);
  observers_.Notify(&FrameInputStateObserver::OnInputScenarioChanged,
                    frame_node, previous_scenario);
  constexpr char kHistogramName[] = "PerformanceManager.InputScenarioChanges";
  base::UmaHistogramEnumeration(kHistogramName, update_reason);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kHistogramName, ".", GetInputScenarioSuffix(previous_scenario)}),
      update_reason);
}

void FrameInputStateDecorator::AddObserver(FrameInputStateObserver* observer) {
  observers_.AddObserver(observer);
}

void FrameInputStateDecorator::RemoveObserver(
    FrameInputStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

FrameInputStateDecorator::InputObserver::InputObserver(
    FrameInputStateDecorator* decorator,
    const FrameNode* frame_node,
    RenderFrameHostProxy proxy)
    : decorator_(decorator), frame_node_(frame_node) {
  content::RenderFrameHost* rfh = proxy.Get();
  if (!rfh) {
    // In tests a FrameNode might not be backed by a RenderFrameHost, in which
    // case this observer does nothing. The test can simulate input by calling
    // UpdateInputScenario() directly.
    CHECK_IS_TEST(base::NotFatalUntil::M136);
    return;
  }

  // Observes the RenderWidgetHost and its input events.
  content::RenderWidgetHost* rwh = rfh->GetRenderWidgetHost();
  // `rfh` should not be detached, so it should have a `RenderWidgetHost`.
  CHECK(rwh, base::NotFatalUntil::M136);
  if (rwh) {
    input_observation_.Observe(rwh);
    rwh_observation_.Observe(rwh);
  }
}

FrameInputStateDecorator::InputObserver::~InputObserver() = default;

void FrameInputStateDecorator::InputObserver::OnInputEvent(
    const content::RenderWidgetHost& rwh,
    const blink::WebInputEvent& event,
    input::InputEventSource source) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kRawKeyDown:
      OnKeyEvent(event);
      return;
    // Pinches and flings are classified as scrolls.
    // Based on
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/input/web_gesture_event.h;l=258;drc=317c59e45d1ed2656fdfd6bfecccfdf6ee8e588e
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kGestureScrollEnd:
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
    case blink::WebInputEvent::Type::kGestureFlingStart:
    case blink::WebInputEvent::Type::kGestureFlingCancel:
    case blink::WebInputEvent::Type::kGesturePinchBegin:
    case blink::WebInputEvent::Type::kGesturePinchEnd:
    case blink::WebInputEvent::Type::kGesturePinchUpdate:
      OnScrollEvent(event);
      return;
    // This is based on input event types defined here:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/input/input_event.mojom;l=86;drc=5be39635e5385132d645d2810e14614642291981
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kGestureTapUnconfirmed:
      // Ignored - let's wait until they're resolved into a more specific
      // gesture.
      return;
    case blink::WebInputEvent::Type::kGestureTapCancel:
      // Ignored because it means that we didn't get any GestureTap event.
      return;
    case blink::WebInputEvent::Type::kGestureShowPress:
    case blink::WebInputEvent::Type::kGestureTap:
    case blink::WebInputEvent::Type::kGestureShortPress:
    case blink::WebInputEvent::Type::kGestureLongPress:
    case blink::WebInputEvent::Type::kGestureLongTap:
    case blink::WebInputEvent::Type::kGestureTwoFingerTap:
    case blink::WebInputEvent::Type::kGestureDoubleTap:
      OnTapEvent(event);
      return;
    default:
      // We intentionally ignore other input types.
      return;
  }
}

void FrameInputStateDecorator::InputObserver::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* rwh) {
  // Stops observing the RenderWidgetHost if it's destroyed before the
  // InputObserver. If the InputObserver is destroyed first, the destructor
  // automatically deletes the ScopedObservations and stops observing.
  input_observation_.Reset();
  rwh_observation_.Reset();
  // TODO(crbug.com/365586676): This is not expected to happen in practice.
  // Remove this function if no reports are received.
  CHECK(false, base::NotFatalUntil::M136);
}

void FrameInputStateDecorator::InputObserver::OnInputInactiveTimer() {
  DCHECK(input_scenario_ != InputScenario::kNoInput);
  input_scenario_ = InputScenario::kNoInput;
  decorator_->UpdateInputScenario(frame_node_, input_scenario_,
                                  InputScenarioUpdateReason::kTimeout);
}

void FrameInputStateDecorator::InputObserver::OnKeyEvent(
    const blink::WebInputEvent& event) {
  base::TimeTicks now = base::TimeTicks::Now();
  // Only consider continuous key events (>2 in 1 second) as typing to avoid
  // noise from single key press.
  if (input_scenario_ == InputScenario::kNoInput &&
      now - last_key_event_time_ < base::Seconds(1)) {
    input_scenario_ = InputScenario::kTyping;
    decorator_->UpdateInputScenario(frame_node_, input_scenario_,
                                    InputScenarioUpdateReason::kKeyEvent);
  }
  if (input_scenario_ == InputScenario::kTyping) {
    // While typing, every key event pushes the deadline to exit the
    // typing scenario.
    timer_.Start(FROM_HERE, kInactivityTimeoutForTyping,
                 base::BindOnce(&InputObserver::OnInputInactiveTimer,
                                base::Unretained(this)));
  }
  last_key_event_time_ = now;
}

void FrameInputStateDecorator::InputObserver::OnScrollEvent(
    const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
    case blink::WebInputEvent::Type::kGestureFlingStart:
    case blink::WebInputEvent::Type::kGesturePinchBegin:
    case blink::WebInputEvent::Type::kGesturePinchUpdate:
      input_scenario_ = InputScenario::kScroll;
      break;
    case blink::WebInputEvent::Type::kGestureScrollEnd:
    case blink::WebInputEvent::Type::kGestureFlingCancel:
    case blink::WebInputEvent::Type::kGesturePinchEnd:
      input_scenario_ = InputScenario::kNoInput;
      timer_.Stop();
      break;
    default:
      NOTREACHED();
  }
  decorator_->UpdateInputScenario(
      frame_node_, input_scenario_,
      input_scenario_ == InputScenario::kScroll
          ? InputScenarioUpdateReason::kScrollStartEvent
          : InputScenarioUpdateReason::kScrollEndEvent);
  if (input_scenario_ == InputScenario::kScroll) {
    // While scrolling, every scroll event pushes the deadline to exit the
    // scrolling scenario. Note: this is just a fail safe if the end/cancel
    // event goes missing.
    timer_.Start(FROM_HERE, kInactivityTimeoutForScroll,
                 base::BindOnce(&InputObserver::OnInputInactiveTimer,
                                base::Unretained(this)));
  }
}

void FrameInputStateDecorator::InputObserver::OnTapEvent(
    const blink::WebInputEvent& event) {
  if (input_scenario_ == InputScenario::kScroll) {
    return;
  }
  if (input_scenario_ != InputScenario::kTap) {
    input_scenario_ = InputScenario::kTap;
    decorator_->UpdateInputScenario(frame_node_, input_scenario_,
                                    InputScenarioUpdateReason::kTapEvent);
  }
  // Every tap event pushes the deadline to exit the tap scenario.
  timer_.Start(FROM_HERE, kInactivityTimeoutForTap,
               base::BindOnce(&InputObserver::OnInputInactiveTimer,
                              base::Unretained(this)));
}

}  // namespace performance_manager
