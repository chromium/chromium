// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph/mock_frame_node_observer.h"
#include "components/performance_manager/test_support/graph/mock_page_node_observer.h"
#include "components/performance_manager/test_support/graph/mock_process_node_observer.h"
#include "components/performance_manager/test_support/graph/mock_worker_node_observer.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/test_browser_child_process.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

class PerformanceManagerTest : public PerformanceManagerTestHarness {
 public:
  using Super = PerformanceManagerTestHarness;

  PerformanceManagerTest() = default;
  ~PerformanceManagerTest() override = default;

  PerformanceManagerTest(const PerformanceManagerTest&) = delete;
  PerformanceManagerTest& operator=(const PerformanceManagerTest&) = delete;

  void SetUp() override {
    EXPECT_FALSE(PerformanceManager::IsAvailable());
    Super::SetUp();
    EXPECT_TRUE(PerformanceManager::IsAvailable());
  }

  void TearDown() override {
    EXPECT_TRUE(PerformanceManager::IsAvailable());
    Super::TearDown();
    EXPECT_FALSE(PerformanceManager::IsAvailable());
  }

 protected:
  // Helper functions to validate observed nodes against PerformanceManager
  // accessors.
  void ValidateFrameNode(
      const FrameNode* node,
      bool expect_lookup_success,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->GetRenderFrameHostProxy().is_valid());
    base::WeakPtr<FrameNode> looked_up_node =
        PerformanceManager::GetFrameNodeForRenderFrameHost(
            node->GetRenderFrameHostProxy().Get());
    if (expect_lookup_success) {
      EXPECT_EQ(looked_up_node.get(), node);
    } else {
      EXPECT_FALSE(looked_up_node);
    }
  }

  void ValidatePageNode(
      const PageNode* node,
      bool expect_lookup_success,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->GetWebContents());
    base::WeakPtr<PageNode> looked_up_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(
            node->GetWebContents().get());
    if (expect_lookup_success) {
      EXPECT_EQ(looked_up_node.get(), node);
    } else {
      EXPECT_FALSE(looked_up_node);
    }
  }

  void ValidateProcessNode(
      const ProcessNode* node,
      bool expect_lookup_success,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    ASSERT_TRUE(node);
    base::WeakPtr<ProcessNode> looked_up_node;
    switch (node->GetProcessType()) {
      case content::PROCESS_TYPE_BROWSER:
        looked_up_node = PerformanceManager::GetProcessNodeForBrowserProcess();
        break;
      case content::PROCESS_TYPE_RENDERER:
        ASSERT_TRUE(node->GetRenderProcessHostProxy().is_valid());
        looked_up_node = PerformanceManager::GetProcessNodeForRenderProcessHost(
            node->GetRenderProcessHostProxy().Get());
        break;
      default:
        ASSERT_TRUE(node->GetBrowserChildProcessHostProxy().is_valid());
        looked_up_node =
            PerformanceManager::GetProcessNodeForBrowserChildProcessHost(
                node->GetBrowserChildProcessHostProxy().Get());
        break;
    }
    if (expect_lookup_success) {
      EXPECT_EQ(looked_up_node.get(), node);
    } else {
      EXPECT_FALSE(looked_up_node);
    }
  }
};

void ValidateWorkerNode(
    const WorkerNode* node,
    bool expect_lookup_success,
    const base::Location& location = base::Location::Current()) {
  SCOPED_TRACE(location.ToString());
  ASSERT_TRUE(node);
  base::WeakPtr<WorkerNode> looked_up_node =
      PerformanceManager::GetWorkerNodeForToken(node->GetWorkerToken());
  if (expect_lookup_success) {
    EXPECT_EQ(looked_up_node.get(), node);
  } else {
    EXPECT_FALSE(looked_up_node);
  }
}

// Installs observers before SetUp() so they can observe the browser ProcessNode
// creation.
class PerformanceManagerBrowserNodeTest : public PerformanceManagerTest {
 public:
  using Super = PerformanceManagerTest;

  PerformanceManagerBrowserNodeTest() = default;
  ~PerformanceManagerBrowserNodeTest() override = default;

  PerformanceManagerBrowserNodeTest(const PerformanceManagerBrowserNodeTest&) =
      delete;
  PerformanceManagerBrowserNodeTest& operator=(
      const PerformanceManagerBrowserNodeTest&) = delete;

  void OnGraphCreated(GraphImpl* graph) override {
    browser_process_observation_.Observe(graph);

    // TODO(crbug.com/40755583): See the comment in
    // PerformanceManagerTest.LookupNodesFromObservers for the ideal behaviour.

    EXPECT_CALL(browser_process_observer_, OnBeforeProcessNodeAdded(_))
        .WillOnce(Invoke([this](const ProcessNode* process_node) {
          observed_browser_process_node_ = process_node;
          ValidateProcessNode(process_node, false);
        }));
    EXPECT_CALL(browser_process_observer_, OnProcessNodeAdded(_))
        .WillOnce(Invoke([&](const ProcessNode* process_node) {
          EXPECT_EQ(process_node, observed_browser_process_node_);
          // TODO(crbug.com/40755583): Should be true.
          ValidateProcessNode(process_node, false);
        }));

    Super::OnGraphCreated(graph);
  }

