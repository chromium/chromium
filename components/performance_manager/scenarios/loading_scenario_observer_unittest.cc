// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/loading_scenario_observer.h"

#include <atomic>
#include <memory>

#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

namespace {

using blink::performance_scenarios::GetLoadingScenario;
using blink::performance_scenarios::Scope;

class LoadingScenarioObserverTest : public GraphTestHarness {
 public:
  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<LoadingScenarioObserver>());
  }

  LoadingScenario GlobalLoadingScenario() const {
    return GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed);
  }

 private:
  ScopedGlobalScenarioMemory scenario_memory_;
};

TEST_F(LoadingScenarioObserverTest, LoadingStateOnePage) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadingTimedOut);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
}

TEST_F(LoadingScenarioObserverTest, LoadingStateMultiplePages) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
}

TEST_F(LoadingScenarioObserverTest, IsVisibleChangeWhileLoading) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
}

TEST_F(LoadingScenarioObserverTest, IsVisibleChangeWhileOtherPageLoading) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.other_page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.other_page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
}

TEST_F(LoadingScenarioObserverTest, LoadingStateChangeWhileVisible) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
}

TEST_F(LoadingScenarioObserverTest, LoadingStateChangeWhileOtherPageVisible) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.other_page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
}

TEST_F(LoadingScenarioObserverTest, IsFocusedChangeWhileLoading) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
}

TEST_F(LoadingScenarioObserverTest, IsFocusedChangeWhileOtherPageLoading) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.other_page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.other_page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.other_page->SetIsFocused(false);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
}

// If a page becomes focused and visible at the same time (such as a pop-up
// that's focused when it appears), the notifications can arrive in either
// order. A focused page should always be considered visible.
TEST_F(LoadingScenarioObserverTest, FocusedAndVisible) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  // One page is visible while the other becomes focused and visible.
  mock_graph.other_page->SetLoadingState(PageNode::LoadingState::kLoading);
  mock_graph.other_page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  mock_graph.page->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  mock_graph.page->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  mock_graph.page->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
}

TEST_F(LoadingScenarioObserverTest, PageNodeRemoved) {
  auto page1 = CreateNode<PageNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto page3 = CreateNode<PageNodeImpl>();
  page3->SetIsVisible(true);
  auto page4 = CreateNode<PageNodeImpl>();
  page4->SetIsVisible(true);
  page4->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  page1.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  page2.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  page3->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  page3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);

  page4->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page4.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
}

TEST_F(LoadingScenarioObserverTest, PageNodeRemovedWhileBackgroundPageLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  auto page1 = CreateNode<PageNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto page3 = CreateNode<PageNodeImpl>();
  page3->SetIsVisible(true);
  auto page4 = CreateNode<PageNodeImpl>();
  page4->SetIsVisible(true);
  page4->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  page1.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  page2.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  page3->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  page3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);

  page4->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page4.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
}

TEST_F(LoadingScenarioObserverTest, PageNodeRemovedWhileVisiblePageLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  auto page1 = CreateNode<PageNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto page3 = CreateNode<PageNodeImpl>();
  page3->SetIsVisible(true);
  auto page4 = CreateNode<PageNodeImpl>();
  page4->SetIsVisible(true);
  page4->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  page1.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  page2.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  page3->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  page3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);

  page4->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page4.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
}

TEST_F(LoadingScenarioObserverTest, PageNodeRemovedWhileFocusedPageLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetIsFocused(true);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  auto page1 = CreateNode<PageNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto page3 = CreateNode<PageNodeImpl>();
  page3->SetIsVisible(true);
  auto page4 = CreateNode<PageNodeImpl>();
  page4->SetIsVisible(true);
  page4->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);

  page1.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);

  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page2.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);

  page3->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);

  page4->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  page4.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
}

}  // namespace

}  // namespace performance_manager
