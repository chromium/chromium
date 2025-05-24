// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <string>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

// Generates a URL in a unique domain, which can be used to create identifiable
// origins or scope URL's for service workers.
GURL GenerateUniqueDomainUrl() {
  static int next_id = 0;
  return GURL(base::StringPrintf("https://www.foo%d.org", next_id++));
}

// Helper function to check that |worker_node| and |client_frame_node| are
// correctly hooked up together.
bool IsWorkerClient(base::WeakPtr<WorkerNode> worker_node,
                    base::WeakPtr<FrameNode> client_frame_node) {
  return base::Contains(worker_node->GetClientFrames(),
                        client_frame_node.get()) &&
         base::Contains(client_frame_node->GetChildWorkerNodes(),
                        worker_node.get());
}

// Helper function to check that |worker_node| and |client_worker_node| are
// correctly hooked up together.
bool IsWorkerClient(base::WeakPtr<WorkerNode> worker_node,
                    base::WeakPtr<WorkerNode> client_worker_node) {
  return base::Contains(worker_node->GetClientWorkers(),
                        client_worker_node.get()) &&
         base::Contains(client_worker_node->GetChildWorkers(),
                        worker_node.get());
}

// Helpers methods for retrieving nodes from the graph.

base::WeakPtr<ProcessNode> GetProcessNode(
    content::RenderProcessHost* render_process_host) {
  return PerformanceManager::GetProcessNodeForRenderProcessHost(
      render_process_host);
}

base::WeakPtr<FrameNode> GetFrameNode(
    content::RenderFrameHost* render_frame_host) {
  return PerformanceManager::GetFrameNodeForRenderFrameHost(render_frame_host);
}

base::WeakPtr<WorkerNode> GetWorkerNode(const blink::WorkerToken& token) {
  return PerformanceManager::GetWorkerNodeForToken(token);
}

}  // namespace

class WorkerWatcherTest : public PerformanceManagerTestHarness {};

// This test creates one dedicated worker with a frame client.
TEST_F(WorkerWatcherTest, SimpleDedicatedWorker) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create the worker.
  const auto origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::DedicatedWorkerToken token =
      dedicated_worker_factory()->CreateDedicatedWorker(
          process_node.get(), client_frame_node.get(), origin);

  base::WeakPtr<WorkerNode> worker_node = GetWorkerNode(token);
  ASSERT_TRUE(worker_node);

  // Check the worker node's properties on the graph.
  Graph* graph = PerformanceManager::GetGraph();
  EXPECT_EQ(worker_node->GetGraph(), graph);
  EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);
  EXPECT_EQ(worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(worker_node->GetOrigin(), origin);
  // Script URL not available until script loads.
  EXPECT_FALSE(worker_node->GetURL().is_valid());
  EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));

  dedicated_worker_factory()->DestroyDedicatedWorker(token);
}

TEST_F(WorkerWatcherTest, NestedDedicatedWorker) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> ancestor_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(ancestor_frame_node);

  // Create the parent worker.
  const auto parent_origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::DedicatedWorkerToken parent_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(
          process_node.get(), ancestor_frame_node.get(), parent_origin);

  base::WeakPtr<WorkerNode> parent_worker_node =
      GetWorkerNode(parent_worker_token);
  ASSERT_TRUE(parent_worker_node);

  // Create the nested worker with an opaque origin derived from the parent
  // origin.
  const auto nested_origin = url::Origin::Resolve(GURL(), parent_origin);
  const blink::DedicatedWorkerToken nested_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(
          process_node.get(), parent_worker_node.get(), nested_origin);

  base::WeakPtr<WorkerNode> nested_worker_node =
      GetWorkerNode(nested_worker_token);
  ASSERT_TRUE(nested_worker_node);

  // Check the worker node's properties on the graph.
  Graph* graph = PerformanceManager::GetGraph();
  EXPECT_EQ(nested_worker_node->GetGraph(), graph);
  EXPECT_EQ(nested_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kDedicated);
  ASSERT_TRUE(process_node);
  EXPECT_EQ(nested_worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(nested_worker_node->GetOrigin(), nested_origin);
  // Script URL not available until script loads.
  EXPECT_FALSE(nested_worker_node->GetURL().is_valid());
  // The ancestor frame is not directly a client of the nested worker.
  EXPECT_FALSE(IsWorkerClient(nested_worker_node, ancestor_frame_node));
  EXPECT_TRUE(IsWorkerClient(nested_worker_node, parent_worker_node));

  // Disconnect and clean up the dedicated workers.
  dedicated_worker_factory()->DestroyDedicatedWorker(nested_worker_token);
  EXPECT_FALSE(nested_worker_node);
  dedicated_worker_factory()->DestroyDedicatedWorker(parent_worker_token);
  EXPECT_FALSE(parent_worker_node);
}

