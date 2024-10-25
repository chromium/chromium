// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_tab_helper.h"

#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace performance_manager {

namespace {

const char kParentUrl[] = "https://parent.com/";
const char kChild1Url[] = "https://child1.com/";
const char kChild2Url[] = "https://child2.com/";
const char kGrandchildUrl[] = "https://grandchild.com/";
const char kNewGrandchildUrl[] = "https://newgrandchild.com/";
const char kCousinFreddyUrl[] = "https://cousinfreddy.com/";

class PerformanceManagerTabHelperTest
    : public PerformanceManagerTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  PerformanceManagerTabHelperTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kSeamlessRenderFrameSwap, GetParam());
  }

  void TearDown() override {
    // Clean up the web contents, which should dispose of the page and frame
    // nodes involved.
    DeleteContents();

    PerformanceManagerTestHarness::TearDown();
  }

  // A helper function for checking that the graph matches the topology of
  // stuff in content. The graph should reflect the set of processes provided
  // by |hosts|, even though content may actually have other processes lying
  // around.
  void CheckGraphTopology(const std::set<content::RenderProcessHost*>& hosts,
                          const char* grandchild_url);

 protected:
  static size_t CountAllRenderProcessHosts() {
    size_t num_hosts = 0;
    for (auto it = content::RenderProcessHost::AllHostsIterator();
         !it.IsAtEnd(); it.Advance()) {
      ++num_hosts;
    }
    return num_hosts;
  }

  static size_t CountAllRenderProcessNodes(GraphImpl* graph) {
    size_t num_hosts = 0;
    for (ProcessNodeImpl* process_node : graph->GetAllProcessNodeImpls()) {
      if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
        ++num_hosts;
      }
    }
    return num_hosts;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

void PerformanceManagerTabHelperTest::CheckGraphTopology(
    const std::set<content::RenderProcessHost*>& hosts,
    const char* grandchild_url) {
  // There may be more RenderProcessHosts in existence than those used from
  // the RFHs above. The graph may not reflect all of them, as only those
  // observed through the TabHelper will have been reflected in the graph.
  size_t num_hosts = CountAllRenderProcessHosts();
  EXPECT_LE(hosts.size(), num_hosts);
  EXPECT_NE(0u, hosts.size());

  // Convert the RPHs to ProcessNodeImpls so we can check they match.
  std::set<ProcessNodeImpl*> process_nodes;
  for (auto* host : hosts) {
    auto* data = RenderProcessUserData::GetForRenderProcessHost(host);
    EXPECT_TRUE(data);
    process_nodes.insert(data->process_node());
  }
  EXPECT_EQ(process_nodes.size(), hosts.size());

  // Check out the graph itself.
  RunInGraph([&process_nodes, num_hosts, grandchild_url](GraphImpl* graph) {
    EXPECT_GE(num_hosts, CountAllRenderProcessNodes(graph));
    EXPECT_EQ(4u, graph->GetAllFrameNodes().size());

    // Expect all frame nodes to be current. This fails if our
    // implementation of RenderFrameHostChanged is borked.
    for (auto* frame : graph->GetAllFrameNodeImpls()) {
      EXPECT_TRUE(frame->IsCurrent());
    }

    ASSERT_EQ(1u, graph->GetAllPageNodes().size());
    auto* page = graph->GetAllPageNodeImpls().AsVector()[0];

    // Extra RPHs can and most definitely do exist.
    auto associated_process_nodes =
        GraphImplOperations::GetAssociatedProcessNodes(page);
    EXPECT_GE(CountAllRenderProcessNodes(graph),
              associated_process_nodes.size());
    EXPECT_GE(num_hosts, associated_process_nodes.size());

    for (ProcessNodeImpl* process_node : associated_process_nodes) {
      EXPECT_TRUE(base::Contains(process_nodes, process_node));
    }

    EXPECT_EQ(4u, GraphImplOperations::GetFrameNodes(page).size());
    ASSERT_EQ(1u, page->main_frame_nodes().size());

    auto* main_frame = page->main_frame_node();
    EXPECT_EQ(kParentUrl, main_frame->GetURL().spec());
    EXPECT_EQ(2u, main_frame->child_frame_nodes().size());

    for (FrameNodeImpl* child_frame : main_frame->child_frame_nodes()) {
      if (child_frame->GetURL().spec() == kChild1Url) {
        ASSERT_EQ(1u, child_frame->child_frame_nodes().size());
        auto* grandchild_frame = *child_frame->child_frame_nodes().begin();
        EXPECT_EQ(grandchild_url, grandchild_frame->GetURL().spec());
      } else if (child_frame->GetURL().spec() == kChild2Url) {
        EXPECT_TRUE(child_frame->child_frame_nodes().empty());
      } else {
        FAIL() << "Unexpected child frame: " << child_frame->GetURL().spec();
      }
    }
  });
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(All, PerformanceManagerTabHelperTest, testing::Bool());

TEST_P(PerformanceManagerTabHelperTest, FrameHierarchyReflectsToGraph) {
  SetContents(CreateTestWebContents());

  auto* parent = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kParentUrl));
  DCHECK(parent);

  auto* parent_tester = content::RenderFrameHostTester::For(parent);
  auto* child1 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChild1Url), parent_tester->AppendChild("child1"));
  auto* grandchild =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kGrandchildUrl),
          content::RenderFrameHostTester::For(child1)->AppendChild(
              "grandchild"));
  auto* child2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChild2Url), parent_tester->AppendChild("child2"));

  // Count the RFHs referenced.
  std::set<content::RenderProcessHost*> hosts;
  auto* grandchild_process = grandchild->GetProcess();
  hosts.insert(main_rfh()->GetProcess());
  hosts.insert(child1->GetProcess());
  hosts.insert(grandchild->GetProcess());
  hosts.insert(child2->GetProcess());

  CheckGraphTopology(hosts, kGrandchildUrl);

  // Navigate the grand-child frame. This tests that we accurately observe the
  // new RFH being created and marked current, with the old one being marked not
  // current and torn down. Note that the old RPH doesn't get torn down.
  auto* new_grandchild =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kNewGrandchildUrl), grandchild);
  auto* new_grandchild_process = new_grandchild->GetProcess();

  // Update the set of processes we expect to be associated with the page.
  hosts.erase(grandchild_process);
  hosts.insert(new_grandchild_process);

  CheckGraphTopology(hosts, kNewGrandchildUrl);

  // Clean up the web contents, which should dispose of the page and frame nodes
  // involved.
  DeleteContents();

  // Allow content/ to settle.
  task_environment()->RunUntilIdle();

  size_t num_hosts = CountAllRenderProcessHosts();

  RunInGraph([num_hosts](GraphImpl* graph) {
    EXPECT_GE(num_hosts, CountAllRenderProcessNodes(graph));
    EXPECT_EQ(0u, graph->GetAllFrameNodes().size());
    ASSERT_EQ(0u, graph->GetAllPageNodes().size());
  });
}

