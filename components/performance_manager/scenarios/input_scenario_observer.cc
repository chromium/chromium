// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/input_scenario_observer.h"

#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/render_frame_host.h"

namespace performance_manager {

InputScenarioObserver::InputScenarioObserver() = default;
InputScenarioObserver::~InputScenarioObserver() = default;

void InputScenarioObserver::OnInputScenarioChanged(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = FrameInputStateDecorator::Data::Get(frame_node);
  content::RenderFrameHost* rfh = frame_node->GetRenderFrameHostProxy().Get();
  if (!rfh || !data) {
    return;
  }

  const ProcessNode* process_node = frame_node->GetProcessNode();
  if (data->input_scenario() == InputScenario::kNoInput) {
    process_input_scenarios_count_[process_node] -= 1;
    if (process_input_scenarios_count_[process_node] == 0) {
      // TODO(crbug.com/365586676): This should also call
      // SetGlobalInputScenario.
      SetInputScenarioForProcessNode(data->input_scenario(), process_node);
    }
  } else {
    process_input_scenarios_count_[process_node] += 1;
    SetInputScenarioForProcessNode(data->input_scenario(), process_node);
  }
}

void InputScenarioObserver::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FrameInputStateDecorator* frame_input_state_decorator =
      FrameInputStateDecorator::GetFromGraph(graph);
  CHECK(frame_input_state_decorator);
  frame_input_state_decorator->AddObserver(this);
  graph->AddProcessNodeObserver(this);
}

void InputScenarioObserver::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FrameInputStateDecorator* frame_input_state_decorator =
      FrameInputStateDecorator::GetFromGraph(graph);
  if (frame_input_state_decorator) {
    frame_input_state_decorator->RemoveObserver(this);
  }
  graph->RemoveProcessNodeObserver(this);
}

void InputScenarioObserver::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  process_input_scenarios_count_.erase(process_node);
}

}  // namespace performance_manager
