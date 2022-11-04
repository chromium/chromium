// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_
#define CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_

#include <list>
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace chromecast {
class RendererPrelauncher;

// Factory class for testing.
class RendererPrelauncherFactory {
 public:
  virtual std::unique_ptr<RendererPrelauncher> Create(
      content::BrowserContext* browser_context,
      const GURL& page_url) = 0;

  virtual ~RendererPrelauncherFactory() = default;
};

// This class maintains a pool of prelaunched (initialized) renderers.
class LRURendererCache {
 public:
  LRURendererCache(content::BrowserContext* browser_context,
                   size_t max_renderers);

  LRURendererCache(const LRURendererCache&) = delete;
  LRURendererCache& operator=(const LRURendererCache&) = delete;

  virtual ~LRURendererCache();

  // Returns a pre-launched renderer. Returns nullptr if a cached renderer isn't
  // available (clients should create their own in this case).
  std::unique_ptr<RendererPrelauncher> TakeRendererPrelauncher(
      const GURL& page_url);

  // Indicate that the renderer for |page_url| is no longer in use. If the total
  // number of in-use renderers is less than |max_renderers_|, then we will
  // immediately pre-load the renderer for |page_url| since it was recently
  // used. This operation may evict a prelaunched renderer to keep the total
  // pool size below |max_renderers_|
  void ReleaseRendererPrelauncher(const GURL& page_url);

 private:
  friend class LRURendererCacheTest;

  void SetFactoryForTesting(RendererPrelauncherFactory* factory);

  void StartNextPrelauncher(const GURL& page_url);
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Evict pre-launched renderers so that the total number of in-use and cached
  // renderers doesn't exceed |max_renderers_|.
  void EvictCache();

  content::BrowserContext* const browser_context_;
  const size_t max_renderers_;
  size_t in_use_count_;
  std::list<std::unique_ptr<RendererPrelauncher>> cache_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  RendererPrelauncherFactory* factory_for_testing_ = nullptr;

  base::WeakPtrFactory<LRURendererCache> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_
