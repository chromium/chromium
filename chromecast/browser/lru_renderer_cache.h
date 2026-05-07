// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_
#define CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_

#include <list>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"
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
class LRURendererCache final : public base::MemoryConsumer {
 public:
  LRURendererCache(content::BrowserContext* browser_context,
                   size_t max_renderers);

  LRURendererCache(const LRURendererCache&) = delete;
  LRURendererCache& operator=(const LRURendererCache&) = delete;

  ~LRURendererCache() override;

  // Returns a pre-launched renderer. Returns nullptr if a cached renderer isn't
  // available (clients should create their own in this case).
  std::unique_ptr<RendererPrelauncher> TakeRendererPrelauncher(
      const GURL& page_url);

  // Indicate that the renderer for |page_url| is no longer in use. If the total
  // number of in-use renderers is less than the current maximum allowed, then
  // we will immediately pre-load the renderer for |page_url| since it was
  // recently used. This operation may evict a prelaunched renderer to keep the
  // total pool size within limits.
  void ReleaseRendererPrelauncher(const GURL& page_url);

 private:
  friend class LRURendererCacheTest;

  void SetFactoryForTesting(RendererPrelauncherFactory* factory);

  void StartNextPrelauncher(const GURL& page_url);

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;

  // Returns the actual maximum amount of renderers allowed, which changes
  // depending on the memory allocation ratio provided by the Memory
  // Coordinator, if kStatefulMemoryPressure is enabled.
  size_t GetCurrentMaxRenderers() const;

  // Evict pre-launched renderers so that the total number of in-use and cached
  // renderers doesn't exceed |GetCurrentMaxRenderers()|.
  void EvictCache();

  content::BrowserContext* const browser_context_;
  // The baseline maximum number of renderers allowed at 100% memory allocation.
  const size_t max_renderers_basis_;
  size_t in_use_count_;
  // The maximum number of renderers allowed, calculated based on
  // |max_renderers_basis_| and the current memory allocation ratio.
  size_t current_max_renderers_;
  std::list<std::unique_ptr<RendererPrelauncher>> cache_;
  base::MemoryConsumerRegistration memory_consumer_registration_;

  RendererPrelauncherFactory* factory_for_testing_ = nullptr;

  base::WeakPtrFactory<LRURendererCache> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_LRU_RENDERER_CACHE_H_