  void TearDown() override {
    browser_process_observation_.Reset();
    Super::TearDown();
  }

 protected:
  ::testing::NiceMock<LenientMockProcessNodeObserver> browser_process_observer_;
  raw_ptr<const ProcessNode> observed_browser_process_node_ = nullptr;

 private:
  base::ScopedObservation<Graph, ProcessNodeObserver>
      browser_process_observation_{&browser_process_observer_};
};

TEST_F(PerformanceManagerTest, NodeAccessors) {
  auto contents = CreateTestWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderProcessHost* rph = rfh->GetProcess();
  ASSERT_TRUE(rph);

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents.get());

  // FrameNode's and ProcessNode's don't exist until an observer fires on
  // navigation. Verify that looking them up before that returns null instead
  // of crashing.
  EXPECT_FALSE(PerformanceManager::GetFrameNodeForRenderFrameHost(rfh));
  EXPECT_FALSE(PerformanceManager::GetProcessNodeForRenderProcessHost(rph));

  // Simulate a committed navigation to create the nodes.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents.get(), GURL("https://www.example.com/"));
  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);

  EXPECT_TRUE(page_node);
  EXPECT_TRUE(frame_node);
  EXPECT_TRUE(process_node);

  EXPECT_EQ(contents.get(), page_node->GetWebContents().get());
  EXPECT_EQ(rfh, frame_node->GetRenderFrameHostProxy().Get());
  EXPECT_EQ(rph, process_node->GetRenderProcessHostProxy().Get());

  contents.reset();

  EXPECT_FALSE(page_node);
  EXPECT_FALSE(frame_node);
  EXPECT_FALSE(process_node);
}

