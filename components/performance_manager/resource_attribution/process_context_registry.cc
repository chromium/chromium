// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/process_context_registry.h"

#include "base/sequence_checker.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

// ResourceContextRegistryStorage owns the ProcessContextRegistry facade, so
// `storage` will always be valid.
ProcessContextRegistry::ProcessContextRegistry(
    const ResourceContextRegistryStorage& storage)
    : storage_(storage) {}

ProcessContextRegistry::~ProcessContextRegistry() = default;

// static
absl::optional<ProcessContext> ProcessContextRegistry::BrowserProcessContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::BrowserProcessContext();
}

// static
absl::optional<ProcessContext>
ProcessContextRegistry::ContextForRenderProcessHost(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::ProcessContextForId(
      RenderProcessHostId(host->GetID()));
}

// static
absl::optional<ProcessContext>
ProcessContextRegistry::ContextForRenderProcessHostId(RenderProcessHostId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!id) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::ProcessContextForId(id);
}

// static
absl::optional<ProcessContext>
ProcessContextRegistry::ContextForBrowserChildProcessHost(
    content::BrowserChildProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::ProcessContextForId(
      BrowserChildProcessHostId(host->GetData().id));
}

// static
absl::optional<ProcessContext>
ProcessContextRegistry::ContextForBrowserChildProcessHostId(
    BrowserChildProcessHostId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!id) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::ProcessContextForId(id);
}

// static
bool ProcessContextRegistry::IsBrowserProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::IsBrowserProcessContext(context);
}

// static
bool ProcessContextRegistry::IsBrowserProcessContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<ProcessContext>(context) &&
         ResourceContextRegistryStorage::IsBrowserProcessContext(
             AsContext<ProcessContext>(context));
}

// static
bool ProcessContextRegistry::IsRenderProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::IsRenderProcessContext(context);
}

// static
bool ProcessContextRegistry::IsRenderProcessContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<ProcessContext>(context) &&
         ResourceContextRegistryStorage::IsRenderProcessContext(
             AsContext<ProcessContext>(context));
}

// static
bool ProcessContextRegistry::IsBrowserChildProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::IsBrowserChildProcessContext(context);
}

// static
bool ProcessContextRegistry::IsBrowserChildProcessContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<ProcessContext>(context) &&
         ResourceContextRegistryStorage::IsBrowserChildProcessContext(
             AsContext<ProcessContext>(context));
}

// static
content::RenderProcessHost*
ProcessContextRegistry::RenderProcessHostFromContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::RenderProcessHostFromContext(context);
}

// static
content::RenderProcessHost*
ProcessContextRegistry::RenderProcessHostFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<ProcessContext>(context)
             ? ProcessContextRegistry::RenderProcessHostFromContext(
                   AsContext<ProcessContext>(context))
             : nullptr;
}

// static
content::BrowserChildProcessHost*
ProcessContextRegistry::BrowserChildProcessHostFromContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::BrowserChildProcessHostFromContext(
      context);
}

// static
content::BrowserChildProcessHost*
ProcessContextRegistry::BrowserChildProcessHostFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<ProcessContext>(context)
             ? ProcessContextRegistry::BrowserChildProcessHostFromContext(
                   AsContext<ProcessContext>(context))
             : nullptr;
}

const ProcessNode* ProcessContextRegistry::GetProcessNodeForContext(
    const ProcessContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_->GetProcessNodeForContext(context);
}

const ProcessNode* ProcessContextRegistry::GetProcessNodeForContext(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ContextIs<ProcessContext>(context)
             ? storage_->GetProcessNodeForContext(
                   AsContext<ProcessContext>(context))
             : nullptr;
}

}  // namespace performance_manager::resource_attribution