// This test creates one shared worker with one client frame.
TEST_F(WorkerWatcherTest, SimpleSharedWorker) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);
  content::GlobalRenderFrameHostId client_render_frame_host_id =
      main_rfh()->GetGlobalId();

  // Create the worker.
  const auto origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::SharedWorkerToken shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get(), origin);

  // Connect the frame to the worker.
  shared_worker_factory()->AddClient(shared_worker_token,
                                     client_render_frame_host_id);

  // Check expectations on the graph.
  base::WeakPtr<WorkerNode> worker_node = GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(worker_node);

  // Check the worker node's properties on the graph.
  Graph* graph = PerformanceManager::GetGraph();
  EXPECT_EQ(worker_node->GetGraph(), graph);
  EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kShared);
  EXPECT_EQ(worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(worker_node->GetOrigin(), origin);
  // Script URL not available until script loads.
  EXPECT_FALSE(worker_node->GetURL().is_valid());
  EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));

  // Clean up.
  shared_worker_factory()->RemoveClient(shared_worker_token,
                                        client_render_frame_host_id);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
}

// This test creates one service worker with one client frame.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClient) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  const GURL worker_url = ServiceWorkerFactory::GenerateWorkerUrl();
  const GURL scope_url = GenerateUniqueDomainUrl();
  // Make sure origins created from `worker_url` and `scope_url` can't be
  // confused.
  EXPECT_NE(url::Origin::Create(worker_url), url::Origin::Create(scope_url));

  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(
          service_worker_version_id, process_node.get(), worker_url, scope_url);
  base::WeakPtr<WorkerNode> worker_node = GetWorkerNode(service_worker_token);
  ASSERT_TRUE(worker_node);

  // Add a window client of the service worker.
  std::string service_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(service_worker_client_uuid.empty());

  // Check the worker node's properties on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(worker_node->GetGraph(), graph);
  EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kService);
  EXPECT_EQ(worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(worker_node->GetOrigin(), url::Origin::Create(scope_url));
  EXPECT_EQ(worker_node->GetURL(), worker_url);

  // The frame can not be connected to the service worker until its
  // RenderFrameHost is available, which happens when the navigation
  // commits.
  EXPECT_TRUE(worker_node->GetClientFrames().empty());

  // Now simulate the navigation commit.
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      client_frame_node.get());

  // Check the worker node's properties on the graph.
  EXPECT_EQ(worker_node->GetGraph(), graph);
  EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kService);
  EXPECT_EQ(worker_node->GetProcessNode(), process_node.get());
  EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));

  // Disconnect and clean up the service worker.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);

  EXPECT_FALSE(worker_node);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker is (briefly?) an uncommitted client of two versions. This presumably