namespace {

void ExpectPageIsAudible(bool is_audible) {
  RunInGraph([&](GraphImpl* graph) {
    ASSERT_EQ(1u, graph->GetAllPageNodes().size());
    auto* page = graph->GetAllPageNodeImpls().AsVector()[0];
    EXPECT_EQ(is_audible, page->IsAudible());
  });
}

#if !BUILDFLAG(IS_ANDROID)
void ExpectNotificationPermissionStatus(
    std::optional<blink::mojom::PermissionStatus> status) {
  RunInGraph([&](GraphImpl* graph) {
    ASSERT_EQ(1u, graph->GetAllPageNodes().size());
    auto* page = graph->GetAllPageNodeImpls().AsVector()[0];
    EXPECT_EQ(status, page->GetNotificationPermissionStatus());
  });
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

TEST_P(PerformanceManagerTabHelperTest, PageIsAudible) {
  SetContents(CreateTestWebContents());

  ExpectPageIsAudible(false);
  content::WebContentsTester::For(web_contents())->SetIsCurrentlyAudible(true);
  ExpectPageIsAudible(true);
  content::WebContentsTester::For(web_contents())->SetIsCurrentlyAudible(false);
  ExpectPageIsAudible(false);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_P(PerformanceManagerTabHelperTest, NotificationPermission) {
  auto owned_permission_controller = std::make_unique<
      testing::StrictMock<content::MockPermissionController>>();
  auto* permission_controller = owned_permission_controller.get();
  GetBrowserContext()->SetPermissionControllerForTesting(
      std::move(owned_permission_controller));
  content::PermissionController::SubscriptionId::Generator
      subscription_id_generator;
  const auto kFirstSubscriptionId = subscription_id_generator.GenerateNextId();
  const auto kSecondSubscriptionId = subscription_id_generator.GenerateNextId();

  SetContents(CreateTestWebContents());
  ExpectNotificationPermissionStatus(std::nullopt);

  // Navigate to an origin with `PermissionStatus::ASK`.
  {
    content::RenderFrameHost* rfh_arg = nullptr;
    content::RenderFrameHost* rfh_arg_2 = nullptr;
    EXPECT_CALL(*permission_controller,
                GetPermissionStatusForCurrentDocument(
                    blink::PermissionType::NOTIFICATIONS, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<1>(&rfh_arg),
            testing::Return(blink::mojom::PermissionStatus::ASK)));
    EXPECT_CALL(*permission_controller,
                SubscribeToPermissionStatusChange(
                    blink::PermissionType::NOTIFICATIONS, testing::_,
                    testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<2>(&rfh_arg_2),
                                 testing::Return(kFirstSubscriptionId)));
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kParentUrl));
    testing::Mock::VerifyAndClear(permission_controller);
    EXPECT_EQ(rfh_arg, web_contents()->GetPrimaryMainFrame());
    EXPECT_EQ(rfh_arg_2, web_contents()->GetPrimaryMainFrame());
    ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::ASK);
  }