// Tests that the PerformanceManager accessors to look up nodes don't crash
// when called from observers as the nodes are being added or removed from the
// graph.
TEST_F(PerformanceManagerTest, LookupNodesFromObservers) {
  // Use lenient mocks because we only care about added/removed notifications.
  ::testing::NiceMock<LenientMockFrameNodeObserver> frame_observer;
  ::testing::NiceMock<LenientMockPageNodeObserver> page_observer;
  ::testing::NiceMock<LenientMockProcessNodeObserver> process_observer;
  ::testing::NiceMock<LenientMockWorkerNodeObserver> worker_observer;
  base::ScopedObservation<Graph, FrameNodeObserver> frame_observation{
      &frame_observer};
  base::ScopedObservation<Graph, PageNodeObserver> page_observation{
      &page_observer};
  base::ScopedObservation<Graph, ProcessNodeObserver> process_observation{
      &process_observer};
  base::ScopedObservation<Graph, WorkerNodeObserver> worker_observation{
      &worker_observer};
  frame_observation.Observe(PerformanceManager::GetGraph());
  page_observation.Observe(PerformanceManager::GetGraph());
  process_observation.Observe(PerformanceManager::GetGraph());
  worker_observation.Observe(PerformanceManager::GetGraph());

  const FrameNode* observed_frame_node = nullptr;
  const PageNode* observed_page_node = nullptr;
  std::map<content::ProcessType, const ProcessNode*> observed_process_nodes;
  std::map<WorkerNode::WorkerType, const WorkerNode*> observed_worker_nodes;

  // Don't care about what order the nodes are created in. Call the QuitClosure
  // when the first 4 (Frame, Page, and two Processes) are added to the graph.
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(4, task_environment()->QuitClosure());

  // TODO(crbug.com/40755583): Ideally nodes should not be returned from
  // PerformanceManager accessors during OnBeforeNodeAdded, because they're not
  // in the graph yet. They should be returned from PerformanceManager accessors
  // during OnNodeAdded because they're now in the graph. But the actual
  // behaviour is inconsistent. For now this test passes as long as none of the
  // accessors crash when called from any observer.

  EXPECT_CALL(frame_observer, OnBeforeFrameNodeAdded(_, _, _, _, _))
      .WillOnce(WithArg<0>(Invoke([&](const FrameNode* frame_node) {
        observed_frame_node = frame_node;
        // TODO(crbug.com/40755583): Should be false.
        ValidateFrameNode(frame_node, true);
      })));
  EXPECT_CALL(frame_observer, OnFrameNodeAdded(_))
      .WillOnce(Invoke([&](const FrameNode* frame_node) {
        EXPECT_EQ(frame_node, observed_frame_node);
        ValidateFrameNode(frame_node, true);
        quit_closure.Run();
      }));

  EXPECT_CALL(page_observer, OnBeforePageNodeAdded(_))
      .WillOnce(Invoke([&](const PageNode* page_node) {
        observed_page_node = page_node;
        ValidatePageNode(page_node, false);
      }));
  EXPECT_CALL(page_observer, OnPageNodeAdded(_))
      .WillOnce(Invoke([&](const PageNode* page_node) {
        EXPECT_EQ(page_node, observed_page_node);
        // TODO(crbug.com/40755583): Should be true.
        ValidatePageNode(page_node, false);
        quit_closure.Run();
      }));

  EXPECT_CALL(process_observer, OnBeforeProcessNodeAdded(_))
      .WillRepeatedly(Invoke([&](const ProcessNode* process_node) {
        observed_process_nodes[process_node->GetProcessType()] = process_node;
        ValidateProcessNode(process_node, false);
      }));
  EXPECT_CALL(process_observer, OnProcessNodeAdded(_))
      .WillRepeatedly(Invoke([&](const ProcessNode* process_node) {
        EXPECT_EQ(process_node,
                  observed_process_nodes[process_node->GetProcessType()]);
        // TODO(crbug.com/40755583): Should be true.
        ValidateProcessNode(process_node, false);
        quit_closure.Run();
      }));

  // Create a new page and simulate a committed navigation to create a FrameNode
  // and renderer ProcessNode.
  auto web_contents = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.example.com/"));

  // Create a utility ProcessNode.
  TestBrowserChildProcess utility_process(
      content::ProcessType::PROCESS_TYPE_UTILITY);
  utility_process.SimulateLaunch();

  // Wait for the observers to fire.
  task_environment()->RunUntilQuit();

  const ProcessNode* renderer_process_node =
      observed_process_nodes[content::PROCESS_TYPE_RENDERER];
  EXPECT_EQ(
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents.get())
          .get(),
      observed_page_node);
  EXPECT_EQ(PerformanceManager::GetFrameNodeForRenderFrameHost(
                web_contents->GetPrimaryMainFrame())
                .get(),
            observed_frame_node);
  EXPECT_EQ(PerformanceManager::GetProcessNodeForRenderProcessHost(
                web_contents->GetPrimaryMainFrame()->GetProcess())
                .get(),
            renderer_process_node);
  EXPECT_EQ(PerformanceManager::GetProcessNodeForBrowserChildProcessHost(
                utility_process.host())
                .get(),
            observed_process_nodes[content::PROCESS_TYPE_UTILITY]);

  // Now that the frame and renderer process exist, create 3 workers, one of
  // each type.
  quit_closure = base::BarrierClosure(3, task_environment()->QuitClosure());

  EXPECT_CALL(worker_observer, OnBeforeWorkerNodeAdded(_, _))
      .WillRepeatedly(WithArg<0>(Invoke([&](const WorkerNode* worker_node) {
        observed_worker_nodes[worker_node->GetWorkerType()] = worker_node;
        ValidateWorkerNode(worker_node, false);
      })));
  EXPECT_CALL(worker_observer, OnWorkerNodeAdded(_))
      .WillRepeatedly(Invoke([&](const WorkerNode* worker_node) {
        EXPECT_EQ(worker_node,
                  observed_worker_nodes[worker_node->GetWorkerType()]);
        // TODO(crbug.com/40755583): Should be true.
        ValidateWorkerNode(worker_node, false);
        quit_closure.Run();
      }));

  const blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(renderer_process_node,
                                                        observed_frame_node);
  const blink::SharedWorkerToken shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(renderer_process_node);
  const int64_t service_worker_id =
      service_worker_factory()->CreateServiceWorker();
  const blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_id,
                                                   renderer_process_node);

  // Wait for the observers to fire.
  task_environment()->RunUntilQuit();

  EXPECT_EQ(
      PerformanceManager::GetWorkerNodeForToken(dedicated_worker_token).get(),
      observed_worker_nodes[WorkerNode::WorkerType::kDedicated]);
  EXPECT_EQ(
      PerformanceManager::GetWorkerNodeForToken(shared_worker_token).get(),
      observed_worker_nodes[WorkerNode::WorkerType::kShared]);
  EXPECT_EQ(
      PerformanceManager::GetWorkerNodeForToken(service_worker_token).get(),
      observed_worker_nodes[WorkerNode::WorkerType::kService]);

  // Don't care about what order the nodes are destroyed in. Call the
  // QuitClosure when all 7 (Frame, Page, two Processes, three Workers) are
  // removed from the graph.
  quit_closure = base::BarrierClosure(7, task_environment()->QuitClosure());

  // TODO(crbug.com/40755583): Ideally nodes should be returned from
  // PerformanceManager accessors during OnBeforeNodeRemoved, because they're
  // still in the graph, but not during OnNodeRemoved, because they've already
  // been removed from the graph.

  EXPECT_CALL(frame_observer, OnBeforeFrameNodeRemoved(_))
      .WillOnce(Invoke([&](const FrameNode* frame_node) {
        EXPECT_EQ(frame_node, observed_frame_node);
        // TODO(crbug.com/40755583): Should be true.
        ValidateFrameNode(frame_node, false);
      }));
  EXPECT_CALL(frame_observer, OnFrameNodeRemoved(_, _, _, _, _))
      .WillOnce(WithArg<0>(Invoke([&](const FrameNode* frame_node) {
        EXPECT_EQ(frame_node, observed_frame_node);
        ValidateFrameNode(frame_node, false);
        quit_closure.Run();
      })));

  EXPECT_CALL(page_observer, OnBeforePageNodeRemoved(_))
      .WillOnce(Invoke([&](const PageNode* page_node) {
        EXPECT_EQ(page_node, observed_page_node);
        // TODO(crbug.com/40755583): Should be true.
        ValidatePageNode(page_node, false);
      }));
  EXPECT_CALL(page_observer, OnPageNodeRemoved(_))
      .WillOnce(Invoke([&](const PageNode* page_node) {
        EXPECT_EQ(page_node, observed_page_node);
        ValidatePageNode(page_node, false);
        quit_closure.Run();
      }));

  EXPECT_CALL(process_observer, OnBeforeProcessNodeRemoved(_))
      .WillRepeatedly(Invoke([&](const ProcessNode* process_node) {
        EXPECT_EQ(process_node,
                  observed_process_nodes[process_node->GetProcessType()]);
        // TODO(crbug.com/40755583): Should be true.
        ValidateProcessNode(process_node, false);
      }));
  EXPECT_CALL(process_observer, OnProcessNodeRemoved(_))
      .WillRepeatedly(Invoke([&](const ProcessNode* process_node) {
        EXPECT_EQ(process_node,
                  observed_process_nodes[process_node->GetProcessType()]);
        ValidateProcessNode(process_node, false);
        quit_closure.Run();
      }));

  EXPECT_CALL(worker_observer, OnBeforeWorkerNodeRemoved(_))
      .WillRepeatedly(Invoke([&](const WorkerNode* worker_node) {
        EXPECT_EQ(worker_node,
                  observed_worker_nodes[worker_node->GetWorkerType()]);
        // TODO(crbug.com/40755583): Should be true.
        ValidateWorkerNode(worker_node, false);
      }));
  EXPECT_CALL(worker_observer, OnWorkerNodeRemoved(_, _))
      .WillRepeatedly(WithArg<0>(Invoke([&](const WorkerNode* worker_node) {
        EXPECT_EQ(worker_node,
                  observed_worker_nodes[worker_node->GetWorkerType()]);
        ValidateWorkerNode(worker_node, false);
        quit_closure.Run();
      })));

  dedicated_worker_factory()->DestroyDedicatedWorker(dedicated_worker_token);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  service_worker_factory()->StopServiceWorker(service_worker_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_id);

  utility_process.SimulateDisconnect();

  // Destroying the WebContents also destroys the contained frame and
  // process.
  web_contents.reset();

  // Wait for the observers to fire.
  task_environment()->RunUntilQuit();
}

