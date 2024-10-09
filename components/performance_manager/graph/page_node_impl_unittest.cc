// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using PageNodeImplTest = GraphTestHarness;

const std::string kHtmlMimeType = "text/html";
const std::string kPdfMimeType = "application/pdf";
const blink::mojom::PermissionStatus kAskNotificationPermission =
    blink::mojom::PermissionStatus::ASK;

const PageNode* ToPublic(PageNodeImpl* page_node) {
  return page_node;
}

const FrameNode* ToPublic(FrameNodeImpl* frame_node) {
  return frame_node;
}

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
                                            parent_frame.get());
  auto child2_frame = CreateFrameNodeAutoId(process_node.get(), page_node.get(),
                                            parent_frame.get());

  // Validate that all frames are tallied to the page.
  EXPECT_EQ(3u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, RemoveFrame) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node =
      CreateFrameNodeAutoId(process_node.get(), page_node.get(), nullptr);

  // Ensure correct page-frame relationship has been established.
  auto frame_nodes = GraphImplOperations::GetFrameNodes(page_node.get());
  EXPECT_EQ(1u, frame_nodes.size());
  EXPECT_TRUE(base::Contains(frame_nodes, frame_node.get()));
  EXPECT_EQ(page_node.get(), frame_node->page_node());

  frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, GetTimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(mock_graph.page->IsVisible());
  AdvanceClock(base::Seconds(42));
  EXPECT_EQ(base::Seconds(42),
            mock_graph.page->GetTimeSinceLastVisibilityChange());

  mock_graph.page->SetIsVisible(false);
  AdvanceClock(base::Seconds(23));
  EXPECT_EQ(base::Seconds(23),
            mock_graph.page->GetTimeSinceLastVisibilityChange());
  EXPECT_FALSE(mock_graph.page->IsVisible());
}

TEST_F(PageNodeImplTest, GetTimeSinceLastAudibleChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  EXPECT_FALSE(mock_graph.page->IsAudible());
  EXPECT_EQ(std::nullopt, mock_graph.page->GetTimeSinceLastAudibleChange());

  mock_graph.page->SetIsAudible(true);
  EXPECT_TRUE(mock_graph.page->IsAudible());
  AdvanceClock(base::Seconds(42));
  EXPECT_EQ(base::Seconds(42),
            mock_graph.page->GetTimeSinceLastAudibleChange());

  mock_graph.page->SetIsAudible(false);
  AdvanceClock(base::Seconds(23));
  EXPECT_EQ(base::Seconds(23),
            mock_graph.page->GetTimeSinceLastAudibleChange());
  EXPECT_FALSE(mock_graph.page->IsAudible());

  // Test a page that's audible at creation.
  auto audible_page = CreateNode<PageNodeImpl>(
      nullptr, /*browser_context_id=*/std::string(), GURL(),
      PagePropertyFlags{PagePropertyFlag::kIsAudible});
  AdvanceClock(base::Seconds(56));
  EXPECT_EQ(base::Seconds(56), audible_page->GetTimeSinceLastAudibleChange());
  EXPECT_TRUE(audible_page->IsAudible());
}

TEST_F(PageNodeImplTest, GetTimeSinceLastNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(mock_graph.page->GetTimeSinceLastNavigation().is_zero());

  // 1st navigation.
  GURL url("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  10u, url, kHtmlMimeType,
                                                  kAskNotificationPermission);
  EXPECT_EQ(url, mock_graph.page->GetMainFrameUrl());
  EXPECT_EQ(10u, mock_graph.page->GetNavigationID());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->GetContentsMimeType());
  AdvanceClock(base::Seconds(11));
  EXPECT_EQ(base::Seconds(11), mock_graph.page->GetTimeSinceLastNavigation());

  // 2nd navigation.
  url = GURL("http://www.example.org/bobcat");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  20u, url, kHtmlMimeType,
                                                  kAskNotificationPermission);
  EXPECT_EQ(url, mock_graph.page->GetMainFrameUrl());
  EXPECT_EQ(20u, mock_graph.page->GetNavigationID());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->GetContentsMimeType());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->GetTimeSinceLastNavigation());

  // Test a same-document navigation.
  url = GURL("http://www.example.org/bobcat#fun");
  mock_graph.page->OnMainFrameNavigationCommitted(true, base::TimeTicks::Now(),
                                                  30u, url, kHtmlMimeType,
                                                  kAskNotificationPermission);
  EXPECT_EQ(url, mock_graph.page->GetMainFrameUrl());
  EXPECT_EQ(30u, mock_graph.page->GetNavigationID());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->GetContentsMimeType());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->GetTimeSinceLastNavigation());

  // Test a navigation to a page with a different MIME type.
  url = GURL("http://www.example.org/document.pdf");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  40u, url, kPdfMimeType,
                                                  kAskNotificationPermission);
  EXPECT_EQ(url, mock_graph.page->GetMainFrameUrl());
  EXPECT_EQ(40u, mock_graph.page->GetNavigationID());
  EXPECT_EQ(kPdfMimeType, mock_graph.page->GetContentsMimeType());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->GetTimeSinceLastNavigation());
}