  base::RepeatingCallback<void(content::PermissionStatus)> callback_arg;

  // Navigate to an origin with `PermissionStatus::GRANTED`.
  {
    content::RenderFrameHost* rfh_arg = nullptr;
    content::RenderProcessHost* rph_arg = nullptr;
    EXPECT_CALL(*permission_controller,
                GetPermissionStatusForCurrentDocument(
                    blink::PermissionType::NOTIFICATIONS, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<1>(&rfh_arg),
            testing::Return(blink::mojom::PermissionStatus::GRANTED)));
    EXPECT_CALL(*permission_controller,
                UnsubscribeFromPermissionStatusChange(kFirstSubscriptionId));
    EXPECT_CALL(*permission_controller,
                SubscribeToPermissionStatusChange(
                    blink::PermissionType::NOTIFICATIONS, testing::_,
                    testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&rph_arg),
                                 testing::SaveArg<5>(&callback_arg),
                                 testing::Return(kSecondSubscriptionId)));
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kCousinFreddyUrl));
    testing::Mock::VerifyAndClear(permission_controller);
    EXPECT_EQ(rfh_arg, web_contents()->GetPrimaryMainFrame());
    ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  }

  // Simulate a change of permission status independent from navigation.
  callback_arg.Run(blink::mojom::PermissionStatus::DENIED);
  ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  // The last subscription is removed when the tab helper is deleted.
  EXPECT_CALL(*permission_controller,
              UnsubscribeFromPermissionStatusChange(kSecondSubscriptionId));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_P(PerformanceManagerTabHelperTest, GetFrameNode) {
  SetContents(CreateTestWebContents());

  auto* tab_helper =
      PerformanceManagerTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  // GetFrameNode() can return nullptr. In this test, it is achieved by using an
  // empty RenderFrameHost.
  auto* empty_frame = web_contents()->GetPrimaryMainFrame();
  DCHECK(empty_frame);

  auto* empty_frame_node = tab_helper->GetFrameNode(empty_frame);
  EXPECT_FALSE(empty_frame_node);

  // This navigation will create a frame node.
  auto* new_frame = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kParentUrl));
  DCHECK(new_frame);

  auto* new_frame_node = tab_helper->GetFrameNode(new_frame);
  EXPECT_TRUE(new_frame_node);
}

namespace {

class LenientMockPageNodeObserver : public PageNode::ObserverDefaultImpl {
 public:
  LenientMockPageNodeObserver() = default;
  ~LenientMockPageNodeObserver() override = default;
  LenientMockPageNodeObserver(const LenientMockPageNodeObserver& other) =
      delete;
  LenientMockPageNodeObserver& operator=(const LenientMockPageNodeObserver&) =
      delete;

  MOCK_METHOD(void, OnFaviconUpdated, (const PageNode*), (override));
};
using MockPageNodeObserver = ::testing::StrictMock<LenientMockPageNodeObserver>;

}  // namespace

TEST_P(PerformanceManagerTabHelperTest,
       NotificationsFromInactiveFrameTreeAreIgnored) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  auto* first_nav_main_rfh = web_contents()->GetPrimaryMainFrame();

  content::LeaveInPendingDeletionState(first_nav_main_rfh);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kCousinFreddyUrl));
  EXPECT_NE(web_contents()->GetPrimaryMainFrame(), first_nav_main_rfh);

  // Mock observer, this can only be used from the PM sequence.
  MockPageNodeObserver observer;
  RunInGraph([&](Graph* graph) { graph->AddPageNodeObserver(&observer); });

  auto* tab_helper =
      PerformanceManagerTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  // The first favicon change is always ignored, call DidUpdateFaviconURL twice
  // to ensure that the test doesn't pass simply because of that.
  tab_helper->DidUpdateFaviconURL(first_nav_main_rfh, {});
  tab_helper->DidUpdateFaviconURL(first_nav_main_rfh, {});

  RunInGraph([&] {
    // The observer shouldn't have been called at this point.
    testing::Mock::VerifyAndClear(&observer);
    // Set the expectation for the next check.
    EXPECT_CALL(observer, OnFaviconUpdated(::testing::_));
  });

  // Sanity check to ensure that notification sent to the active main frame are
  // forwarded. DidUpdateFaviconURL needs to be called twice as the first
  // favicon change is always ignored.
  tab_helper->DidUpdateFaviconURL(web_contents()->GetPrimaryMainFrame(), {});
  tab_helper->DidUpdateFaviconURL(web_contents()->GetPrimaryMainFrame(), {});

  RunInGraph([&](Graph* graph) {
    testing::Mock::VerifyAndClear(&observer);
    graph->RemovePageNodeObserver(&observer);
  });
}

}  // namespace performance_manager
