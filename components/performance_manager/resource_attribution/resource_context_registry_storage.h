// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXT_REGISTRY_STORAGE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXT_REGISTRY_STORAGE_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/resource_attribution/frame_context_registry.h"
#include "components/performance_manager/public/resource_attribution/page_context_registry.h"
#include "components/performance_manager/public/resource_attribution/process_context_registry.h"
#include "components/performance_manager/public/resource_attribution/worker_context_registry.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class BrowserChildProcessHost;
class RenderFrameHost;
class RenderProcessHost;
}  // namespace content

namespace performance_manager::resource_attribution {

// Storage to map all types of ResourceContext tokens to content and
// PerformanceManager objects. Public access is through a set of facade classes,
// one for each context type (ProcessContextRegistry, etc.)
class ResourceContextRegistryStorage final
    : public FrameNode::ObserverDefaultImpl,
      public PageNode::ObserverDefaultImpl,
      public ProcessNode::ObserverDefaultImpl,
      public WorkerNode::ObserverDefaultImpl,
      public GraphOwned {
 public:
  // Storage used from the UI thread.
  class UIThreadStorage;

  ResourceContextRegistryStorage();
  ~ResourceContextRegistryStorage() final;

  ResourceContextRegistryStorage(const ResourceContextRegistryStorage&) =
      delete;
  ResourceContextRegistryStorage& operator=(
      const ResourceContextRegistryStorage&) = delete;

  // Static UI thread accessors.

  // FrameContext accessors.
  static absl::optional<FrameContext> FrameContextForRenderFrameHost(
      content::RenderFrameHost* host);

  static content::RenderFrameHost* RenderFrameHostFromContext(
      const FrameContext& context);

  // PageContext accessors.
  static absl::optional<PageContext> PageContextForId(
      const content::GlobalRenderFrameHostId& id);

  static content::WebContents* WebContentsFromContext(
      const PageContext& context);
  static content::RenderFrameHost* CurrentMainRenderFrameHostFromContext(
      const PageContext& context);
  static std::set<content::RenderFrameHost*> AllMainRenderFrameHostsFromContext(
      const PageContext& context);

  // ProcessContext accessors.
  static absl::optional<ProcessContext> BrowserProcessContext();
  static absl::optional<ProcessContext> ProcessContextForId(
      RenderProcessHostId id);
  static absl::optional<ProcessContext> ProcessContextForId(
      BrowserChildProcessHostId id);

  static bool IsBrowserProcessContext(const ProcessContext& context);
  static bool IsRenderProcessContext(const ProcessContext& context);
  static bool IsBrowserChildProcessContext(const ProcessContext& context);
  static content::RenderProcessHost* RenderProcessHostFromContext(
      const ProcessContext& context);
  static content::BrowserChildProcessHost* BrowserChildProcessHostFromContext(
      const ProcessContext& context);

  // WorkerContext accessors.
  static absl::optional<WorkerContext> WorkerContextForWorkerToken(
      const blink::WorkerToken& token);

  static absl::optional<blink::WorkerToken> WorkerTokenFromContext(
      const WorkerContext& context);

  // PM sequence accessors.
  const FrameNode* GetFrameNodeForContext(const FrameContext& context) const;
  const PageNode* GetPageNodeForContext(const PageContext& context) const;
  const ProcessNode* GetProcessNodeForContext(
      const ProcessContext& context) const;
  const WorkerNode* GetWorkerNodeForContext(const WorkerContext& context) const;

  // FrameNodeObserver overrides:
  void OnFrameNodeAdded(const FrameNode* frame_node) final;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) final;
  void OnIsCurrentChanged(const FrameNode* frame_node) final;

  // PageNodeObserver overrides:
  void OnPageNodeAdded(const PageNode* page_node) final;
  void OnBeforePageNodeRemoved(const PageNode* page_node) final;

  // ProcessNodeObserver overrides:
  void OnProcessNodeAdded(const ProcessNode* process_node) final;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) final;

  // WorkerNodeObserver overrides:
  void OnWorkerNodeAdded(const WorkerNode* worker_node) final;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) final;

  // GraphOwned overrides:
  void OnPassedToGraph(Graph* graph) final;
  void OnTakenFromGraph(Graph* graph) final;

 private:
  static void RegisterUIThreadStorage(UIThreadStorage* storage);
  static void DeleteUIThreadStorage(std::unique_ptr<UIThreadStorage> storage);

  // Validates that non-static methods are called on the PM sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Storage used only from the PM sequence. Mutable so that invalidated
  // WeakPtr's can be cleaned up from logically const methods.
  mutable std::map<FrameContext, base::WeakPtr<FrameNode>>
      frame_nodes_by_context_ GUARDED_BY_CONTEXT(sequence_checker_);
  mutable std::map<PageContext, base::WeakPtr<PageNode>> page_nodes_by_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mutable std::map<ProcessContext, base::WeakPtr<ProcessNode>>
      process_nodes_by_context_ GUARDED_BY_CONTEXT(sequence_checker_);
  mutable std::map<WorkerContext, base::WeakPtr<WorkerNode>>
      worker_nodes_by_context_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<UIThreadStorage> ui_thread_storage_;

  // Pointer through which static methods access UIThreadStorage on the UI
  // thread.
  static UIThreadStorage* static_ui_thread_storage_;

  // Public accessors for the storage. ResourceContextRegistryStorage registers
  // these with the graph in OnPassedToGraph().
  FrameContextRegistry frame_registry_{*this};
  PageContextRegistry page_registry_{*this};
  ProcessContextRegistry process_registry_{*this};
  WorkerContextRegistry worker_registry_{*this};
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXT_REGISTRY_STORAGE_H_