// happens on version update or some such, or perhaps when a frame is a
// bona-fide client of two service workers. Apparently this happens quite
// rarely in the field.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClientOfTwoWorkers) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create and start both service workers.
  int64_t first_service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  const GURL first_worker_url = ServiceWorkerFactory::GenerateWorkerUrl();
  const GURL first_scope_url = GenerateUniqueDomainUrl();
  blink::ServiceWorkerToken first_service_worker_token =
      service_worker_factory()->StartServiceWorker(
          first_service_worker_version_id, process_node.get(), first_worker_url,
          first_scope_url);
  base::WeakPtr<WorkerNode> first_service_worker_node =
      GetWorkerNode(first_service_worker_token);
  ASSERT_TRUE(first_service_worker_node);

  int64_t second_service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  const GURL second_worker_url = ServiceWorkerFactory::GenerateWorkerUrl();
  const GURL second_scope_url = GenerateUniqueDomainUrl();
  blink::ServiceWorkerToken second_service_worker_token =
      service_worker_factory()->StartServiceWorker(
          second_service_worker_version_id, process_node.get(),
          second_worker_url, second_scope_url);
  base::WeakPtr<WorkerNode> second_service_worker_node =
      GetWorkerNode(second_service_worker_token);
  ASSERT_TRUE(second_service_worker_node);

  // Make sure origins created from `worker_url` and `scope_url` can't be
  // confused.
  EXPECT_NE(url::Origin::Create(first_worker_url),
            url::Origin::Create(first_scope_url));
  EXPECT_NE(url::Origin::Create(second_worker_url),
            url::Origin::Create(second_scope_url));

  // Add a window client of the service worker twice.
  std::string service_worker_client_uuid = service_worker_factory()->AddClient(
      first_service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(service_worker_client_uuid.empty());
  EXPECT_FALSE(service_worker_factory()
                   ->AddClient(second_service_worker_version_id,
                               client_frame_node.get(),
                               service_worker_client_uuid)
                   .empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(first_service_worker_node->GetGraph(), graph);
  EXPECT_EQ(first_service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(first_service_worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(first_service_worker_node->GetOrigin(),
            url::Origin::Create(first_scope_url));
  EXPECT_EQ(first_service_worker_node->GetURL(), first_worker_url);
  // The frame was never added as a client of the service worker.
  EXPECT_TRUE(first_service_worker_node->GetClientFrames().empty());

  EXPECT_EQ(second_service_worker_node->GetGraph(), graph);
  EXPECT_EQ(second_service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(second_service_worker_node->GetProcessNode(), process_node.get());
  EXPECT_EQ(second_service_worker_node->GetOrigin(),
            url::Origin::Create(second_scope_url));
  EXPECT_EQ(second_service_worker_node->GetURL(), second_worker_url);
  // The frame was never added as a client of the service worker.
  EXPECT_TRUE(second_service_worker_node->GetClientFrames().empty());

  // Disconnect and clean up the service worker.
  service_worker_factory()->RemoveClient(first_service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_factory()->StopServiceWorker(first_service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(
      first_service_worker_version_id);
  EXPECT_FALSE(first_service_worker_node);

  service_worker_factory()->RemoveClient(second_service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_factory()->StopServiceWorker(second_service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(
      second_service_worker_version_id);
  EXPECT_FALSE(second_service_worker_node);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker has a double client relationship with a service worker.
// This appears to be happening out in the real world, if quite rarely.
// See https://crbug.com/1143281#c33.
TEST_F(WorkerWatcherTest, ServiceWorkerTwoFrameClientRelationships) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create and start a service worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  // Add a window client of the service worker.
  std::string first_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(first_client_uuid.empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  // The frame was not yet added as a client.
  EXPECT_TRUE(service_worker_node->GetClientFrames().empty());

  // Add a second client relationship between the same two entities.
  std::string second_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(second_client_uuid.empty());

  // Now simulate the navigation commit for the first client relationship.
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, first_client_uuid, client_frame_node.get());

  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(1u, service_worker_node->GetClientFrames().size());
  EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));

  // Now simulate the navigation commit for the second client relationship.
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, second_client_uuid, client_frame_node.get());

  // Verify that the graph is still the same.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(1u, service_worker_node->GetClientFrames().size());
  EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));

  // Remove the first client relationship.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         first_client_uuid);

  // Verify that the graph is still the same.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(1u, service_worker_node->GetClientFrames().size());
  EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));

  // Remove the second client relationship.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         second_client_uuid);

  // Verify that the frame is no longer a client of the service worker.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_TRUE(service_worker_node->GetClientFrames().empty());

  // Teardown.
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker is created but it's navigation is never committed before the
// FrameTreeNode is destroyed.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClientDestroyedBeforeCommit) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  // Add a window client of the service worker.
  std::string service_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(service_worker_client_uuid.empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(service_worker_node->GetProcessNode(), process_node.get());

  // The frame was never added as a client of the service worker.
  EXPECT_TRUE(service_worker_node->GetClientFrames().empty());

  // Disconnect and clean up the service worker.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);
}

TEST_F(WorkerWatcherTest, AllTypesOfServiceWorkerClients) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  std::string frame_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(frame_client_uuid.empty());
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, client_frame_node.get());

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(
          process_node.get(), client_frame_node.get());
  base::WeakPtr<WorkerNode> dedicated_worker_node =
      GetWorkerNode(dedicated_worker_token);
  ASSERT_TRUE(dedicated_worker_node);
  std::string dedicated_worker_client_uuid =
      service_worker_factory()->AddClient(service_worker_version_id,
                                          dedicated_worker_node.get());
  ASSERT_FALSE(dedicated_worker_client_uuid.empty());

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  base::WeakPtr<WorkerNode> shared_worker_node =
      GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(shared_worker_node);
  std::string shared_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, shared_worker_node.get());
  ASSERT_FALSE(shared_worker_client_uuid.empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
  EXPECT_TRUE(IsWorkerClient(service_worker_node, dedicated_worker_node));
  EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));

  // Disconnect and clean up the service worker and its clients.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         shared_worker_client_uuid);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         dedicated_worker_client_uuid);
  dedicated_worker_factory()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         frame_client_uuid);

  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);
}