TEST_F(PageNodeImplTest, BrowserContextID) {
  const std::string kTestBrowserContextId =
      base::UnguessableToken::Create().ToString();
  auto page_node = CreateNode<PageNodeImpl>(nullptr, kTestBrowserContextId);

  EXPECT_EQ(page_node->GetBrowserContextID(), kTestBrowserContextId);
}

TEST_F(PageNodeImplTest, LoadingState) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should start at kLoadingNotStarted.
  EXPECT_EQ(PageNode::LoadingState::kLoadingNotStarted,
            page_node->GetLoadingState());

  // Set to kLoadingNotStarted and the property should stay kLoadingNotStarted.
  page_node->SetLoadingState(PageNode::LoadingState::kLoadingNotStarted);
  EXPECT_EQ(PageNode::LoadingState::kLoadingNotStarted,
            page_node->GetLoadingState());

  // Set to kLoading and the property should switch to kLoading.
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(PageNode::LoadingState::kLoading, page_node->GetLoadingState());

  // Set to kLoading again and the property should stay kLoading.
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(PageNode::LoadingState::kLoading, page_node->GetLoadingState());
}

TEST_F(PageNodeImplTest, HadFormInteractions) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->HadFormInteraction());

  page_node->SetHadFormInteractionForTesting(true);
  EXPECT_TRUE(page_node->HadFormInteraction());

  page_node->SetHadFormInteractionForTesting(false);
  EXPECT_FALSE(page_node->HadFormInteraction());
}

TEST_F(PageNodeImplTest, HadUserEdits) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->HadUserEdits());

  page_node->SetHadUserEditsForTesting(true);
  EXPECT_TRUE(page_node->HadUserEdits());

  page_node->SetHadUserEditsForTesting(false);
  EXPECT_FALSE(page_node->HadUserEdits());
}

