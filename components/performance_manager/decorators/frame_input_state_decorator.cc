// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_input_state_decorator.h"

#include "base/check_is_test.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/render_frame_host.h"

namespace performance_manager {

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
  switch (Data::Get(frame_node)->input_scenario()) {
    case InputScenario::kNoInput:
      break;
    case InputScenario::kTyping:
      UpdateInputScenario(frame_node, false);
      break;
  }
  Data::Destroy(frame_node);
}

void FrameInputStateDecorator::UpdateInputScenario(const FrameNode* frame_node,
                                                   bool typing) {
  InputScenario new_scenario =
      typing ? InputScenario::kTyping : InputScenario::kNoInput;

  auto* data = Data::Get(frame_node);
  if (!data || data->input_scenario() == new_scenario) {
    return;
  }

  data->set_input_scenario(new_scenario);
  observers_.Notify(&FrameInputStateObserver::OnInputScenarioChanged,
                    frame_node);
}

void FrameInputStateDecorator::AddObserver(FrameInputStateObserver* observer) {
  observers_.AddObserver(observer);
}

void FrameInputStateDecorator::RemoveObserver(
    FrameInputStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

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
                    const blink::WebInputEvent& event) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(content::RenderWidgetHost* rwh) override;

 private:
  void OnInputInactiveTimer();
  void OnKeyEvent(const blink::WebInputEvent& event);

  raw_ptr<FrameInputStateDecorator> decorator_;
  raw_ptr<const FrameNode> frame_node_;
  base::OneShotTimer timer_;

  // Typing detection:
  base::TimeTicks last_key_event_time_;
  bool typing_ = false;

  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHost::InputEventObserver>
      input_observation_{this};
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      rwh_observation_{this};
};

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
    const blink::WebInputEvent& event) {
  // Currently we only care about key down events.
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    OnKeyEvent(event);
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
  DCHECK(typing_);
  typing_ = false;
  decorator_->UpdateInputScenario(frame_node_, typing_);
}

void FrameInputStateDecorator::InputObserver::OnKeyEvent(
    const blink::WebInputEvent& event) {
  base::TimeTicks now = base::TimeTicks::Now();
  // Only consider continuous key events (>2 in 1 second) as typing to avoid
  // noise from single key press.
  if (!typing_ && now - last_key_event_time_ < base::Seconds(1)) {
    typing_ = true;
    decorator_->UpdateInputScenario(frame_node_, typing_);
  }
  if (typing_) {
    // While typing, every key event pushes the deadline to exit the
    // typing scenario.
    timer_.Start(FROM_HERE, kInactivityTimeoutForTyping,
                 base::BindOnce(&InputObserver::OnInputInactiveTimer,
                                base::Unretained(this)));
  }
  last_key_event_time_ = now;
}

}  // namespace performance_manager