// Tests that the WorkerWatcher can handle the case where the service worker
// starts after it has been assigned a client. In this case, the clients are not
// connected to the service worker until it starts. It also tests that when the
// service worker stops, its existing clients are also disconnected.
TEST_F(WorkerWatcherTest, ServiceWorkerStartsAndStopsWithExistingClients) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(frame_node);

  // Create the worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  std::string frame_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, frame_node.get());
  ASSERT_FALSE(frame_client_uuid.empty());
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, frame_node.get());

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(process_node.get(),
                                                        frame_node.get());
  base::WeakPtr<WorkerNode> dedicated_worker_node =
      GetWorkerNode(dedicated_worker_token);
  ASSERT_TRUE(dedicated_worker_node);
  std::string dedicated_worker_client_uuid =
      service_worker_factory()->AddClient(service_worker_version_id,
                                          dedicated_worker_node.get());
  ASSERT_FALSE(dedicated_worker_client_uuid.empty());

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  base::WeakPtr<WorkerNode> shared_worker_node =
      GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(shared_worker_node);
  std::string shared_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, shared_worker_node.get());
  ASSERT_FALSE(shared_worker_client_uuid.empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  // The clients exists in the graph but they are not connected to the
  // service worker.
  EXPECT_EQ(frame_node->GetGraph(), graph);
  EXPECT_EQ(dedicated_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetGraph(), graph);

  // Note: Because a dedicated worker is always connected to a frame, this
  // frame node actually has |dedicated_worker_node| as its sole client.
  EXPECT_THAT(frame_node->GetChildWorkerNodes(),
              ::testing::ElementsAre(dedicated_worker_node.get()));
  EXPECT_TRUE(dedicated_worker_node->GetChildWorkers().empty());
  EXPECT_TRUE(shared_worker_node->GetChildWorkers().empty());

  // Now start the service worker.
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  // Check expectations on the graph.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(service_worker_node->GetProcessNode(), process_node.get());

  EXPECT_EQ(frame_node->GetGraph(), graph);
  EXPECT_EQ(dedicated_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetGraph(), graph);

  // Now is it correctly hooked up.
  EXPECT_TRUE(IsWorkerClient(service_worker_node, frame_node));
  EXPECT_TRUE(IsWorkerClient(service_worker_node, dedicated_worker_node));
  EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));

  // Stop the service worker. All the clients will be disconnected.
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);

  // Check expectations on the graph.

  // The clients exists in the graph but they are not connected to the
  // service worker.
  EXPECT_EQ(frame_node->GetGraph(), graph);
  EXPECT_EQ(dedicated_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetGraph(), graph);

  // Note: Because a dedicated worker is always connected to a frame, this
  // frame node actually has |dedicated_worker_node| as its sole client.
  EXPECT_THAT(frame_node->GetChildWorkerNodes(),
              ::testing::ElementsAre(dedicated_worker_node.get()));
  EXPECT_TRUE(dedicated_worker_node->GetChildWorkers().empty());
  EXPECT_TRUE(shared_worker_node->GetChildWorkers().empty());

  // Disconnect and clean up the service worker and its clients
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         shared_worker_client_uuid);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         dedicated_worker_client_uuid);
  dedicated_worker_factory()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         frame_client_uuid);

  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
}