TEST_F(PerformanceManagerBrowserNodeTest, LookupBrowserProcessFromObservers) {
  // The observer and expectations for creating the browser ProcessNode are
  // installed in OnGraphCreated().

  EXPECT_EQ(PerformanceManager::GetProcessNodeForBrowserProcess().get(),
            observed_browser_process_node_);

  base::OnceClosure quit_closure = task_environment()->QuitClosure();

  // TODO(crbug.com/40755583): See the comment in
  // PerformanceManagerTest.LookupNodesFromObservers for the ideal behaviour.

  EXPECT_CALL(browser_process_observer_, OnBeforeProcessNodeRemoved(_))
      .WillOnce(Invoke([&](const ProcessNode* process_node) {
        EXPECT_EQ(process_node, observed_browser_process_node_);
        // TODO(crbug.com/40755583): Should be true.
        ValidateProcessNode(process_node, false);
      }));
  EXPECT_CALL(browser_process_observer_, OnProcessNodeRemoved(_))
      .WillOnce(Invoke([&](const ProcessNode* process_node) {
        EXPECT_EQ(process_node, observed_browser_process_node_);
        ValidateProcessNode(process_node, false);
        // Avoid dangling raw_ptr.
        observed_browser_process_node_ = nullptr;
        std::move(quit_closure).Run();
      }));

  DeleteBrowserProcessNodeForTesting();

  // Wait for the observers to fire.
  task_environment()->RunUntilQuit();
}

}  // namespace performance_manager
