// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_tab_helper.h"

#include <optional>
#include <set>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/test_support/graph/mock_page_node_observer.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
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

class PerformanceManagerTabHelperTest : public PerformanceManagerTestHarness {
 public:
  PerformanceManagerTabHelperTest() = default;

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

  static size_t CountAllRenderProcessNodes(Graph* graph) {
    size_t num_hosts = 0;
    for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
      if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
        ++num_hosts;
      }
    }
    return num_hosts;
  }
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
  std::set<const ProcessNode*> process_nodes;
  for (auto* host : hosts) {
    auto* data = RenderProcessUserData::GetForRenderProcessHost(host);
    EXPECT_TRUE(data);
    process_nodes.insert(data->process_node());
  }
  EXPECT_EQ(process_nodes.size(), hosts.size());

  // Check out the graph itself.
  auto* graph = PerformanceManager::GetGraph();

  EXPECT_GE(num_hosts, CountAllRenderProcessNodes(graph));
  EXPECT_EQ(4u, graph->GetAllFrameNodes().size());

  // Expect all frame nodes to be active. This fails if our
  // implementation of RenderFrameHostChanged is borked.
  for (auto* frame : graph->GetAllFrameNodes()) {
    EXPECT_TRUE(frame->IsActive());
  }

  ASSERT_EQ(1u, graph->GetAllPageNodes().size());
  auto* page = graph->GetAllPageNodes().AsVector()[0];

  // Extra RPHs can and most definitely do exist.
  auto associated_process_nodes =
      GraphOperations::GetAssociatedProcessNodes(page);
  EXPECT_GE(CountAllRenderProcessNodes(graph), associated_process_nodes.size());
  EXPECT_GE(num_hosts, associated_process_nodes.size());

  for (const ProcessNode* process_node : associated_process_nodes) {
    EXPECT_TRUE(process_nodes.contains(process_node));
  }

  EXPECT_EQ(4u, GraphOperations::GetFrameNodes(page).size());
  ASSERT_EQ(1u, page->GetMainFrameNodes().size());

  auto* main_frame = page->GetPrimaryMainFrameNode();
  EXPECT_EQ(kParentUrl, main_frame->GetURL().spec());
  EXPECT_EQ(2u, main_frame->GetChildFrameNodes().size());

  for (const FrameNode* child_frame : main_frame->GetChildFrameNodes()) {
    if (child_frame->GetURL().spec() == kChild1Url) {
      ASSERT_EQ(1u, child_frame->GetChildFrameNodes().size());
      auto* grandchild_frame = *child_frame->GetChildFrameNodes().begin();
      EXPECT_EQ(grandchild_url, grandchild_frame->GetURL().spec());
    } else if (child_frame->GetURL().spec() == kChild2Url) {
      EXPECT_TRUE(child_frame->GetChildFrameNodes().empty());
    } else {
      FAIL() << "Unexpected child frame: " << child_frame->GetURL().spec();
    }
  }
}

}  // namespace

TEST_F(PerformanceManagerTabHelperTest, FrameHierarchyReflectsToGraph) {
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

  auto* graph = PerformanceManager::GetGraph();
  EXPECT_GE(num_hosts, CountAllRenderProcessNodes(graph));
  EXPECT_EQ(0u, graph->GetAllFrameNodes().size());
  ASSERT_EQ(0u, graph->GetAllPageNodes().size());
}