// Tests that the WorkerWatcher can handle the case where the service worker
// starts after it has been assigned a worker client, but the client has
// already died by the time the service worker starts.
TEST_F(WorkerWatcherTest, SharedWorkerStartsWithDeadWorkerClients) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(frame_node);

  // Create the service worker.
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();

  // Create a worker client of each type and connect them to the service worker
  // before starting it.

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(process_node.get(),
                                                        frame_node.get());
  base::WeakPtr<WorkerNode> dedicated_worker_node =
      GetWorkerNode(dedicated_worker_token);
  ASSERT_TRUE(dedicated_worker_node);
  std::string dedicated_worker_client_uuid =
      service_worker_factory()->AddClient(service_worker_version_id,
                                          dedicated_worker_node.get());
  ASSERT_FALSE(dedicated_worker_client_uuid.empty());

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  base::WeakPtr<WorkerNode> shared_worker_node =
      GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(shared_worker_node);
  std::string shared_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, shared_worker_node.get());
  ASSERT_FALSE(shared_worker_client_uuid.empty());

  // Destroy the workers before the service worker starts.
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  EXPECT_FALSE(shared_worker_node);
  dedicated_worker_factory()->DestroyDedicatedWorker(dedicated_worker_token);
  EXPECT_FALSE(dedicated_worker_node);

  // Now start the service worker.
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(service_worker_node->GetProcessNode(), process_node.get());
  EXPECT_TRUE(service_worker_node->GetChildWorkers().empty());

  // Disconnect the non-existent clients.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         shared_worker_client_uuid);
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         dedicated_worker_client_uuid);

  // No changes in the graph.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_EQ(service_worker_node->GetProcessNode(), process_node.get());
  EXPECT_TRUE(service_worker_node->GetChildWorkers().empty());

  // Stop and destroy the service worker.
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);
}

TEST_F(WorkerWatcherTest, SharedWorkerDiesAsServiceWorkerClient) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  ASSERT_TRUE(process_node);

  // Create the shared and service workers.
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  base::WeakPtr<WorkerNode> shared_worker_node =
      GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(shared_worker_node);

  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);
  ASSERT_TRUE(service_worker_node);

  std::string service_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, shared_worker_node.get());
  ASSERT_FALSE(service_worker_client_uuid.empty());

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  EXPECT_EQ(shared_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kShared);
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));

  // Destroy the shared worker while it still has a client registration
  // against the service worker.
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  ASSERT_FALSE(shared_worker_node);
  ASSERT_TRUE(service_worker_node);

  // Check expectations on the graph again.
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetWorkerType(),
            WorkerNode::WorkerType::kService);
  EXPECT_TRUE(service_worker_node->GetClientWorkers().empty());

  // Issue the trailing service worker client removal.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);

  // Stop and destroy the service worker.
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  EXPECT_FALSE(service_worker_node);
}

