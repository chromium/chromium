// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the cache manager, handling required clearing of
// the renderer-side cache.

#ifndef COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_
#define COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_

#include <stddef.h>

#include <map>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "components/web_cache/public/mojom/web_cache.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace web_cache {

// Note: memory usage uses uint64_t because potentially the browser could be
// 32 bit and the renderers 64 bits.
class WebCacheManager : public content::RenderProcessHostCreationObserver,
                        public content::RenderProcessHostObserver {
 public:
  // Gets the singleton WebCacheManager object.  The first time this method
  // is called, a WebCacheManager object is constructed and returned.
  // Subsequent calls will return the same object.
  static WebCacheManager* GetInstance();

  WebCacheManager(const WebCacheManager&) = delete;
  WebCacheManager& operator=(const WebCacheManager&) = delete;

  // Clears all in-memory caches.
  void ClearCache();

  // Instantly clears renderer cache for a process.
  // Must be called between Add(process_id) and Remove(process_id).
  void ClearCacheForProcess(int process_id);

  // Clears all in-memory caches when a tab is reloaded or the user navigates
  // to a different website.
  void ClearCacheOnNavigation();

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  friend class base::NoDestructor<WebCacheManager>;
  FRIEND_TEST_ALL_PREFIXES(WebCacheManagerTest, AddRemoveRendererTest);

  // This class is a singleton.  Do not instantiate directly. Call GetInstance()
  // instead.
  WebCacheManager();
  ~WebCacheManager() override;

  // When a render process is created, it registers itself with the cache
  // manager host. The renderer will populate its cache, which may need to get
  // cleared later.
  void Add(int renderer_id);

  // Unregister the renderer when it gets destroyed.
  void Remove(int renderer_id);

  enum ClearCacheOccasion {
    // Instructs to clear the cache instantly.
    INSTANTLY,
    // Instructs to clear the cache when a navigation takes place (this
    // includes reloading a tab).
    ON_NAVIGATION
  };

  // Inform all |renderers| to clear their cache.
  void ClearRendererCache(const std::set<int>& renderers,
                          ClearCacheOccasion occation);

  std::set<int> renderers_;
  // Maps every renderer_id with its corresponding
  // mojo::Remote<mojom::WebCache>. The key is the unique id of every render
  // process host.
  std::map<int, mojo::Remote<mojom::WebCache>> web_cache_services_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      rph_observations_{this};

  base::WeakPtrFactory<WebCacheManager> weak_factory_{this};
};

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_
