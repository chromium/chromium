// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/loading_scenario_observer.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
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

  // Creates a mock RenderProcessHost that can store the process scenario
  // mapping for a ProcessNode.
  content::RenderProcessHost* CreateMockRenderProcessHost() {
    return rph_factory_.CreateRenderProcessHost(&dummy_browser_context_,
                                                dummy_site_instance_.get());
  }

 private:
  ScopedGlobalScenarioMemory scenario_memory_;

  // Dummy BrowserContext and SiteInstance for mock RenderProcessHosts. The
  // BrowserContext must outlive the SiteInstance.
  content::TestBrowserContext dummy_browser_context_;
  scoped_refptr<content::SiteInstance> dummy_site_instance_ =
      content::SiteInstance::Create(&dummy_browser_context_);

  // Factory to create mock RenderProcessHosts. This must be deleted before
  // the BrowserContext and SiteInstance to clean up all RPH's it created.
  content::MockRenderProcessHostFactory rph_factory_;
};

LoadingScenario GlobalLoadingScenario() {
  return GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed);
}

LoadingScenario CurrentProcessLoadingScenario() {
  return GetLoadingScenario(Scope::kCurrentProcess)
      ->load(std::memory_order_relaxed);
}

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

TEST_F(LoadingScenarioObserverTest, PerProcessState) {
  // Process state needs RenderProcessHosts.
  content::RenderProcessHost* rph1 = CreateMockRenderProcessHost();
  content::RenderProcessHost* rph2 = CreateMockRenderProcessHost();

  // Map in the read-only scenario memory for the first mock process as the
  // "current process" state.
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcess(rph1);
  ASSERT_TRUE(process_region.IsValid());
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_scenario_memory(Scope::kCurrentProcess,
                              std::move(process_region));

  // Create a page with a frame backed by the "current" mock process.
  auto process1 =
      CreateRendererProcessNode(RenderProcessHostProxy::CreateForTesting(
          RenderProcessHostId(rph1->GetID())));
  auto page1 = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNodeAutoId(process1.get(), page1.get());

  // Create a second page with a frame backed by a different process.
  auto process2 =
      CreateRendererProcessNode(RenderProcessHostProxy::CreateForTesting(
          RenderProcessHostId(rph2->GetID())));
  auto page2 = CreateNode<PageNodeImpl>();
  auto frame2 = CreateFrameNodeAutoId(process2.get(), page2.get());

  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kNoPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(), LoadingScenario::kNoPageLoading);

  // Only changes to `page1` should be visible in the current process state.
  page1->SetIsVisible(false);
  page1->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
  page2->SetIsVisible(true);
  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  // Add a frame to `page2` that's hosted in the "current" process. The
  // current process scenario should now update.
  auto frame3 =
      CreateFrameNodeAutoId(process1.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);

  // Changes to both pages should now be reflected in the current process
  // scenario.
  page2->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
  page1->SetIsVisible(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);

  // Remove the current process from `page2`. It should immediately be removed
  // from the current process scenario, and the scenario should no longer update
  // when `page2` changes.
  page2->SetIsVisible(true);
  page2->SetIsFocused(true);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kFocusedPageLoading);
  frame3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  page1->SetIsVisible(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
  page2->SetIsFocused(false);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  // Add another frames that's NOT hosted in the current process to `page2`.
  // This shouldn't affect the current process scenario.
  frame3 = CreateFrameNodeAutoId(process2.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  // Now add 2 frames hosted in the current process to `page2`. It should be
  // part of the current process scenario as long as at least 1 frame is hosted
  // in it.
  auto frame4 =
      CreateFrameNodeAutoId(process1.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  auto frame5 =
      CreateFrameNodeAutoId(process1.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  frame3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  frame4.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  frame5.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  // Add a frame from `page2` while it's NOT loading. Remove it while it's
  // loading.
  page2->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  frame3 = CreateFrameNodeAutoId(process1.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
  page2->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  frame3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);

  // Add a frame from `page2` while it's loading. Remove it while it's not
  // loading.
  frame3 = CreateFrameNodeAutoId(process1.get(), page2.get(), frame2.get());
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kVisiblePageLoading);
  page2->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
  frame3.reset();
  EXPECT_EQ(GlobalLoadingScenario(), LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(CurrentProcessLoadingScenario(),
            LoadingScenario::kBackgroundPageLoading);
}

}  // namespace

}  // namespace performance_manager
