// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/browser/web_cache_manager.h"

#include "base/no_destructor.h"

namespace web_cache {

// static
WebCacheManager* WebCacheManager::GetInstance() {
  static base::NoDestructor<WebCacheManager> s_instance;
  return s_instance.get();
}

WebCacheManager::WebCacheManager() {
  // The instance can be created once renderers are already running, thus we
  // cannot observe the creation of the previous processes.
  for (auto iter(content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    Add(iter.GetCurrentValue()->GetID());
  }
}
WebCacheManager::~WebCacheManager() = default;

void WebCacheManager::Add(int renderer_id) {
  renderers_.insert(renderer_id);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(renderer_id);
  if (host) {
    mojo::Remote<mojom::WebCache> service;
    host->BindReceiver(service.BindNewPipeAndPassReceiver());
    web_cache_services_[renderer_id] = std::move(service);
  }
}

void WebCacheManager::Remove(int renderer_id) {
  renderers_.erase(renderer_id);
  web_cache_services_.erase(renderer_id);
}

void WebCacheManager::ClearCache() {
  // Tell each renderer process to clear the cache.
  ClearRendererCache(renderers_, INSTANTLY);
}

void WebCacheManager::ClearCacheOnNavigation() {
  // Tell each renderer process to clear the cache when a tab is reloaded or
  // the user navigates to a new website.
  ClearRendererCache(renderers_, ON_NAVIGATION);
}

void WebCacheManager::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  Add(process_host->GetID());
  rph_observations_.AddObservation(process_host);
}

void WebCacheManager::RenderProcessExited(
    content::RenderProcessHost* process_host,
    const content::ChildProcessTerminationInfo& info) {
  RenderProcessHostDestroyed(process_host);
}

void WebCacheManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* process_host) {
  rph_observations_.RemoveObservation(process_host);
  Remove(process_host->GetID());
}

void WebCacheManager::ClearCacheForProcess(int render_process_id) {
  std::set<int> renderers;
  renderers.insert(render_process_id);
  ClearRendererCache(renderers, INSTANTLY);
}

void WebCacheManager::ClearRendererCache(
    const std::set<int>& renderers,
    WebCacheManager::ClearCacheOccasion occasion) {
  for (int renderer_id : renderers) {
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(renderer_id);
    if (host) {
      // Find the mojo::Remote<WebCache> by renderer process id.
      auto it = web_cache_services_.find(renderer_id);
      if (it != web_cache_services_.end()) {
        mojo::Remote<mojom::WebCache>& service = it->second;
        DCHECK(service);
        service->ClearCache(occasion == ON_NAVIGATION);
      }
    }
  }
}

}  // namespace web_cache
