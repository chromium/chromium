// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/frame_context_registry.h"

#include "base/sequence_checker.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

FrameContextRegistry::FrameContextRegistry(
    const ResourceContextRegistryStorage& storage)
    : storage_(storage) {}

FrameContextRegistry::~FrameContextRegistry() = default;

// static
absl::optional<FrameContext> FrameContextRegistry::ContextForRenderFrameHost(
    content::RenderFrameHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::FrameContextForRenderFrameHost(host);
}

// static
absl::optional<FrameContext> FrameContextRegistry::ContextForRenderFrameHostId(
    const content::GlobalRenderFrameHostId& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::FrameContextForRenderFrameHost(
      content::RenderFrameHost::FromID(id));
}

// static
content::RenderFrameHost* FrameContextRegistry::RenderFrameHostFromContext(
    const FrameContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::RenderFrameHostFromContext(context);
}

// static
content::RenderFrameHost* FrameContextRegistry::RenderFrameHostFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<FrameContext>(context)
             ? ResourceContextRegistryStorage::RenderFrameHostFromContext(
                   AsContext<FrameContext>(context))
             : nullptr;
}

const FrameNode* FrameContextRegistry::GetFrameNodeForContext(
    const FrameContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_->GetFrameNodeForContext(context);
}

const FrameNode* FrameContextRegistry::GetFrameNodeForContext(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ContextIs<FrameContext>(context)
             ? storage_->GetFrameNodeForContext(
                   AsContext<FrameContext>(context))
             : nullptr;
}

}  // namespace performance_manager::resource_attribution