TEST_F(WorkerWatcherTest, OneSharedWorkerTwoClients) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node_1 = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node_1 = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node_1);
  ASSERT_TRUE(client_frame_node_1);
  content::GlobalRenderFrameHostId client_render_frame_host_id_1 =
      main_rfh()->GetGlobalId();

  auto web_contents_2 = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents_2.get(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node_2 =
      GetProcessNode(web_contents_2->GetPrimaryMainFrame()->GetProcess());
  base::WeakPtr<FrameNode> client_frame_node_2 =
      GetFrameNode(web_contents_2->GetPrimaryMainFrame());
  ASSERT_TRUE(process_node_2);
  ASSERT_TRUE(client_frame_node_2);
  content::GlobalRenderFrameHostId client_render_frame_host_id_2 =
      web_contents_2->GetPrimaryMainFrame()->GetGlobalId();

  ASSERT_NE(client_frame_node_1.get(), client_frame_node_2.get());

  // Create the worker.
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node_1.get());
  base::WeakPtr<WorkerNode> worker_node = GetWorkerNode(shared_worker_token);
  ASSERT_TRUE(worker_node);

  // Create 2 client frame nodes and connect them to the worker.
  shared_worker_factory()->AddClient(shared_worker_token,
                                     client_render_frame_host_id_1);
  shared_worker_factory()->AddClient(shared_worker_token,
                                     client_render_frame_host_id_2);

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();
  EXPECT_EQ(worker_node->GetGraph(), graph);
  EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kShared);

  // Check frame 1.
  EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_1));

  // Check frame 2.
  EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_2));

  // Disconnect and clean up the shared worker.
  shared_worker_factory()->RemoveClient(shared_worker_token,
                                        client_render_frame_host_id_1);
  shared_worker_factory()->RemoveClient(shared_worker_token,
                                        client_render_frame_host_id_2);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  EXPECT_FALSE(worker_node);
}

TEST_F(WorkerWatcherTest, OneClientTwoSharedWorkers) {
  SetContents(CreateTestWebContents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);
  content::GlobalRenderFrameHostId client_render_frame_host_id =
      main_rfh()->GetGlobalId();

  // Create the 2 workers and connect them to the frame.
  const blink::SharedWorkerToken& shared_worker_token_1 =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  shared_worker_factory()->AddClient(shared_worker_token_1,
                                     client_render_frame_host_id);
  base::WeakPtr<WorkerNode> shared_worker_node_1 =
      GetWorkerNode(shared_worker_token_1);
  ASSERT_TRUE(shared_worker_node_1);

  const blink::SharedWorkerToken& shared_worker_token_2 =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  shared_worker_factory()->AddClient(shared_worker_token_2,
                                     client_render_frame_host_id);
  base::WeakPtr<WorkerNode> shared_worker_node_2 =
      GetWorkerNode(shared_worker_token_2);
  ASSERT_TRUE(shared_worker_node_2);

  // Check expectations on the graph.
  Graph* graph = PerformanceManager::GetGraph();

  // Check worker 1.
  EXPECT_EQ(shared_worker_node_1->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node_1->GetWorkerType(),
            WorkerNode::WorkerType::kShared);
  EXPECT_TRUE(IsWorkerClient(shared_worker_node_1, client_frame_node));

  // Check worker 2.
  EXPECT_EQ(shared_worker_node_2->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node_2->GetWorkerType(),
            WorkerNode::WorkerType::kShared);
  EXPECT_TRUE(IsWorkerClient(shared_worker_node_2, client_frame_node));

  // Disconnect and clean up the shared workers.
  shared_worker_factory()->RemoveClient(shared_worker_token_1,
                                        client_render_frame_host_id);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token_1);
  EXPECT_FALSE(shared_worker_node_1);

  shared_worker_factory()->RemoveClient(shared_worker_token_2,
                                        client_render_frame_host_id);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token_2);
  EXPECT_FALSE(shared_worker_node_2);
}

