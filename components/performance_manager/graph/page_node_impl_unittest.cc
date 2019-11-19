// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl.h"

#include "base/stl_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using PageNodeImplTest = GraphTestHarness;

}  // namespace

TEST_F(PageNodeImplTest, SafeDowncast) {
  auto page = CreateNode<PageNodeImpl>();
  PageNode* node = page.get();
  EXPECT_EQ(page.get(), PageNodeImpl::FromNode(node));
  NodeBase* base = page.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

using PageNodeImplDeathTest = PageNodeImplTest;

TEST_F(PageNodeImplDeathTest, SafeDowncast) {
  auto page = CreateNode<PageNodeImpl>();
  ASSERT_DEATH_IF_SUPPORTED(FrameNodeImpl::FromNodeBase(page.get()), "");
}

TEST_F(PageNodeImplTest, AddFrameBasic) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto parent_frame =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());
  auto child1_frame = CreateFrameNodeAutoId(process_node.get(), page_node.get(),
                                            parent_frame.get(), 1);
  auto child2_frame = CreateFrameNodeAutoId(process_node.get(), page_node.get(),
                                            parent_frame.get(), 2);

  // Validate that all frames are tallied to the page.
  EXPECT_EQ(3u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, RemoveFrame) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node =
      CreateFrameNodeAutoId(process_node.get(), page_node.get(), nullptr, 0);

  // Ensure correct page-frame relationship has been established.
  auto frame_nodes = GraphImplOperations::GetFrameNodes(page_node.get());
  EXPECT_EQ(1u, frame_nodes.size());
  EXPECT_TRUE(base::Contains(frame_nodes, frame_node.get()));
  EXPECT_EQ(page_node.get(), frame_node->page_node());

  frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(40, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(20, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(70, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(50, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(mock_graph.page->is_visible());
  AdvanceClock(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            mock_graph.page->TimeSinceLastVisibilityChange());

  mock_graph.page->SetIsVisible(false);
  AdvanceClock(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            mock_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(mock_graph.page->is_visible());
}

TEST_F(PageNodeImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(mock_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  GURL url("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  10u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(10u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            mock_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  url = GURL("http://www.example.org/bobcat");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  20u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(20u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            mock_graph.page->TimeSinceLastNavigation());

  // Test a same-document navigation.
  url = GURL("http://www.example.org/bobcat#fun");
  mock_graph.page->OnMainFrameNavigationCommitted(true, base::TimeTicks::Now(),
                                                  30u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(30u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            mock_graph.page->TimeSinceLastNavigation());
}

TEST_F(PageNodeImplTest, BrowserContextID) {
  const std::string kTestBrowserContextId =
      base::UnguessableToken::Create().ToString();
  auto page_node =
      CreateNode<PageNodeImpl>(WebContentsProxy(), kTestBrowserContextId);
  const PageNode* public_page_node = page_node.get();

  EXPECT_EQ(page_node->browser_context_id(), kTestBrowserContextId);
  EXPECT_EQ(public_page_node->GetBrowserContextID(), kTestBrowserContextId);
}

TEST_F(PageNodeImplTest, IsLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->is_loading());

  // Set to false and the property should stay false.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());

  // Set to true and the property should read true.
  page_node->SetIsLoading(true);
  EXPECT_TRUE(page_node->is_loading());

  // Set to false and the property should read false again.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
}

namespace {

class LenientMockObserver : public PageNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnPageNodeAdded, void(const PageNode*));
  MOCK_METHOD1(OnBeforePageNodeRemoved, void(const PageNode*));
  MOCK_METHOD1(OnIsVisibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsAudibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsLoadingChanged, void(const PageNode*));
  MOCK_METHOD1(OnUkmSourceIdChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageLifecycleStateChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageOriginTrialFreezePolicyChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageIsHoldingWebLockChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageIsHoldingIndexedDBLockChanged, void(const PageNode*));
  MOCK_METHOD1(OnMainFrameUrlChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageAlmostIdleChanged, void(const PageNode*));
  MOCK_METHOD1(OnMainFrameDocumentChanged, void(const PageNode*));
  MOCK_METHOD1(OnTitleUpdated, void(const PageNode*));
  MOCK_METHOD1(OnFaviconUpdated, void(const PageNode*));

  void SetNotifiedPageNode(const PageNode* page_node) {
    notified_page_node_ = page_node;
  }

  const PageNode* TakeNotifiedPageNode() {
    const PageNode* node = notified_page_node_;
    notified_page_node_ = nullptr;
    return node;
  }

 private:
  const PageNode* notified_page_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(PageNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();

  MockObserver obs;
  graph()->AddPageNodeObserver(&obs);

  // Create a page node and expect a matching call to "OnPageNodeAdded".
  EXPECT_CALL(obs, OnPageNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* raw_page_node = page_node.get();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsVisibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsVisible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsAudibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsAudible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsLoadingChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsLoading(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnUkmSourceIdChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetUkmSourceId(static_cast<ukm::SourceId>(0x1234));
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnPageLifecycleStateChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetLifecycleStateForTesting(PageNodeImpl::LifecycleState::kFrozen);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnPageAlmostIdleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetPageAlmostIdleForTesting(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  const GURL kTestUrl = GURL("https://foo.com/");
  int64_t navigation_id = 0x1234;
  EXPECT_CALL(obs, OnMainFrameUrlChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  // Expect no OnMainFrameDocumentChanged for same-document navigation
  page_node->OnMainFrameNavigationCommitted(true, base::TimeTicks::Now(),
                                            ++navigation_id, kTestUrl);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnMainFrameDocumentChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                            ++navigation_id, kTestUrl);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnTitleUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnTitleUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnFaviconUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnFaviconUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  // Release the page node and expect a call to "OnBeforePageNodeRemoved".
  EXPECT_CALL(obs, OnBeforePageNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node.reset();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  graph()->RemovePageNodeObserver(&obs);
}

TEST_F(PageNodeImplTest, PublicInterface) {
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* public_page_node = page_node.get();

  // Simply test that the public interface impls yield the same result as their
  // private counterpart.

  EXPECT_EQ(page_node->browser_context_id(),
            public_page_node->GetBrowserContextID());
  EXPECT_EQ(page_node->page_almost_idle(),
            public_page_node->IsPageAlmostIdle());
  EXPECT_EQ(page_node->is_visible(), public_page_node->IsVisible());
  EXPECT_EQ(page_node->is_audible(), public_page_node->IsAudible());
  EXPECT_EQ(page_node->is_loading(), public_page_node->IsLoading());
  EXPECT_EQ(page_node->ukm_source_id(), public_page_node->GetUkmSourceID());
  EXPECT_EQ(page_node->lifecycle_state(),
            public_page_node->GetLifecycleState());
  EXPECT_EQ(page_node->navigation_id(), public_page_node->GetNavigationID());
  EXPECT_EQ(page_node->main_frame_url(), public_page_node->GetMainFrameUrl());
}

}  // namespace performance_manager
