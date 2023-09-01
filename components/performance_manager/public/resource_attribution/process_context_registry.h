// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_REGISTRY_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderProcessHost;
class BrowserChildProcessHost;
}  // namespace content

namespace performance_manager {
class ProcessNode;
}

namespace performance_manager::resource_attribution {

class ResourceContextRegistryStorage;

class ProcessContextRegistry final
    : public GraphRegisteredImpl<ProcessContextRegistry> {
 public:
  ProcessContextRegistry(const ProcessContextRegistry&) = delete;
  ProcessContextRegistry& operator=(const ProcessContextRegistry&) = delete;

  // Accessors to look up ProcessContext tokens on the UI thread. These are
  // always safe to call but will return nullopt if the PerformanceManager graph
  // is not initialized (during startup and shutdown).

  // Returns the ResourceContext token for the browser process. (In tests this
  // may return nullopt when there is no browser process.)
  static absl::optional<ProcessContext> BrowserProcessContext();

  // Returns the ResourceContext token for the renderer process hosted in
  // `host`.
  static absl::optional<ProcessContext> ContextForRenderProcessHost(
      content::RenderProcessHost* host);

  // Returns the ResourceContext token for the renderer process with id `id`, or
  // nullopt if there is no process with that id.
  static absl::optional<ProcessContext> ContextForRenderProcessHostId(
      RenderProcessHostId id);

  // Returns the ResourceContext token for the non-renderer child process hosted
  // in `host`.
  static absl::optional<ProcessContext> ContextForBrowserChildProcessHost(
      content::BrowserChildProcessHost* host);

  // Returns the ResourceContext token for the non-renderer child process with
  // id `id`, or nullopt if there is no process with that id.
  static absl::optional<ProcessContext> ContextForBrowserChildProcessHostId(
      BrowserChildProcessHostId id);

  // Accessors to resolve ProcessContext tokens on the UI thread. These are
  // always safe to call but will always return false/nullopt if the
  // PerformanceManager graph is not initialized (during startup and shutdown).

  // Returns true iff the given `context` token refers to the browser process.
  static bool IsBrowserProcessContext(const ProcessContext& context);
  static bool IsBrowserProcessContext(const ResourceContext& context);

  // Returns true iff the given `context` token refers to a renderer process.
  static bool IsRenderProcessContext(const ProcessContext& context);
  static bool IsRenderProcessContext(const ResourceContext& context);

  // Returns true iff the given `context` token refers to a non-renderer child
  // process.
  static bool IsBrowserChildProcessContext(const ProcessContext& context);
  static bool IsBrowserChildProcessContext(const ResourceContext& context);

  // If the given `context` token refers to a renderer process, returns its
  // RenderProcessHost. Otherwise returns nullptr.
  static content::RenderProcessHost* RenderProcessHostFromContext(
      const ProcessContext& context);
  static content::RenderProcessHost* RenderProcessHostFromContext(
      const ResourceContext& context);

  // If the given `context` token refers to a non-renderer child process,
  // returns its BrowserChildProcessHost. Otherwise returns nullptr.
  static content::BrowserChildProcessHost* BrowserChildProcessHostFromContext(
      const ProcessContext& context);
  static content::BrowserChildProcessHost* BrowserChildProcessHostFromContext(
      const ResourceContext& context);

  // Accessors to resolve ProcessContext tokens on the PM sequence. To find the
  // ResourceContext token for a ProcessNode, call
  // `process_node->GetResourceContext()`.
  const ProcessNode* GetProcessNodeForContext(
      const ProcessContext& context) const;
  const ProcessNode* GetProcessNodeForContext(
      const ResourceContext& context) const;

 private:
  friend class ResourceContextRegistryStorage;

  explicit ProcessContextRegistry(
      const ResourceContextRegistryStorage& storage);
  ~ProcessContextRegistry() final;

  // Validates that non-static methods are called on the PM sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Accessor for registry storage on the PM sequence. On the UI thread the
  // storage is accessed through static members of
  // ResourceContextRegistryStorage.
  raw_ref<const ResourceContextRegistryStorage> storage_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_REGISTRY_H_
