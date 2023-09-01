// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_REGISTRY_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace performance_manager {
class FrameNode;
}

namespace performance_manager::resource_attribution {

class ResourceContextRegistryStorage;

class FrameContextRegistry final
    : public GraphRegisteredImpl<FrameContextRegistry> {
 public:
  FrameContextRegistry(const FrameContextRegistry&) = delete;
  FrameContextRegistry& operator=(const FrameContextRegistry&) = delete;

  // Accessors to look up FrameContext tokens on the UI thread. These are
  // always safe to call but will return nullopt if the PerformanceManager graph
  // is not initialized (during startup and shutdown).
  static absl::optional<FrameContext> ContextForRenderFrameHost(
      content::RenderFrameHost* host);
  static absl::optional<FrameContext> ContextForRenderFrameHostId(
      const content::GlobalRenderFrameHostId& id);

  // Accessors to resolve FrameContext tokens on the UI thread. These are
  // always safe to call but will always return false/nullopt if the
  // PerformanceManager graph is not initialized (during startup and shutdown).
  static content::RenderFrameHost* RenderFrameHostFromContext(
      const FrameContext& context);
  static content::RenderFrameHost* RenderFrameHostFromContext(
      const ResourceContext& context);

  // Accessors to resolve FrameContext tokens on the PM sequence.
  const FrameNode* GetFrameNodeForContext(const FrameContext& context) const;
  const FrameNode* GetFrameNodeForContext(const ResourceContext& context) const;

 private:
  friend class ResourceContextRegistryStorage;

  explicit FrameContextRegistry(const ResourceContextRegistryStorage& storage);
  ~FrameContextRegistry() final;

  // Validates that non-static methods are called on the PM sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Accessor for registry storage on the PM sequence.
  raw_ref<const ResourceContextRegistryStorage> storage_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_REGISTRY_H_
