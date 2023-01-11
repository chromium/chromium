// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/lru_renderer_cache.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/browser/renderer_prelauncher.h"
#include "content/public/browser/site_instance.h"

namespace chromecast {

LRURendererCache::LRURendererCache(
    content::BrowserContext* browser_context,
    size_t max_renderers)
    : browser_context_(browser_context),
      max_renderers_(max_renderers),
      in_use_count_(0),
      weak_factory_(this) {
  DCHECK(browser_context_);
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&LRURendererCache::OnMemoryPressure,
                                     weak_factory_.GetWeakPtr()));
}

LRURendererCache::~LRURendererCache() = default;

std::unique_ptr<RendererPrelauncher> LRURendererCache::TakeRendererPrelauncher(
    const GURL& page_url) {
  in_use_count_++;
  std::unique_ptr<RendererPrelauncher> search;
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    if ((*it)->IsForURL(page_url)) {
      // Found pre-launched renderer for this site, return it.
      search = std::move(*it);
      cache_.erase(it);
      LOG(INFO) << "Cache hit for pre-launched SiteInstance: "
                << page_url.spec();
      break;
    }
  }

  if (!search) {
    // TODO(jlevasseur): Submit metric?
    LOG(WARNING) << "Cache miss for pre-launched SiteInstance: "
                 << page_url.spec();
  }

  EvictCache();
  return search;
}

void LRURendererCache::EvictCache() {
  // Evict least-recently-used renderers so that the total number of renderers
  // doesn't exceed |max_renderers_|, or until the cache is empty.
  while (!cache_.empty() && in_use_count_ + cache_.size() > max_renderers_) {
    LOG(INFO) << "Evicting pre-launched SiteInstance: " << cache_.back()->url();
    cache_.pop_back();
  }
}

void LRURendererCache::ReleaseRendererPrelauncher(const GURL& page_url) {
  DCHECK(in_use_count_ > 0);
  in_use_count_--;
  if (in_use_count_ >= max_renderers_) {
    DCHECK(cache_.empty());
    // We don't have room to maintain a cache, so don't prelaunch this site even
    // though it's the most recently used.
    return;
  }
  if (!page_url.is_valid()) {
    // Can't cache an invalid site.
    return;
  }
  // We have room to maintain a non-empty cache, so we can pre-launch the
  // renderer process for the next site. We post this as a task to ensure that
  // the prior site (which is in the process of being released) has completed
  // destruction; otherwise, its renderer process will overlap with the next
  // pre-launched process, temporarily exceeding |max_renderers_|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LRURendererCache::StartNextPrelauncher,
                                weak_factory_.GetWeakPtr(), page_url));
}

void LRURendererCache::StartNextPrelauncher(const GURL& page_url) {
  if (in_use_count_ >= max_renderers_) {
    DCHECK(cache_.empty());
    // The maximum number of renderers is already in use, so the cache must
    // remain empty.
    return;
  }
  DLOG(INFO) << "Pre-launching a renderer for: " << page_url.spec();
  if (factory_for_testing_) {
    cache_.push_front(factory_for_testing_->Create(browser_context_, page_url));
  } else {
    cache_.push_front(
        std::make_unique<RendererPrelauncher>(browser_context_, page_url));
  }
  // Evict the cache before prelaunching.
  EvictCache();
  cache_.front()->Prelaunch();
}

void LRURendererCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    DLOG(INFO) << "Dropping prelauncher cache due to memory pressure.";
    cache_.clear();
  }
}

void LRURendererCache::SetFactoryForTesting(
    RendererPrelauncherFactory* factory) {
  DCHECK(factory);
  factory_for_testing_ = factory;
}

}  // namespace chromecast
