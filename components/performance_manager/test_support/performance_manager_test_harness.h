// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/dedicated_worker_creator.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_client_info.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace performance_manager {

class GraphImpl;
class FrameNode;
class ProcessNode;
class WorkerNode;

// A test harness that initializes PerformanceManagerImpl, plus the entire
// RenderViewHost harness. Allows for creating full WebContents, and their
// accompanying structures in the graph. The task environment is accessed
// via content::RenderViewHostTestHarness::task_environment().
//
// Meant to be used from components_unittests, but not from unit_tests or
// browser tests. unit_tests should use PerformanceManagerTestHarnessHelper.
//
// To set the active WebContents for the test use:
//
//   SetContents(CreateTestWebContents());
//
// This will create a PageNode, but nothing else. To create FrameNodes and
// ProcessNodes for the test WebContents, simulate a committed navigation with:
//
//  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
//      GURL("https://www.foo.com/"));
//
// This will create nodes backed by thin test stubs (TestWebContents,
// TestRenderFrameHost, etc). If you want full process trees and live
// RenderFrameHosts, use PerformanceManagerBrowserTestHarness.
//
// If you just want to test how code interacts with the graph it's better to
// use GraphTestHarness, which has a rich set of methods for directly creating
// graph nodes.
class PerformanceManagerTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  using Super = content::RenderViewHostTestHarness;

  class DedicatedWorkerFactory;
  class SharedWorkerFactory;
  class ServiceWorkerFactory;

  PerformanceManagerTestHarness();

  // Constructs a PerformanceManagerTestHarness which uses |traits| to
  // initialize its BrowserTaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  explicit PerformanceManagerTestHarness(TaskEnvironmentTraits&&... traits)
      : Super(std::forward<TaskEnvironmentTraits>(traits)...) {
    helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
  }

  PerformanceManagerTestHarness(const PerformanceManagerTestHarness&) = delete;
  PerformanceManagerTestHarness& operator=(
      const PerformanceManagerTestHarness&) = delete;
  ~PerformanceManagerTestHarness() override;

  // Setup returns once the PM is fully initialized and OnGraphCreated has
  // returned.
  void SetUp() override;

  // Teards down the PM. Blocks until it is fully torn down.
  void TearDown() override;

  // Creates a test web contents with performance manager tab helpers
  // attached. This is a test web contents that can be interacted with
  // via WebContentsTester.
  std::unique_ptr<content::WebContents> CreateTestWebContents();

  // Allows a test to cause the PM to be torn down early, so it can explicitly
  // test TearDown logic. This may only be called once, and no other functions
  // (except "TearDown") may be called afterwards.
  void TearDownNow();

  // An additional seam that gets invoked as part of the PM initialization. This
  // will be invoked on the PM sequence as part of "SetUp". This will be called
  // after graph features have been configured (see "GetGraphFeatures").
  virtual void OnGraphCreated(GraphImpl* graph) {}

  // Allows configuring which Graph features are initialized during "SetUp".
  // This defaults to initializing no features.
  GraphFeatures& GetGraphFeatures() { return helper_->GetGraphFeatures(); }

  DedicatedWorkerFactory* dedicated_worker_factory() {
    return dedicated_worker_factory_.get();
  }

  SharedWorkerFactory* shared_worker_factory() {
    return shared_worker_factory_.get();
  }

  ServiceWorkerFactory* service_worker_factory() {
    return service_worker_factory_.get();
  }

 private:
  std::unique_ptr<PerformanceManagerTestHarnessHelper> helper_;

  std::unique_ptr<DedicatedWorkerFactory> dedicated_worker_factory_;
  std::unique_ptr<SharedWorkerFactory> shared_worker_factory_;
  std::unique_ptr<ServiceWorkerFactory> service_worker_factory_;
};

// A test DedicatedWorkerService that allows to simulate creating and destroying
// dedicated workers.
class PerformanceManagerTestHarness::DedicatedWorkerFactory {
 public:
  explicit DedicatedWorkerFactory(
      content::DedicatedWorkerService::Observer* observer);

  DedicatedWorkerFactory(const DedicatedWorkerFactory&) = delete;
  DedicatedWorkerFactory& operator=(const DedicatedWorkerFactory&) = delete;

  ~DedicatedWorkerFactory();

  // Creates a new dedicated worker and returns its ID.
  const blink::DedicatedWorkerToken& CreateDedicatedWorker(
      const ProcessNode* process_node,
      const FrameNode* frame_node,
      const url::Origin& origin = url::Origin());
  const blink::DedicatedWorkerToken& CreateDedicatedWorker(
      const ProcessNode* process_node,
      const WorkerNode* parent_dedicated_worker_node,
      const url::Origin& origin = url::Origin());

  // Destroys an existing dedicated worker.
  void DestroyDedicatedWorker(const blink::DedicatedWorkerToken& token);