namespace {

class LenientMockObserver : public PageNodeImpl::Observer {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD(void, OnPageNodeAdded, (const PageNode*), (override));
  MOCK_METHOD(void, OnBeforePageNodeRemoved, (const PageNode*), (override));
  // Note that opener/embedder functionality is actually tested in the
  // FrameNodeImpl and GraphImpl unittests.
  MOCK_METHOD(void,
              OnOpenerFrameNodeChanged,
              (const PageNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnEmbedderFrameNodeChanged,
              (const PageNode*, const FrameNode*, EmbeddingType),
              (override));
  MOCK_METHOD(void, OnTypeChanged, (const PageNode*, PageType), (override));
  MOCK_METHOD(void, OnIsFocusedChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnIsVisibleChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnIsAudibleChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnHasPictureInPictureChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void,
              OnLoadingStateChanged,
              (const PageNode*, PageNode::LoadingState),
              (override));
  MOCK_METHOD(void, OnUkmSourceIdChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnPageLifecycleStateChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnPageIsHoldingWebLockChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void,
              OnPageIsHoldingIndexedDBLockChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void, OnPageUsesWebRTCChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnMainFrameUrlChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnMainFrameDocumentChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnTitleUpdated, (const PageNode*), (override));
  MOCK_METHOD(void, OnFaviconUpdated, (const PageNode*), (override));
  MOCK_METHOD(void, OnHadFormInteractionChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnHadUserEditsChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnAboutToBeDiscarded,
              (const PageNode*, const PageNode*),
              (override));

  void SetNotifiedPageNode(const PageNode* page_node) {
    notified_page_node_ = page_node;
  }

  const PageNode* TakeNotifiedPageNode() {
    const PageNode* node = notified_page_node_;
    notified_page_node_ = nullptr;
    return node;
  }

 private:
  raw_ptr<const PageNode, DanglingUntriaged> notified_page_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

}  // namespace

TEST_F(PageNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();

  MockObserver head_obs;
  MockObserver obs;
  MockObserver tail_obs;
  graph()->AddPageNodeObserver(&head_obs);
  graph()->AddPageNodeObserver(&obs);
  graph()->AddPageNodeObserver(&tail_obs);

  // Remove observers at the head and tail of the list inside a callback, and
  // expect that `obs` is still notified correctly.
  EXPECT_CALL(head_obs, OnPageNodeAdded(_)).WillOnce(InvokeWithoutArgs([&] {
    graph()->RemovePageNodeObserver(&head_obs);
    graph()->RemovePageNodeObserver(&tail_obs);
  }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnPageNodeAdded(_)).Times(0);

  // Create a page node and expect a matching call to "OnPageNodeAdded".
  EXPECT_CALL(obs, OnPageNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* raw_page_node = page_node.get();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsFocusedChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsFocused(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsVisibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsVisible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsAudibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsAudible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnLoadingStateChanged(_, _))
      .WillOnce(testing::WithArg<0>(
          Invoke(&obs, &MockObserver::SetNotifiedPageNode)));
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnUkmSourceIdChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetUkmSourceId(static_cast<ukm::SourceId>(0x1234));
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnPageLifecycleStateChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetLifecycleStateForTesting(PageNodeImpl::LifecycleState::kFrozen);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  const GURL kTestUrl = GURL("https://foo.com/");
  int64_t navigation_id = 0x1234;
  EXPECT_CALL(obs, OnMainFrameUrlChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  // Expect no OnMainFrameDocumentChanged for same-document navigation
  page_node->OnMainFrameNavigationCommitted(
      true, base::TimeTicks::Now(), ++navigation_id, kTestUrl, kHtmlMimeType,
      kAskNotificationPermission);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnMainFrameDocumentChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), ++navigation_id, kTestUrl, kHtmlMimeType,
      kAskNotificationPermission);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnTitleUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnTitleUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnFaviconUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnFaviconUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  // Re-entrant iteration should work.
  EXPECT_CALL(obs, OnIsFocusedChanged(raw_page_node))
      .WillOnce(InvokeWithoutArgs([&] { page_node->SetIsVisible(false); }));
  EXPECT_CALL(obs, OnIsVisibleChanged(raw_page_node));
  page_node->SetIsFocused(false);

  // Release the page node and expect a call to "OnBeforePageNodeRemoved".
  EXPECT_CALL(obs, OnBeforePageNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node.reset();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  graph()->RemovePageNodeObserver(&obs);
}

TEST_F(PageNodeImplTest, SetMainFrameRestoredState) {
  const GURL kUrl("http://www.example.org");
  auto page = CreateNode<PageNodeImpl>();
  const PageNode* raw_page_node = page.get();
  EXPECT_EQ(page->GetNotificationPermissionStatus(), std::nullopt);

  MockObserver obs;
  graph()->AddPageNodeObserver(&obs);

  EXPECT_CALL(obs, OnMainFrameUrlChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page->SetMainFrameRestoredState(kUrl,
                                  /* notification_permission_status=*/blink::
                                      mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_EQ(page->GetMainFrameUrl(), kUrl);
  EXPECT_EQ(page->GetNotificationPermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  graph()->RemovePageNodeObserver(&obs);
}

TEST_F(PageNodeImplTest, PublicInterface) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* public_page_node = page_node.get();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  // Simply test that the public interface impls yield the same result as their
  // private counterpart.

  EXPECT_EQ(page_node->main_frame_node(), public_page_node->GetMainFrameNode());
}

TEST_F(PageNodeImplTest, OpenerFrameNode) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto opener_frame_node =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());

  auto opened_page_node = CreateNode<PageNodeImpl>();
  const PageNode* public_opened_page_node = opened_page_node.get();

  opened_page_node->SetOpenerFrameNode(opener_frame_node.get());

  EXPECT_EQ(opened_page_node->opener_frame_node(), opener_frame_node.get());
  EXPECT_EQ(public_opened_page_node->GetOpenerFrameNode(),
            opener_frame_node.get());
}

TEST_F(PageNodeImplTest, EmbedderFrameNode) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto embedder_frame_node =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());

  auto embedded_page_node = CreateNode<PageNodeImpl>();
  const PageNode* public_embedded_page_node = embedded_page_node.get();

  embedded_page_node->SetEmbedderFrameNodeAndEmbeddingType(
      embedder_frame_node.get(), PageNode::EmbeddingType::kGuestView);

  EXPECT_EQ(embedded_page_node->GetEmbeddingType(),
            PageNode::EmbeddingType::kGuestView);
  EXPECT_EQ(embedded_page_node->embedder_frame_node(),
            embedder_frame_node.get());
  EXPECT_EQ(public_embedded_page_node->GetEmbedderFrameNode(),
            embedder_frame_node.get());
}
TEST_F(PageNodeImplTest, GetMainFrameNodes) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNodeAutoId(process.get(), page.get());
  auto frame2 = CreateFrameNodeAutoId(process.get(), page.get());

  auto frames = ToPublic(page.get())->GetMainFrameNodes();
  EXPECT_THAT(frames, testing::UnorderedElementsAre(ToPublic(frame1.get()),
                                                    ToPublic(frame2.get())));
}

}  // namespace performance_manager