namespace {

void ExpectPageIsAudible(bool is_audible) {
  auto* graph = PerformanceManager::GetGraph();
  ASSERT_EQ(1u, graph->GetAllPageNodes().size());
  auto* page = graph->GetAllPageNodes().AsVector()[0];
  EXPECT_EQ(is_audible, page->IsAudible());
}

#if !BUILDFLAG(IS_ANDROID)
void ExpectNotificationPermissionStatus(
    std::optional<blink::mojom::PermissionStatus> status) {
  auto* graph = PerformanceManager::GetGraph();
  ASSERT_EQ(1u, graph->GetAllPageNodes().size());
  auto* page = graph->GetAllPageNodes().AsVector()[0];
  EXPECT_EQ(status, page->GetNotificationPermissionStatus());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

TEST_F(PerformanceManagerTabHelperTest, PageIsAudible) {
  SetContents(CreateTestWebContents());

  ExpectPageIsAudible(false);
  content::WebContentsTester::For(web_contents())->SetIsCurrentlyAudible(true);
  ExpectPageIsAudible(true);
  content::WebContentsTester::For(web_contents())->SetIsCurrentlyAudible(false);
  ExpectPageIsAudible(false);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PerformanceManagerTabHelperTest, NotificationPermission) {
  auto owned_permission_controller =
      std::make_unique<testing::NiceMock<content::MockPermissionController>>();
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
    blink::mojom::PermissionDescriptorPtr descriptor;

    EXPECT_CALL(*permission_controller, GetPermissionStatusForCurrentDocument)
        .WillOnce([&](const blink::mojom::PermissionDescriptorPtr&
                          permission_descriptor,
                      content::RenderFrameHost* render_frame_host) {
          descriptor = permission_descriptor->Clone();
          rfh_arg = render_frame_host;
          return blink::mojom::PermissionStatus::ASK;
        });
    EXPECT_CALL(*permission_controller,
                SubscribeToPermissionResultChange(
                    PermissionDescriptorToPermissionTypeMatcher(
                        blink::PermissionType::NOTIFICATIONS),
                    testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<2>(&rfh_arg_2),
                                 testing::Return(testing::ByMove(
                                     permission_controller->CreateSubscription(
                                         kFirstSubscriptionId)))));

    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kParentUrl));
    testing::Mock::VerifyAndClear(permission_controller);
    EXPECT_EQ(blink::PermissionDescriptorToPermissionType(descriptor),
              blink::PermissionType::NOTIFICATIONS);
    EXPECT_EQ(rfh_arg, web_contents()->GetPrimaryMainFrame());
    ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::ASK);
  }

  base::RepeatingCallback<void(content::PermissionResult)> callback_arg;

  // Navigate to an origin with `PermissionStatus::GRANTED`.
  {
    content::RenderFrameHost* rfh_arg = nullptr;
    content::RenderProcessHost* rph_arg = nullptr;
    blink::mojom::PermissionDescriptorPtr descriptor;

    EXPECT_CALL(*permission_controller, GetPermissionStatusForCurrentDocument)
        .WillOnce([&](const blink::mojom::PermissionDescriptorPtr&
                          permission_descriptor,
                      content::RenderFrameHost* render_frame_host) {
          descriptor = permission_descriptor->Clone();
          rfh_arg = render_frame_host;
          return blink::mojom::PermissionStatus::GRANTED;
        });
    EXPECT_CALL(*permission_controller,
                UnsubscribeFromPermissionResultChange(kFirstSubscriptionId));
    EXPECT_CALL(*permission_controller,
                SubscribeToPermissionResultChange(
                    PermissionDescriptorToPermissionTypeMatcher(
                        blink::PermissionType::NOTIFICATIONS),
                    testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<1>(&rph_arg), testing::SaveArg<5>(&callback_arg),
            testing::Return(
                testing::ByMove(permission_controller->CreateSubscription(
                    kSecondSubscriptionId)))));

    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kCousinFreddyUrl));
    testing::Mock::VerifyAndClear(permission_controller);
    EXPECT_EQ(blink::PermissionDescriptorToPermissionType(descriptor),
              blink::PermissionType::NOTIFICATIONS);
    EXPECT_EQ(rfh_arg, web_contents()->GetPrimaryMainFrame());
    ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  }

  // Simulate a change of permission status independent from navigation.
  callback_arg.Run(
      content::PermissionResult(blink::mojom::PermissionStatus::DENIED));
  ExpectNotificationPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  // The last subscription is removed when the tab helper is deleted.
  EXPECT_CALL(*permission_controller,
              UnsubscribeFromPermissionResultChange(kSecondSubscriptionId));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PerformanceManagerTabHelperTest, GetFrameNode) {
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

TEST_F(PerformanceManagerTabHelperTest,
       NotificationsFromInactiveFrameTreeAreIgnored) {
  // When this feature is enabled, PerformanceManagerTabHelper does not ignore
  // the first favicon/title update.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUseLoadingStateToDetectBackgroundTitleOrFaviconUpdate);

  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  auto* first_nav_main_rfh = web_contents()->GetPrimaryMainFrame();

  content::LeaveInPendingDeletionState(first_nav_main_rfh);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kCousinFreddyUrl));
  EXPECT_NE(web_contents()->GetPrimaryMainFrame(), first_nav_main_rfh);

  // Mock observer.
  Graph* graph = PerformanceManager::GetGraph();

  MockPageNodeObserver observer;
  graph->AddPageNodeObserver(&observer);

  auto* tab_helper =
      PerformanceManagerTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  tab_helper->DidUpdateFaviconURL(
      first_nav_main_rfh, {},
      blink::mojom::FaviconUpdateReason::kLinkElementChange);

  // The observer shouldn't have been called at this point.
  testing::Mock::VerifyAndClear(&observer);
  // Set the expectation for the next check.
  EXPECT_CALL(observer, OnFaviconUpdated(::testing::_, ::testing::_));

  // Sanity check to ensure that notification sent to the active main frame are
  // forwarded.
  tab_helper->DidUpdateFaviconURL(
      web_contents()->GetPrimaryMainFrame(), {},
      blink::mojom::FaviconUpdateReason::kLinkElementChange);

  testing::Mock::VerifyAndClear(&observer);
  graph->RemovePageNodeObserver(&observer);
}

