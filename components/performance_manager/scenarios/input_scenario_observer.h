// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_INPUT_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_INPUT_SCENARIO_OBSERVER_H_

#include "base/sequence_checker.h"
#include "components/performance_manager/decorators/frame_input_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class InputScenarioObserver : public FrameInputStateObserver,
                              public ProcessNodeObserver,
                              public GraphOwned {
 public:
  InputScenarioObserver();
  ~InputScenarioObserver() override;

  InputScenarioObserver(const InputScenarioObserver&) = delete;
  InputScenarioObserver& operator=(const InputScenarioObserver&) = delete;

  // FrameInputStateObserver:
  void OnInputScenarioChanged(const FrameNode* frame_node) override;

  // ProcessNodeObserver:
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  std::map<const ProcessNode*, size_t> process_input_scenarios_count_;
  size_t global_input_scenarios_count_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_INPUT_SCENARIO_OBSERVER_H_
