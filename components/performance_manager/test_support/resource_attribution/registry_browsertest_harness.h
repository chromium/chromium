// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_

#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/global_routing_id.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace performance_manager {
class Graph;
}

namespace performance_manager::resource_attribution {

// A test harness that creates PM nodes to test with ResourceContext registry
// classes. By default this also enables the registries in GraphFeatures.
class RegistryBrowserTestHarness : public PerformanceManagerBrowserTestHarness {
 public:
  using Super = PerformanceManagerBrowserTestHarness;

  explicit RegistryBrowserTestHarness(bool enable_registries = true);
  ~RegistryBrowserTestHarness() override;

  RegistryBrowserTestHarness(const RegistryBrowserTestHarness&) = delete;
  RegistryBrowserTestHarness& operator=(const RegistryBrowserTestHarness&) =
      delete;

  // Gets a pointer to the given Registry class and passes it to `function` on
  // the PM sequence, blocking the main thread until `function` is executed. If
  // the registry is not enabled, `function` will be called with nullptr.
  template <typename Registry>
  static void RunInGraphWithRegistry(
      base::FunctionRef<void(const Registry*)> function);

  // Convenience function to return the default WebContents.
  content::WebContents* web_contents() const { return shell()->web_contents(); }

  // Returns a PageContext for the default WebContents without using the
  // PageContextRegistry.
  ResourceContext GetWebContentsPageContext() const;

 protected:
  // Creates a set of PM nodes for the test. By default this creates one
  // PageNode with two FrameNodes (a main frame and a subframe), each with their
  // own ProcessNode. Subclasses can override CreateNodes() and DeleteNodes() to
  // create additional nodes; call the inherited CreateNodes() last to wait
  // until all nodes are in the PM graph.
  virtual void CreateNodes();

  // Deletes all PM nodes created by CreateNodes(). This is called from
  // PostRunTestOnMainThread(), and can be called earlier to delete nodes during
  // the test. When overriding this, call the inherited DeleteNodes() last to
  // wait until all nodes are removed from the PM graph.
  virtual void DeleteNodes();

  // BrowserTestBase overrides:
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 protected:
  // Details of the frames created by CreateFrameNodes().
  content::GlobalRenderFrameHostId main_frame_id_;
  content::GlobalRenderFrameHostId sub_frame_id_;

  // True if web_contents() has a page that must be unloaded to delete frames.
  bool web_contents_loaded_page_ = false;

 private:
  // True if the ResourceContext registries should be enabled for the test.
  bool enable_registries_ = true;
};

// A test harness that creates PM nodes to test but does NOT enable the
// ResourceContext registries.
class RegistryDisabledBrowserTestHarness : public RegistryBrowserTestHarness {
 public:
  RegistryDisabledBrowserTestHarness() : RegistryBrowserTestHarness(false) {}
};

// Helper classes to wait for nodes to be removed from the PM graph, to test
// registry access from OnBefore*NodeRemoved.

template <typename NodeType, typename ObserverBase>
class RemoveNodeWaiter : public NodeType::ObserverDefaultImpl {
 public:
  using AddRemoveObserverMethod = void (Graph::*)(ObserverBase*);
  using OnRemovedCallback = base::OnceCallback<void(const NodeType*)>;

  // When `watched_node` is removed from the graph, will call
  // `on_removed_callback` from OnBefore*NodeRemoved. This must be created
  // on the main thread before destroying the node's content layer object.
  RemoveNodeWaiter(base::WeakPtr<NodeType> watched_node,
                   OnRemovedCallback on_removed_callback,
                   AddRemoveObserverMethod add_observer,
                   AddRemoveObserverMethod remove_observer);

  ~RemoveNodeWaiter() override;

  RemoveNodeWaiter(const RemoveNodeWaiter&) = delete;
  RemoveNodeWaiter& operator=(const RemoveNodeWaiter&) = delete;

  // Waits until `on_removed_callback` is called. This should be called on the
  // main thread after destroying `watched_node`'s content layer object.
  void Wait();

