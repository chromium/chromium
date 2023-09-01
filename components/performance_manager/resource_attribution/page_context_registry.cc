// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/page_context_registry.h"

#include <set>

#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

PageContextRegistry::PageContextRegistry(
    const ResourceContextRegistryStorage& storage)
    : storage_(storage) {}

PageContextRegistry::~PageContextRegistry() = default;

// static
absl::optional<PageContext> PageContextRegistry::ContextForWebContents(
    content::WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!contents) {
    return absl::nullopt;
  }
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::PageContextForId(rfh->GetGlobalId());
}

// static
absl::optional<PageContext> PageContextRegistry::ContextForRenderFrameHost(
    content::RenderFrameHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host) {
    return absl::nullopt;
  }
  return ResourceContextRegistryStorage::PageContextForId(host->GetGlobalId());
}

// static
absl::optional<PageContext> PageContextRegistry::ContextForRenderFrameHostId(
    const content::GlobalRenderFrameHostId& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::PageContextForId(id);
}

// static
content::WebContents* PageContextRegistry::WebContentsFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::WebContentsFromContext(context);
}

// static
content::WebContents* PageContextRegistry::WebContentsFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<PageContext>(context)
             ? ResourceContextRegistryStorage::WebContentsFromContext(
                   AsContext<PageContext>(context))
             : nullptr;
}

// static
content::RenderFrameHost*
PageContextRegistry::CurrentMainRenderFrameHostFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::CurrentMainRenderFrameHostFromContext(
      context);
}

// static
content::RenderFrameHost*
PageContextRegistry::CurrentMainRenderFrameHostFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<PageContext>(context)
             ? ResourceContextRegistryStorage::
                   CurrentMainRenderFrameHostFromContext(
                       AsContext<PageContext>(context))
             : nullptr;
}

// static
std::set<content::RenderFrameHost*>
PageContextRegistry::AllMainRenderFrameHostsFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::AllMainRenderFrameHostsFromContext(
      context);
}

// static
std::set<content::RenderFrameHost*>
PageContextRegistry::AllMainRenderFrameHostsFromContext(
    const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ContextIs<PageContext>(context)
             ? ResourceContextRegistryStorage::
                   AllMainRenderFrameHostsFromContext(
                       AsContext<PageContext>(context))
             : std::set<content::RenderFrameHost*>{};
}

const PageNode* PageContextRegistry::GetPageNodeForContext(
    const PageContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_->GetPageNodeForContext(context);
}

const PageNode* PageContextRegistry::GetPageNodeForContext(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ContextIs<PageContext>(context)
             ? storage_->GetPageNodeForContext(AsContext<PageContext>(context))
             : nullptr;
}

}  // namespace performance_manager::resource_attribution
