// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/input_scenario_observer.h"

#include "base/check.h"

namespace performance_manager {

InputScenarioObserver::InputScenarioObserver() = default;
InputScenarioObserver::~InputScenarioObserver() = default;

void InputScenarioObserver::OnInputScenarioChanged(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = FrameInputStateDecorator::Data::Get(frame_node);
  CHECK(data, base::NotFatalUntil::M136);

  const ProcessNode* process_node = frame_node->GetProcessNode();
  size_t& process_input_count = process_input_scenarios_count_[process_node];
  if (data->input_scenario() == InputScenario::kNoInput) {
    CHECK_GT(process_input_count, 0u);
    CHECK_GT(global_input_scenarios_count_, 0u);
    process_input_count -= 1;
    global_input_scenarios_count_ -= 1;
    if (process_input_count == 0) {
      SetInputScenarioForProcessNode(InputScenario::kNoInput, process_node);
    }
    if (global_input_scenarios_count_ == 0) {
      SetGlobalInputScenario(InputScenario::kNoInput);
    }
  } else {
    process_input_count += 1;
    global_input_scenarios_count_ += 1;
    if (process_input_count == 1) {
      SetInputScenarioForProcessNode(data->input_scenario(), process_node);
    }
    if (global_input_scenarios_count_ == 1) {
      SetGlobalInputScenario(data->input_scenario());
    }
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