TEST_F(WorkerWatcherTest, FrameDestroyed) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  // This test uses a navigation to destroy the frame. The BackForwardCache
  // would prevent that.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  base::WeakPtr<ProcessNode> process_node = GetProcessNode(process());
  base::WeakPtr<FrameNode> client_frame_node = GetFrameNode(main_rfh());
  ASSERT_TRUE(process_node);
  ASSERT_TRUE(client_frame_node);
  content::GlobalRenderFrameHostId render_frame_host_id =
      main_rfh()->GetGlobalId();

  // Create a worker of each type.
  const blink::DedicatedWorkerToken& dedicated_worker_token =
      dedicated_worker_factory()->CreateDedicatedWorker(
          process_node.get(), client_frame_node.get());
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_factory()->CreateSharedWorker(process_node.get());
  int64_t service_worker_version_id =
      service_worker_factory()->CreateServiceWorker();
  blink::ServiceWorkerToken service_worker_token =
      service_worker_factory()->StartServiceWorker(service_worker_version_id,
                                                   process_node.get());

  // Connect the frame to the shared worker and the service worker. Note that it
  // is already connected to the dedicated worker.
  shared_worker_factory()->AddClient(shared_worker_token, render_frame_host_id);
  std::string service_worker_client_uuid = service_worker_factory()->AddClient(
      service_worker_version_id, client_frame_node.get());
  ASSERT_FALSE(service_worker_client_uuid.empty());
  service_worker_factory()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      client_frame_node.get());

  // Check that everything is wired up correctly.
  Graph* graph = PerformanceManager::GetGraph();

  base::WeakPtr<WorkerNode> dedicated_worker_node =
      GetWorkerNode(dedicated_worker_token);
  base::WeakPtr<WorkerNode> shared_worker_node =
      GetWorkerNode(shared_worker_token);
  base::WeakPtr<WorkerNode> service_worker_node =
      GetWorkerNode(service_worker_token);

  ASSERT_TRUE(dedicated_worker_node);
  ASSERT_TRUE(shared_worker_node);
  ASSERT_TRUE(service_worker_node);

  EXPECT_EQ(dedicated_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_TRUE(IsWorkerClient(dedicated_worker_node, client_frame_node));
  EXPECT_TRUE(IsWorkerClient(shared_worker_node, client_frame_node));
  EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));

  // Navigate the main frame to destroy it. But keep the renderer alive
  content::RenderProcessHost* render_process_host = process();
  render_process_host->AddPendingView();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.bar.com/"));
  ASSERT_FALSE(client_frame_node);

  // Check that the workers are no longer connected to the deleted frame.
  ASSERT_TRUE(dedicated_worker_node);
  ASSERT_TRUE(shared_worker_node);
  ASSERT_TRUE(service_worker_node);
  EXPECT_EQ(dedicated_worker_node->GetGraph(), graph);
  EXPECT_EQ(shared_worker_node->GetGraph(), graph);
  EXPECT_EQ(service_worker_node->GetGraph(), graph);
  EXPECT_TRUE(dedicated_worker_node->GetClientFrames().empty());
  EXPECT_TRUE(shared_worker_node->GetClientFrames().empty());
  EXPECT_TRUE(service_worker_node->GetClientFrames().empty());

  // Clean up. The watcher is still expecting a worker removed notification.
  service_worker_factory()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_factory()->StopServiceWorker(service_worker_version_id);
  service_worker_factory()->DestroyServiceWorker(service_worker_version_id);
  shared_worker_factory()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_factory()->DestroySharedWorker(shared_worker_token);
  dedicated_worker_factory()->DestroyDedicatedWorker(dedicated_worker_token);

  EXPECT_FALSE(dedicated_worker_node);
  EXPECT_FALSE(shared_worker_node);
  EXPECT_FALSE(service_worker_node);

  render_process_host->RemovePendingView();
}

}  // namespace performance_manager