 protected:
  // Invoked on the PM sequence from the corresponding
  // ObserverDefaultImpl::OnBefore*NodeRemoved() method.
  void OnBeforeNodeRemoved(const NodeType* node);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Only accessed from the main thread.
  base::RunLoop waiter_run_loop_ GUARDED_BY_CONTEXT(sequence_checker_);

  // These are initialized on the main thread in the constructor and read from
  // the PM thread.
  base::WeakPtr<NodeType> watched_node_;
  OnRemovedCallback on_removed_callback_;
  AddRemoveObserverMethod remove_observer_;
  base::OnceClosure waiter_quit_closure_;
};

class RemoveFrameNodeWaiter final
    : public RemoveNodeWaiter<FrameNode, FrameNodeObserver> {
 public:
  using Super = RemoveNodeWaiter<FrameNode, FrameNodeObserver>;

  RemoveFrameNodeWaiter(base::WeakPtr<FrameNode> watched_node,
                        Super::OnRemovedCallback on_removed_callback);

  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) final;
};

class RemovePageNodeWaiter final
    : public RemoveNodeWaiter<PageNode, PageNodeObserver> {
 public:
  using Super = RemoveNodeWaiter<PageNode, PageNodeObserver>;

  RemovePageNodeWaiter(base::WeakPtr<PageNode> watched_node,
                       Super::OnRemovedCallback on_removed_callback);

  void OnBeforePageNodeRemoved(const PageNode* page_node) final;
};

class RemoveProcessNodeWaiter final
    : public RemoveNodeWaiter<ProcessNode, ProcessNodeObserver> {
 public:
  using Super = RemoveNodeWaiter<ProcessNode, ProcessNodeObserver>;

  RemoveProcessNodeWaiter(base::WeakPtr<ProcessNode> watched_node,
                          Super::OnRemovedCallback on_removed_callback);

  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) final;
};

class RemoveWorkerNodeWaiter final
    : public RemoveNodeWaiter<WorkerNode, WorkerNodeObserver> {
 public:
  using Super = RemoveNodeWaiter<WorkerNode, WorkerNodeObserver>;

  RemoveWorkerNodeWaiter(base::WeakPtr<WorkerNode> watched_node,
                         Super::OnRemovedCallback on_removed_callback);

  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) final;
};

// Implementation

// static
template <typename Registry>
void RegistryBrowserTestHarness::RunInGraphWithRegistry(
    base::FunctionRef<void(const Registry*)> function) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([function](Graph* graph) {
                   function(Registry::GetFromGraph(graph));
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

template <typename NodeType, typename ObserverBase>
RemoveNodeWaiter<NodeType, ObserverBase>::RemoveNodeWaiter(
    base::WeakPtr<NodeType> watched_node,
    OnRemovedCallback on_removed_callback,
    AddRemoveObserverMethod add_observer,
    AddRemoveObserverMethod remove_observer)
    : watched_node_(watched_node),
      on_removed_callback_(std::move(on_removed_callback)),
      remove_observer_(remove_observer),
      waiter_quit_closure_(waiter_run_loop_.QuitClosure()) {
  // Add a node observer on the PM sequence.
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([this, add_observer](Graph* graph) {
                   (graph->*add_observer)(this);
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

template <typename NodeType, typename ObserverBase>
RemoveNodeWaiter<NodeType, ObserverBase>::~RemoveNodeWaiter() {
  // Remove the node observer on the PM sequence.
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([this](Graph* graph) {
                   (graph->*remove_observer_)(this);
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

template <typename NodeType, typename ObserverBase>
void RemoveNodeWaiter<NodeType, ObserverBase>::Wait() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  waiter_run_loop_.Run();
}

template <typename NodeType, typename ObserverBase>
void RemoveNodeWaiter<NodeType, ObserverBase>::OnBeforeNodeRemoved(
    const NodeType* node) {
  if (!watched_node_) {
    // The observer is still installed after the node was removed. Ignore.
    ASSERT_TRUE(on_removed_callback_.is_null());
    return;
  }
  if (node == watched_node_.get()) {
    ASSERT_FALSE(on_removed_callback_.is_null());
    std::move(on_removed_callback_).Run(node);
    std::move(waiter_quit_closure_).Run();
  }
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_
