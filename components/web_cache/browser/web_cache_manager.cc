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
    Add(iter.GetCurrentValue());
  }
}

WebCacheManager::~WebCacheManager() = default;

void WebCacheManager::Add(content::RenderProcessHost* process_host) {
  mojo::Remote<mojom::WebCache> service;
  process_host->BindReceiver(service.BindNewPipeAndPassReceiver());
  web_cache_services_[process_host->GetID()] = std::move(service);
}

void WebCacheManager::Remove(content::RenderProcessHost* process_host) {
  web_cache_services_.erase(process_host->GetID());
}

void WebCacheManager::ClearCache() {
  // Tell each renderer process to clear the cache.
  ClearRendererCache(INSTANTLY);
}

void WebCacheManager::ClearCacheOnNavigation() {
  // Tell each renderer process to clear the cache when a tab is reloaded or
  // the user navigates to a new website.
  ClearRendererCache(ON_NAVIGATION);
}

void WebCacheManager::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  Add(process_host);
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
  Remove(process_host);
}

void WebCacheManager::ClearCacheForProcess(content::ChildProcessId process_id) {
  auto it = web_cache_services_.find(process_id);
  if (it != web_cache_services_.end()) {
    it->second->ClearCache(false);
  }
}

void WebCacheManager::ClearRendererCache(
    WebCacheManager::ClearCacheOccasion occasion) {
  for (auto& service : web_cache_services_) {
    service.second->ClearCache(occasion == ON_NAVIGATION);
  }
}

}  // namespace web_cache
