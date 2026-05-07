// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/lru_renderer_cache.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/utils.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/browser/renderer_prelauncher.h"
#include "content/public/browser/site_instance.h"

namespace chromecast {

LRURendererCache::LRURendererCache(content::BrowserContext* browser_context,
                                   size_t max_renderers)
    : browser_context_(browser_context),
      max_renderers_basis_(max_renderers),
      in_use_count_(0),
      current_max_renderers_(max_renderers),
      memory_consumer_registration_(
          "LRURendererCache",
          std::nullopt,  // TODO(b/489671163): Fix traits.
          this,
          base::MemoryConsumerRegistration::CheckUnregister::kDisabled,
          base::MemoryConsumerRegistration::CheckRegistryExists::kDisabled),
      weak_factory_(this) {
  DCHECK(browser_context_);
  // Ensure any already assigned memory limit is honored.
  OnUpdateMemoryLimit();
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
  // doesn't exceed |GetCurrentMaxRenderers()|, or until the cache is empty.
  const size_t current_max_renderers = GetCurrentMaxRenderers();
  while (!cache_.empty() &&
         in_use_count_ + cache_.size() > current_max_renderers) {
    LOG(INFO) << "Evicting pre-launched SiteInstance: " << cache_.back()->url();
    cache_.pop_back();
  }
}

void LRURendererCache::ReleaseRendererPrelauncher(const GURL& page_url) {
  DCHECK(in_use_count_ > 0);
  in_use_count_--;
  if (in_use_count_ >= GetCurrentMaxRenderers()) {
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
  // pre-launched process, temporarily exceeding |GetCurrentMaxRenderers()|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LRURendererCache::StartNextPrelauncher,
                                weak_factory_.GetWeakPtr(), page_url));
}

void LRURendererCache::StartNextPrelauncher(const GURL& page_url) {
  if (in_use_count_ >= GetCurrentMaxRenderers()) {
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

void LRURendererCache::OnUpdateMemoryLimit() {
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    // IMPORTANT: Ensure no memory is released during this call.
    // By using std::max, we ensure the new limit is at least the current size,
    // preventing growth without triggering immediate eviction.
    // The target size is calculated by scaling the baseline maximum
    // |max_renderers_basis_| by the memory allocation ratio.
    size_t target_size =
        base::ScaleByMemoryLimit(max_renderers_basis_, memory_limit());
    current_max_renderers_ =
        std::max(in_use_count_ + cache_.size(), target_size);
  }
}

void LRURendererCache::OnReleaseMemory() {
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    current_max_renderers_ =
        base::ScaleByMemoryLimit(max_renderers_basis_, memory_limit());
    EvictCache();
  } else if (memory_limit_ratio() <= base::kCriticalMemoryPressureThreshold) {
    DLOG(INFO) << "Dropping prelauncher cache due to memory coordinator "
                  "notification.";
    cache_.clear();
  }
}

size_t LRURendererCache::GetCurrentMaxRenderers() const {
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    return current_max_renderers_;
  }
  return max_renderers_basis_;
}

void LRURendererCache::SetFactoryForTesting(
    RendererPrelauncherFactory* factory) {
  DCHECK(factory);
  factory_for_testing_ = factory;
}

}  // namespace chromecast
