// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl.h"

#include <string>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using PageNodeImplTest = GraphTestHarness;

const std::string kHtmlMimeType = "text/html";
const std::string kPdfMimeType = "application/pdf";

static const freezing::FreezingVote kFreezingVote(
    freezing::FreezingVoteValue::kCannotFreeze,
    "cannot freeze");

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

TEST_F(PageNodeImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(mock_graph.page->is_visible());
  AdvanceClock(base::Seconds(42));
  EXPECT_EQ(base::Seconds(42),
            mock_graph.page->TimeSinceLastVisibilityChange());

  mock_graph.page->SetIsVisible(false);
  AdvanceClock(base::Seconds(23));
  EXPECT_EQ(base::Seconds(23),
            mock_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(mock_graph.page->is_visible());
}

TEST_F(PageNodeImplTest, TimeSinceLastAudibleChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  EXPECT_FALSE(mock_graph.page->is_audible());
  EXPECT_EQ(absl::nullopt, mock_graph.page->TimeSinceLastAudibleChange());

  mock_graph.page->SetIsAudible(true);
  EXPECT_TRUE(mock_graph.page->is_audible());
  AdvanceClock(base::Seconds(42));
  EXPECT_EQ(base::Seconds(42), mock_graph.page->TimeSinceLastAudibleChange());

  mock_graph.page->SetIsAudible(false);
  AdvanceClock(base::Seconds(23));
  EXPECT_EQ(base::Seconds(23), mock_graph.page->TimeSinceLastAudibleChange());
  EXPECT_FALSE(mock_graph.page->is_audible());

  // Test a page that's audible at creation.
  auto audible_page = CreateNode<PageNodeImpl>(
      WebContentsProxy(), /*browser_context_id=*/std::string(), GURL(),
      PagePropertyFlags{PagePropertyFlag::kIsAudible});
  AdvanceClock(base::Seconds(56));
  EXPECT_EQ(base::Seconds(56), audible_page->TimeSinceLastAudibleChange());
  EXPECT_TRUE(audible_page->is_audible());
}

TEST_F(PageNodeImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(mock_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  GURL url("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  10u, url, kHtmlMimeType);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(10u, mock_graph.page->navigation_id());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->contents_mime_type());
  AdvanceClock(base::Seconds(11));
  EXPECT_EQ(base::Seconds(11), mock_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  url = GURL("http://www.example.org/bobcat");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  20u, url, kHtmlMimeType);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(20u, mock_graph.page->navigation_id());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->contents_mime_type());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->TimeSinceLastNavigation());

  // Test a same-document navigation.
  url = GURL("http://www.example.org/bobcat#fun");
  mock_graph.page->OnMainFrameNavigationCommitted(true, base::TimeTicks::Now(),
                                                  30u, url, kHtmlMimeType);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(30u, mock_graph.page->navigation_id());
  EXPECT_EQ(kHtmlMimeType, mock_graph.page->contents_mime_type());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->TimeSinceLastNavigation());

  // Test a navigation to a page with a different MIME type.
  url = GURL("http://www.example.org/document.pdf");
  mock_graph.page->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(),
                                                  40u, url, kPdfMimeType);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(40u, mock_graph.page->navigation_id());
  EXPECT_EQ(kPdfMimeType, mock_graph.page->contents_mime_type());
  AdvanceClock(base::Seconds(17));
  EXPECT_EQ(base::Seconds(17), mock_graph.page->TimeSinceLastNavigation());
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

TEST_F(PageNodeImplTest, LoadingState) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should start at kLoadingNotStarted.
  EXPECT_EQ(PageNode::LoadingState::kLoadingNotStarted,
            page_node->loading_state());

  // Set to kLoadingNotStarted and the property should stay kLoadingNotStarted.
  page_node->SetLoadingState(PageNode::LoadingState::kLoadingNotStarted);
  EXPECT_EQ(PageNode::LoadingState::kLoadingNotStarted,
            page_node->loading_state());

  // Set to kLoading and the property should switch to kLoading.
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(PageNode::LoadingState::kLoading, page_node->loading_state());

  // Set to kLoading again and the property should stay kLoading.
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(PageNode::LoadingState::kLoading, page_node->loading_state());
}

TEST_F(PageNodeImplTest, HadFormInteractions) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->had_form_interaction());

  page_node->SetHadFormInteractionForTesting(true);
  EXPECT_TRUE(page_node->had_form_interaction());

  page_node->SetHadFormInteractionForTesting(false);
  EXPECT_FALSE(page_node->had_form_interaction());
}

TEST_F(PageNodeImplTest, HadUserEdits) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->had_user_edits());

  page_node->SetHadUserEditsForTesting(true);
  EXPECT_TRUE(page_node->had_user_edits());

  page_node->SetHadUserEditsForTesting(false);
  EXPECT_FALSE(page_node->had_user_edits());
}

TEST_F(PageNodeImplTest, GetFreezingVote) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to absl::nullopt.
  EXPECT_FALSE(page_node->freezing_vote());

  page_node->set_freezing_vote(kFreezingVote);
  ASSERT_TRUE(page_node->freezing_vote().has_value());
  EXPECT_EQ(kFreezingVote, page_node->freezing_vote().value());

  page_node->set_freezing_vote(absl::nullopt);
  EXPECT_FALSE(page_node->freezing_vote());
}

