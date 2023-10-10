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
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/page_context_registry.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace performance_manager::resource_attribution {

// Storage to map all types of ResourceContext tokens to content and
// PerformanceManager objects. Public access is through a set of facade classes,
// one for each context type (ProcessContextRegistry, etc.)
class ResourceContextRegistryStorage final
    : public FrameNode::ObserverDefaultImpl,
      public PageNode::ObserverDefaultImpl,
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

  // PageContext accessors.
  static absl::optional<PageContext> PageContextForId(
      const content::GlobalRenderFrameHostId& id);

  static content::WebContents* WebContentsFromContext(
      const PageContext& context);
  static content::RenderFrameHost* CurrentMainRenderFrameHostFromContext(
      const PageContext& context);
  static std::set<content::RenderFrameHost*> AllMainRenderFrameHostsFromContext(
      const PageContext& context);

  // PM sequence accessors.
  const PageNode* GetPageNodeForContext(const PageContext& context) const;

  // FrameNodeObserver overrides:
  void OnFrameNodeAdded(const FrameNode* frame_node) final;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) final;
  void OnIsCurrentChanged(const FrameNode* frame_node) final;

  // PageNodeObserver overrides:
  void OnPageNodeAdded(const PageNode* page_node) final;
  void OnBeforePageNodeRemoved(const PageNode* page_node) final;

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
  mutable std::map<PageContext, base::WeakPtr<PageNode>> page_nodes_by_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<UIThreadStorage> ui_thread_storage_;

  // Pointer through which static methods access UIThreadStorage on the UI
  // thread.
  static UIThreadStorage* static_ui_thread_storage_;

  // Public accessors for the storage. ResourceContextRegistryStorage registers
  // these with the graph in OnPassedToGraph().
  PageContextRegistry page_registry_{*this};
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXT_REGISTRY_STORAGE_H_
