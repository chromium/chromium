// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/input_scenario_observer.h"

#include "base/check.h"

namespace performance_manager {

namespace {

// Input scenarios order is fixed, see the InputScenario enum comments.
// The one with a higher numeric value always has priority.
size_t GetActiveProcessInputScenario(
    const std::array<size_t, static_cast<size_t>(InputScenario::kMax) + 1>&
        input_counts) {
  size_t largest_non_zero_index = input_counts.size() - 1;
  while (largest_non_zero_index > 0 &&
         input_counts.at(largest_non_zero_index) == 0) {
    --largest_non_zero_index;
  }
  return largest_non_zero_index;
}

size_t GetActiveGlobalInputScenario(
    const std::map<
        const ProcessNode*,
        std::array<size_t, static_cast<size_t>(InputScenario::kMax) + 1>>&
        process_input_counts) {
  size_t largest_index = 0;
  for (const auto& [unused_k, input_counts] : process_input_counts) {
    largest_index =
        std::max(largest_index, GetActiveProcessInputScenario(input_counts));
  }
  return largest_index;
}

}  // namespace

InputScenarioObserver::InputScenarioObserver() = default;
InputScenarioObserver::~InputScenarioObserver() = default;

void InputScenarioObserver::OnInputScenarioChanged(
    const FrameNode* frame_node,
    InputScenario previous_scenario) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = FrameInputStateDecorator::Data::Get(frame_node);
  CHECK(data, base::NotFatalUntil::M136);

  const ProcessNode* process_node = frame_node->GetProcessNode();
  auto& process_input_counts = process_input_scenarios_count_[process_node];
  const size_t previous_process_scenario_index =
      GetActiveProcessInputScenario(process_input_counts);
  const size_t previous_global_scenario_index =
      GetActiveGlobalInputScenario(process_input_scenarios_count_);
  if (previous_scenario != InputScenario::kNoInput) {
    const size_t scenario_index = static_cast<size_t>(previous_scenario);
    CHECK_GT(process_input_counts.at(scenario_index), 0u);
    --process_input_counts.at(scenario_index);
  }
  if (data->input_scenario() != InputScenario::kNoInput) {
    const size_t scenario_index = static_cast<size_t>(data->input_scenario());
    ++process_input_counts.at(scenario_index);
  }

  const size_t process_scenario_index =
      GetActiveProcessInputScenario(process_input_counts);
  const size_t global_scenario_index =
      GetActiveGlobalInputScenario(process_input_scenarios_count_);
  if (process_scenario_index != previous_process_scenario_index) {
    SetInputScenarioForProcessNode(
        static_cast<InputScenario>(process_scenario_index), process_node);
  }
  if (global_scenario_index != previous_global_scenario_index) {
    SetGlobalInputScenario(static_cast<InputScenario>(global_scenario_index));
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
