// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/input_scenario_observer.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/performance_manager/decorators/frame_input_state_decorator.h"
#include "components/performance_manager/embedder/scoped_global_scenario_memory.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using performance_scenarios::GetInputScenario;
using performance_scenarios::ScenarioScope;

class InputScenarioObserverTest : public GraphTestHarness {
 public:
  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<FrameInputStateDecorator>());
    graph->PassToGraph(std::make_unique<InputScenarioObserver>());
  }

 protected:
  FrameInputStateDecorator& frame_input_state() {
    return *FrameInputStateDecorator::GetFromGraph(graph());
  }

 private:
  ScopedGlobalScenarioMemory scenario_memory_;
};

InputScenario GlobalInputScenario() {
  return GetInputScenario(ScenarioScope::kGlobal)
      ->load(std::memory_order_relaxed);
}

InputScenario CurrentProcessInputScenario() {
  return GetInputScenario(ScenarioScope::kCurrentProcess)
      ->load(std::memory_order_relaxed);
}

TEST_F(InputScenarioObserverTest, FrameInputState) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  // Map in the read-only scenario memory for the first mock process as the
  // "current process" state.
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcessNode(mock_graph.process.get());
  ASSERT_TRUE(process_region.IsValid());
  performance_scenarios::ScopedReadOnlyScenarioMemory process_scenario_memory(
      ScenarioScope::kCurrentProcess, std::move(process_region));

  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);

  frame_input_state().UpdateInputScenario(
      mock_graph.frame.get(), InputScenario::kTyping,
      FrameInputStateDecorator::InputScenarioUpdateReason::kKeyEvent);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTyping);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kTyping);

  frame_input_state().UpdateInputScenario(
      mock_graph.child_frame.get(), InputScenario::kTyping,
      FrameInputStateDecorator::InputScenarioUpdateReason::kKeyEvent);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTyping);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kTyping);

  frame_input_state().UpdateInputScenario(
      mock_graph.frame.get(), InputScenario::kNoInput,
      FrameInputStateDecorator::InputScenarioUpdateReason::kTimeout);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTyping);
  // Only `child_frame`, which is hosted in `other_process`, still has input.
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);

  frame_input_state().UpdateInputScenario(
      mock_graph.child_frame.get(), InputScenario::kNoInput,
      FrameInputStateDecorator::InputScenarioUpdateReason::kTimeout);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
}

TEST_F(InputScenarioObserverTest, FrameNodeRemoved) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  // Map in the read-only scenario memory for the mock process as the "current
  // process" state.
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcessNode(mock_graph.process.get());
  ASSERT_TRUE(process_region.IsValid());
  performance_scenarios::ScopedReadOnlyScenarioMemory process_scenario_memory(
      ScenarioScope::kCurrentProcess, std::move(process_region));

  auto new_frame1 =
      CreateFrameNodeAutoId(mock_graph.process.get(), mock_graph.page.get());
  auto new_frame2 =
      CreateFrameNodeAutoId(mock_graph.process.get(), mock_graph.page.get());

  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);

  frame_input_state().UpdateInputScenario(
      new_frame1.get(), InputScenario::kScroll,
      FrameInputStateDecorator::InputScenarioUpdateReason::kScrollStartEvent);
  frame_input_state().UpdateInputScenario(
      new_frame2.get(), InputScenario::kScroll,
      FrameInputStateDecorator::InputScenarioUpdateReason::kScrollStartEvent);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kScroll);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kScroll);

  // Delete a frame receiving input. Another frame is still receiving input.
  new_frame1.reset();
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kScroll);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kScroll);

  // Delete the last frame receiving input.
  new_frame2.reset();
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);
}

TEST_F(InputScenarioObserverTest, OverlappingInputScenarios) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  // Map in the read-only scenario memory for the first mock process as the
  // "current process" state.
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcessNode(mock_graph.process.get());
  ASSERT_TRUE(process_region.IsValid());
  performance_scenarios::ScopedReadOnlyScenarioMemory process_scenario_memory(
      ScenarioScope::kCurrentProcess, std::move(process_region));

  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);

  frame_input_state().UpdateInputScenario(
      mock_graph.frame.get(), InputScenario::kTap,
      FrameInputStateDecorator::InputScenarioUpdateReason::kTapEvent);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTap);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kTap);

  frame_input_state().UpdateInputScenario(
      mock_graph.child_frame.get(), InputScenario::kTyping,
      FrameInputStateDecorator::InputScenarioUpdateReason::kKeyEvent);
  // Tap has priority over typing.
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTap);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kTap);

  frame_input_state().UpdateInputScenario(
      mock_graph.frame.get(), InputScenario::kScroll,
      FrameInputStateDecorator::InputScenarioUpdateReason::kScrollStartEvent);
  // Scroll has priority over typing. Tap is over because a scroll has started
  // in the same frame.
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kScroll);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kScroll);

  frame_input_state().UpdateInputScenario(
      mock_graph.frame.get(), InputScenario::kNoInput,
      FrameInputStateDecorator::InputScenarioUpdateReason::kScrollEndEvent);
  // Scroll has ended, but typing is active in the child frame. The child frame
  // is running in a different process.
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kTyping);
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);

  frame_input_state().UpdateInputScenario(
      mock_graph.child_frame.get(), InputScenario::kNoInput,
      FrameInputStateDecorator::InputScenarioUpdateReason::kTimeout);
  // Now all input scenarios have ended.
  EXPECT_EQ(CurrentProcessInputScenario(), InputScenario::kNoInput);
  EXPECT_EQ(GlobalInputScenario(), InputScenario::kNoInput);
}

}  // namespace

}  // namespace performance_manager
