// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_REGISTRY_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
struct GlobalRenderFrameHostId;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace performance_manager {
class PageNode;
}

namespace performance_manager::resource_attribution {

class ResourceContextRegistryStorage;

class PageContextRegistry final
    : public GraphRegisteredImpl<PageContextRegistry> {
 public:
  PageContextRegistry(const PageContextRegistry&) = delete;
  PageContextRegistry& operator=(const PageContextRegistry&) = delete;

  // Accessors to look up PageContext tokens on the UI thread. These are
  // always safe to call but will return nullopt if the PerformanceManager graph
  // is not initialized (during startup and shutdown).
  //
  // TODO(https://crbug.com/1211368): PerformanceManager currently has one
  // PageNode per WebContents, with multiple "main" FrameNodes for different
  // page states (primary page, prerendering, BFCache). This interface copies
  // that structure. Eventually PerformanceManager may be refactored to expose
  // multiple PageNodes, one for each page state, with a single main FrameNode
  // per PageNode. When that happens, update this interface to match.
  static absl::optional<PageContext> ContextForWebContents(
      content::WebContents* contents);
  static absl::optional<PageContext> ContextForRenderFrameHost(
      content::RenderFrameHost* host);
  static absl::optional<PageContext> ContextForRenderFrameHostId(
      const content::GlobalRenderFrameHostId& id);

  // Accessors to resolve PageContext tokens on the UI thread. These are
  // always safe to call but will always return false/nullopt if the
  // PerformanceManager graph is not initialized (during startup and shutdown).
  static content::WebContents* WebContentsFromContext(
      const PageContext& context);
  static content::WebContents* WebContentsFromContext(
      const ResourceContext& context);
  static content::RenderFrameHost* CurrentMainRenderFrameHostFromContext(
      const PageContext& context);
  static content::RenderFrameHost* CurrentMainRenderFrameHostFromContext(
      const ResourceContext& context);
  static std::set<content::RenderFrameHost*> AllMainRenderFrameHostsFromContext(
      const PageContext& context);
  static std::set<content::RenderFrameHost*> AllMainRenderFrameHostsFromContext(
      const ResourceContext& context);

  // Accessors to resolve PageContext tokens on the PM sequence.
  const PageNode* GetPageNodeForContext(const PageContext& context) const;
  const PageNode* GetPageNodeForContext(const ResourceContext& context) const;

 private:
  friend class ResourceContextRegistryStorage;

  explicit PageContextRegistry(const ResourceContextRegistryStorage& storage);
  ~PageContextRegistry() final;

  // Validates that non-static methods are called on the PM sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Accessor for registry storage on the PM sequence.
  raw_ref<const ResourceContextRegistryStorage> storage_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_REGISTRY_H_