namespace {

class LenientMockObserver : public PageNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnPageNodeAdded, void(const PageNode*));
  MOCK_METHOD1(OnBeforePageNodeRemoved, void(const PageNode*));
  // Note that opener/embedder functionality is actually tested in the
  // FrameNodeImpl and GraphImpl unittests.
  MOCK_METHOD2(OnOpenerFrameNodeChanged,
               void(const PageNode*, const FrameNode*));
  MOCK_METHOD3(OnEmbedderFrameNodeChanged,
               void(const PageNode*, const FrameNode*, EmbeddingType));
  MOCK_METHOD2(OnTypeChanged, void(const PageNode*, PageType));
  MOCK_METHOD1(OnIsFocusedChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsVisibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsAudibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnHasPictureInPictureChanged, void(const PageNode*));
  MOCK_METHOD2(OnLoadingStateChanged,
               void(const PageNode*, PageNode::LoadingState));
  MOCK_METHOD1(OnUkmSourceIdChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageLifecycleStateChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageIsHoldingWebLockChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageIsHoldingIndexedDBLockChanged, void(const PageNode*));
  MOCK_METHOD1(OnMainFrameUrlChanged, void(const PageNode*));
  MOCK_METHOD1(OnMainFrameDocumentChanged, void(const PageNode*));
  MOCK_METHOD1(OnTitleUpdated, void(const PageNode*));
  MOCK_METHOD1(OnFaviconUpdated, void(const PageNode*));
  MOCK_METHOD1(OnHadFormInteractionChanged, void(const PageNode*));
  MOCK_METHOD1(OnHadUserEditsChanged, void(const PageNode*));
  MOCK_METHOD2(OnFreezingVoteChanged,
               void(const PageNode*, absl::optional<freezing::FreezingVote>));
  MOCK_METHOD2(OnPageStateChanged, void(const PageNode*, PageNode::PageState));
  MOCK_METHOD2(OnAboutToBeDiscarded, void(const PageNode*, const PageNode*));

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
      true, base::TimeTicks::Now(), ++navigation_id, kTestUrl, kHtmlMimeType);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnMainFrameDocumentChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), ++navigation_id, kTestUrl, kHtmlMimeType);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnTitleUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnTitleUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnFaviconUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnFaviconUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnFreezingVoteChanged(_, testing::Eq(absl::nullopt)))
      .WillOnce(testing::WithArg<0>(
          Invoke(&obs, &MockObserver::SetNotifiedPageNode)));
  page_node->set_freezing_vote(kFreezingVote);
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
  EXPECT_EQ(page_node->is_focused(), public_page_node->IsFocused());
  EXPECT_EQ(page_node->is_visible(), public_page_node->IsVisible());
  EXPECT_EQ(page_node->is_audible(), public_page_node->IsAudible());
  EXPECT_EQ(page_node->loading_state(), public_page_node->GetLoadingState());
  EXPECT_EQ(page_node->ukm_source_id(), public_page_node->GetUkmSourceID());
  EXPECT_EQ(page_node->lifecycle_state(),
            public_page_node->GetLifecycleState());

  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 10u,
                                            GURL("https://foo.com"),
                                            kHtmlMimeType);
  EXPECT_EQ(page_node->navigation_id(), public_page_node->GetNavigationID());
  EXPECT_EQ(page_node->main_frame_url(), public_page_node->GetMainFrameUrl());
  EXPECT_EQ(page_node->contents_mime_type(),
            public_page_node->GetContentsMimeType());
  EXPECT_EQ(page_node->freezing_vote(), public_page_node->GetFreezingVote());
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

TEST_F(PageNodeImplTest, VisitMainFrameNodes) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNodeAutoId(process.get(), page.get());
  auto frame2 = CreateFrameNodeAutoId(process.get(), page.get());

  std::set<const FrameNode*> visited;
  EXPECT_TRUE(ToPublic(page.get())
                  ->VisitMainFrameNodes([&visited](const FrameNode* frame) {
                    EXPECT_TRUE(visited.insert(frame).second);
                    return true;
                  }));
  EXPECT_THAT(visited, testing::UnorderedElementsAre(ToPublic(frame1.get()),
                                                     ToPublic(frame2.get())));

  // Do an aborted visit.
  visited.clear();
  EXPECT_FALSE(ToPublic(page.get())
                   ->VisitMainFrameNodes([&visited](const FrameNode* frame) {
                     EXPECT_TRUE(visited.insert(frame).second);
                     return false;
                   }));
  EXPECT_EQ(1u, visited.size());
}

TEST_F(PageNodeImplTest, BackForwardCache) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  EXPECT_EQ(PageNode::PageState::kActive, page->page_state());

  MockObserver obs;
  graph()->AddPageNodeObserver(&obs);

  EXPECT_CALL(obs, OnPageStateChanged(_, PageNode::PageState::kActive));
  page->set_page_state(PageNode::PageState::kBackForwardCache);
  EXPECT_EQ(PageNode::PageState::kBackForwardCache, page->page_state());

  graph()->RemovePageNodeObserver(&obs);
}

TEST_F(PageNodeImplTest, Prerendering) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(
      WebContentsProxy(),                   // wc_proxy
      std::string(),                        // browser_context_id
      GURL(),                               // url
      PagePropertyFlags{},                  // initial_property_flags
      base::TimeTicks::Now(),               // visibility_change_time
      PageNode::PageState::kPrerendering);  // page_state
  EXPECT_EQ(PageNode::PageState::kPrerendering, page->page_state());

  MockObserver obs;
  graph()->AddPageNodeObserver(&obs);

  EXPECT_CALL(obs, OnPageStateChanged(_, PageNode::PageState::kPrerendering));
  page->set_page_state(PageNode::PageState::kActive);
  EXPECT_EQ(PageNode::PageState::kActive, page->page_state());

  graph()->RemovePageNodeObserver(&obs);
}

}  // namespace performance_manager