namespace {

bool IsUserOrBrowserInitiatedLoad(content::WebContents* web_contents) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);
  return page_node->IsUserOrBrowserInitiatedLoad();
}

// Longer than the window during which a renderer-initiated navigation is
// treated as a client redirect of the current document.
constexpr base::TimeDelta kLongerThanClientRedirectWindow = base::Seconds(11);

// Shorter than that window.
constexpr base::TimeDelta kShorterThanClientRedirectWindow = base::Seconds(5);

class PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest
    : public PerformanceManagerTabHelperTest {
 protected:
  void SetUp() override {
    PerformanceManagerTabHelperTest::SetUp();
    SetContents(CreateTestWebContents());
    PerformanceManagerTabHelper::FromWebContents(web_contents())
        ->SetTickClockForTesting(&tick_clock_);
  }

  base::SimpleTestTickClock tick_clock_;
};

}  // namespace

TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoad) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  tick_clock_.Advance(kLongerThanClientRedirectWindow);

  // A renderer-initiated navigation without user activation is not
  // user-initiated, as soon as it starts.
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A renderer-initiated navigation with user activation is user-initiated.
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild2Url), main_rfh());
  navigation->SetHasUserGesture(true);
  navigation->Start();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A browser-initiated navigation is user-initiated, even without a gesture.
  tick_clock_.Advance(kLongerThanClientRedirectWindow);
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kGrandchildUrl), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kCousinFreddyUrl), web_contents());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoadRevertsOnAbort) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  tick_clock_.Advance(kLongerThanClientRedirectWindow);

  // An aborted navigation restores the value of the current document.
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->AbortFromRenderer();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

// Tests that when navigations overlap, the page follows the most recently
// started one, even if an older one commits in the meantime.
TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoadFollowsLatestNavigation) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  tick_clock_.Advance(kLongerThanClientRedirectWindow);

  // An unsolicited renderer-initiated navigation gets ready to commit.
  auto older_navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), main_rfh());
  older_navigation->SetHasUserGesture(false);
  older_navigation->ReadyToCommit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A browser-initiated navigation starts before the older one commits.
  auto newer_navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kChild2Url), web_contents());
  newer_navigation->Start();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // The older navigation committing doesn't affect the newer navigation's
  // value.
  older_navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // When the newer navigation doesn't commit, the page reverts to the value of
  // the document committed by the older navigation.
  newer_navigation->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

// Tests that when the newest of overlapping navigations fails while an older
// one is still pending, the page follows the older one.
TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoadFallsBackToOlderPendingNavigation) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  tick_clock_.Advance(kLongerThanClientRedirectWindow);

  // An unsolicited renderer-initiated navigation gets ready to commit.
  auto older_navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), main_rfh());
  older_navigation->SetHasUserGesture(false);
  older_navigation->ReadyToCommit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A browser-initiated navigation starts, then fails while the older one is
  // still pending.
  auto newer_navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kChild2Url), web_contents());
  newer_navigation->Start();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  newer_navigation->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));

  older_navigation->Commit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoadIgnoresSameDocumentAndSubframes) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  tick_clock_.Advance(kLongerThanClientRedirectWindow);

  // A same-document navigation without user activation doesn't load a page.
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(std::string(kParentUrl) + "#ref"), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->CommitSameDocument();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // Neither does a subframe navigation.
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), child);
  navigation->SetHasUserGesture(false);
  navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

// Tests that a renderer-initiated navigation without user activation that
// starts shortly after a user- or browser-initiated document committed is
// treated as a client redirect, and inherits that document's value. This also
// applies to each hop of a chain of client redirects.
TEST_F(PerformanceManagerTabHelperUserOrBrowserInitiatedLoadTest,
       UserOrBrowserInitiatedLoadInheritedByClientRedirects) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kParentUrl));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // First hop.
  tick_clock_.Advance(kShorterThanClientRedirectWindow);
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild1Url), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // Second hop. The window restarts when each hop commits.
  tick_clock_.Advance(kShorterThanClientRedirectWindow);
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kChild2Url), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A navigation that starts after the window isn't a client redirect.
  tick_clock_.Advance(kLongerThanClientRedirectWindow);
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kGrandchildUrl), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));

  // A client redirect of a document that wasn't user- or browser-initiated
  // inherits that document's value.
  tick_clock_.Advance(kShorterThanClientRedirectWindow);
  navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kCousinFreddyUrl), main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Start();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
  navigation->Commit();
  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(web_contents()));
}

}  // namespace performance_manager