 private:
  raw_ptr<content::DedicatedWorkerService::Observer> observer_;

  // Maps each running worker to its creator.
  base::flat_map<blink::DedicatedWorkerToken, content::DedicatedWorkerCreator>
      dedicated_worker_creators_;
};

// A test SharedWorkerService that allows to simulate creating and destroying
// shared workers and adding clients to existing workers.
class PerformanceManagerTestHarness::SharedWorkerFactory {
 public:
  explicit SharedWorkerFactory(
      content::SharedWorkerService::Observer* observer);

  SharedWorkerFactory(const SharedWorkerFactory&) = delete;
  SharedWorkerFactory& operator=(const SharedWorkerFactory&) = delete;

  ~SharedWorkerFactory();

  // Creates a new shared worker and returns its token.
  blink::SharedWorkerToken CreateSharedWorker(
      const ProcessNode* process_node,
      const url::Origin& origin = url::Origin());

  // Destroys a running shared worker.
  void DestroySharedWorker(const blink::SharedWorkerToken& shared_worker_token);

  // Adds a new frame client to an existing worker.
  void AddClient(const blink::SharedWorkerToken& shared_worker_token,
                 content::GlobalRenderFrameHostId client_render_frame_host_id);

  // Removes an existing frame client from a worker.
  void RemoveClient(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId client_render_frame_host_id);

 private:
  raw_ptr<content::SharedWorkerService::Observer> observer_;

  // Contains the set of clients for each running workers.
  base::flat_map<blink::SharedWorkerToken,
                 base::flat_set<content::GlobalRenderFrameHostId>>
      shared_worker_client_frames_;
};

// A test ServiceWorkerContext that allows to simulate a worker starting and
// stopping and adding clients to running workers.
class PerformanceManagerTestHarness::ServiceWorkerFactory {
 public:
  explicit ServiceWorkerFactory(
      content::ServiceWorkerContextObserver* observer);
  ~ServiceWorkerFactory();

  ServiceWorkerFactory(const ServiceWorkerFactory&) = delete;
  ServiceWorkerFactory& operator=(const ServiceWorkerFactory&) = delete;

  // Generates a unique URL for a fake worker node.
  static GURL GenerateWorkerUrl();

  // Generates a unique UUID for a fake worker client.
  static std::string GenerateClientUuid();

  // Creates a new service worker and returns its version ID.
  int64_t CreateServiceWorker();

  // Deletes an existing service worker.
  void DestroyServiceWorker(int64_t version_id);

  // Starts an existing service worker and returns its token.
  blink::ServiceWorkerToken StartServiceWorker(
      int64_t version_id,
      const ProcessNode* process_node,
      const GURL& worker_url = GenerateWorkerUrl(),
      const GURL& scope_url = GURL());

  // Stops a service shared worker.
  void StopServiceWorker(int64_t version_id);

  // Adds a new client to an existing service worker and returns its generated
  // client UUID, or the `client_uuid` passed as an argument. Returns an empty
  // string on failure.
  std::string AddClient(int64_t version_id,
                        const FrameNode* frame_node,
                        std::string client_uuid = GenerateClientUuid());
  std::string AddClient(int64_t version_id,
                        const WorkerNode* worker_node,
                        std::string client_uuid = GenerateClientUuid());

  // Removes an existing client from a worker.
  void RemoveClient(int64_t version_id, const std::string& client_uuid);

  // Simulates when the navigation commits, meaning that the RenderFrameHost is
  // now available for a window client. Not valid for worker clients.
  void OnControlleeNavigationCommitted(int64_t version_id,
                                       const std::string& client_uuid,
                                       const FrameNode* frame_node);

 private:
  // Adds a new client to an existing service worker with the provided
  // client UUID.
  void AddClientImpl(int64_t version_id,
                     std::string client_uuid,
                     const content::ServiceWorkerClientInfo& client_info);

  raw_ptr<content::ServiceWorkerContextObserver> observer_;

  // The ID that the next service worker will be assigned.
  int64_t next_service_worker_instance_id_ = 0;

  struct ServiceWorkerInfo {
    ServiceWorkerInfo();
    ~ServiceWorkerInfo();

    // Move-only.
    ServiceWorkerInfo(const ServiceWorkerInfo& other) = delete;
    ServiceWorkerInfo& operator=(const ServiceWorkerInfo& other) = delete;
    ServiceWorkerInfo(ServiceWorkerInfo&& other);
    ServiceWorkerInfo& operator=(ServiceWorkerInfo&& other);

    bool is_running = false;

    // Contains all the clients
    base::flat_set<std::string /*client_uuid*/> clients;
  };

  base::flat_map<int64_t /*version_id*/, ServiceWorkerInfo>
      service_worker_infos_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
