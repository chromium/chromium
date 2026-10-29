// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_cache.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "components/image_fetcher/core/cache/image_data_store.h"
#include "components/image_fetcher/core/cache/image_metadata_store.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPrefLastStartupEviction[] =
    "cached_image_fetcher_last_startup_eviction_time";

constexpr char kPrefLastLRUEviction[] =
    "cached_image_fetcher_last_lru_eviction_time";

// TODO(wylieb): Control these parameters server-side.
constexpr int kCacheMaxSize = 64 * 1024 * 1024;         // 64mb.
constexpr int kCacheResizeWhenFull = 48 * 1024 * 1024;  // 48mb.

// Cache items are allowed to live for the given amount of days.
constexpr int kCacheItemsTimeToLiveDays = 7;
constexpr int kImageCacheEvictionIntervalHours = 24;

// Both the image data store and the metadata store must finish initialization.
constexpr int kStoreCount = 2;

void OnStartupEvictionQueued() {}

}  // namespace

namespace image_fetcher {

// static
std::string ImageCache::HashUrlToKey(const std::string& input) {
  return base32::Base32Encode(base::SHA1Hash(base::as_byte_span(input)));
}

// static
void ImageCache::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPrefLastStartupEviction, base::Time());
  registry->RegisterTimePref(kPrefLastLRUEviction, base::Time());
}

ImageCache::ImageCache(std::unique_ptr<ImageDataStore> data_store,
                       std::unique_ptr<ImageMetadataStore> metadata_store,
                       PrefService* pref_service,
                       base::Clock* clock,
                       scoped_refptr<base::SequencedTaskRunner> task_runner)
    : data_store_(std::move(data_store)),
      metadata_store_(std::move(metadata_store)),
      pref_service_(pref_service),
      clock_(clock),
      task_runner_(task_runner) {}

ImageCache::~ImageCache() = default;

void ImageCache::SaveImage(std::string url,
                           std::string image_data,
                           bool needs_transcoding,
                           ExpirationInterval expiration_interval) {
  // If the image data is larger than the cache's max size, bail out.
  if (image_data.length() > kCacheMaxSize) {
    return;
  }

  base::OnceClosure request = base::BindOnce(
      &ImageCache::SaveImageImpl, weak_ptr_factory_.GetWeakPtr(), url,
      std::move(image_data), needs_transcoding, std::move(expiration_interval));
  QueueOrStartRequest(std::move(request));
}

void ImageCache::LoadImage(bool read_only,
                           std::string url,
                           ImageDataCallback callback) {
  base::OnceClosure request =
      base::BindOnce(&ImageCache::LoadImageImpl, weak_ptr_factory_.GetWeakPtr(),
                     read_only, url, std::move(callback));
  QueueOrStartRequest(std::move(request));
}

void ImageCache::DeleteImage(std::string url) {
  base::OnceClosure request = base::BindOnce(
      &ImageCache::DeleteImageImpl, weak_ptr_factory_.GetWeakPtr(), url);
  QueueOrStartRequest(std::move(request));
}

void ImageCache::QueueOrStartRequest(base::OnceClosure request) {
  if (!initialization_finished_) {
    queued_requests_.push_back(std::move(request));
    MaybeStartInitialization();
    return;
  }

  std::move(request).Run();
}

void ImageCache::MaybeStartInitialization() {
  if (initialization_attempted_) {
    return;
  }

  initialization_attempted_ = true;
  base::RepeatingClosure on_initialized = base::BarrierClosure(
      kStoreCount, base::BindOnce(&ImageCache::OnDependenciesInitialized,
                                  weak_ptr_factory_.GetWeakPtr()));
  data_store_->Initialize(on_initialized);
  metadata_store_->Initialize(on_initialized);
}

void ImageCache::OnDependenciesInitialized() {
  // A cache-miss callback may release the last external reference to this
  // cache.
  scoped_refptr<ImageCache> keep_alive(this);
  initialization_finished_ = true;
  const bool data_store_initialized = data_store_->IsInitialized();
  const bool metadata_store_initialized = metadata_store_->IsInitialized();
  cache_available_ = data_store_initialized && metadata_store_initialized;
  if (!data_store_initialized) {
    ImageFetcherMetricsReporter::ReportEvent(
        ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName,
        ImageFetcherEvent::kCacheDataStoreInitializationFailed);
  }
  if (!metadata_store_initialized) {
    ImageFetcherMetricsReporter::ReportEvent(
        ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName,
        ImageFetcherEvent::kCacheMetadataStoreInitializationFailed);
  }

  // Complete queued requests even if a store failed. Loads will return cache
  // misses and writes/deletions will be ignored without touching either store.
  auto queued_requests = std::move(queued_requests_);
  queued_requests_.clear();
  for (base::OnceClosure& request : queued_requests) {
    std::move(request).Run();
  }

  if (!cache_available_) {
    return;
  }

  // TODO(wylieb): Consider delaying eviction as new requests come in via
  // separate weak pointers.
  ImageFetcherMetricsReporter::ReportEvent(
      ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName,
      ImageFetcherEvent::kCacheStartupEvictionStarted);

  // Once all the queued requests are taken care of, run eviction.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(OnStartupEvictionQueued),
      base::BindOnce(&ImageCache::RunEvictionOnStartup,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::SaveImageImpl(const std::string& url,
                               std::string image_data,
                               bool needs_transcoding,
                               ExpirationInterval expiration_interval) {
  if (!cache_available_) {
    return;
  }
  std::string key = ImageCache::HashUrlToKey(url);

  // If the cache is full, evict some stuff.
  RunEvictionWhenFull();

  size_t length = image_data.length();
  data_store_->SaveImage(key, std::move(image_data), needs_transcoding);
  metadata_store_->SaveImageMetadata(key, length, needs_transcoding,
                                     std::move(expiration_interval));
}

void ImageCache::LoadImageImpl(bool read_only,
                               const std::string& url,
                               ImageDataCallback callback) {
  if (!cache_available_) {
    std::move(callback).Run(/*needs_transcoding=*/false, std::string());
    return;
  }
  std::string key = ImageCache::HashUrlToKey(url);
  metadata_store_->LoadImageMetadata(
      key, base::BindOnce(&ImageCache::OnImageMetadataLoadedForLoadImage,
                          weak_ptr_factory_.GetWeakPtr(), read_only, key,
                          std::move(callback), base::TimeTicks::Now()));
}

void ImageCache::OnImageMetadataLoadedForLoadImage(
    bool read_only,
    const std::string& key,
    ImageDataCallback callback,
    base::TimeTicks start_time,
    std::optional<CachedImageMetadataProto> metadata) {
  // Record time spent to load metadata.
  ImageFetcherMetricsReporter::ReportLoadImageMetadata(start_time);

  if (!metadata.has_value()) {
    std::move(callback).Run(/* needs_transcoding */ false, "");
    return;
  }

  data_store_->LoadImage(key, metadata->needs_transcoding(),
                         std::move(callback));
  if (!read_only) {
    metadata_store_->UpdateImageMetadata(key);
  }
}

void ImageCache::DeleteImageImpl(const std::string& url) {
  if (!cache_available_) {
    return;
  }
  std::string key = ImageCache::HashUrlToKey(url);

  data_store_->DeleteImage(key);
  metadata_store_->DeleteImageMetadata(key);
}

void ImageCache::RunEvictionOnStartup() {
  base::Time last_eviction_time =
      pref_service_->GetTime(kPrefLastStartupEviction);
  // If we've already garbage collected in the past interval, bail out.
  if (last_eviction_time >
      clock_->Now() - base::Hours(kImageCacheEvictionIntervalHours)) {
    return;
  }

  // Update the time we did startup eviction so it can used for reporting.
  pref_service_->SetTime(kPrefLastStartupEviction, clock_->Now());

  RunEviction(kCacheMaxSize, base::BindOnce(&ImageCache::RunReconciliation,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::RunEvictionWhenFull() {
  // Storage is within limits, bail out.
  if (metadata_store_->GetEstimatedSize(CacheOption::kBestEffort) <
      kCacheMaxSize) {
    return;
  }

  // Report the time since the last LRU eviction. This allows us to gauge if the
  // cache is properly sized.
  base::Time last_eviction_time = pref_service_->GetTime(kPrefLastLRUEviction);
  // Only report for non-null times.
  if (last_eviction_time != base::Time()) {
    ImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
        last_eviction_time);
  }

  // Update the time we did LRU eviction so it can used for reporting.
  pref_service_->SetTime(kPrefLastLRUEviction, clock_->Now());

  RunEviction(kCacheResizeWhenFull, base::DoNothing());
}

void ImageCache::RunEviction(size_t bytes_left,
                             base::OnceClosure on_completion) {
  metadata_store_->EvictImageMetadata(
      clock_->Now() - base::Days(kCacheItemsTimeToLiveDays), bytes_left,
      base::BindOnce(&ImageCache::OnKeysEvicted, weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_completion)));
}

void ImageCache::OnKeysEvicted(base::OnceClosure on_completion,
                               std::vector<std::string> keys) {
  for (const std::string& key : keys) {
    data_store_->DeleteImage(key);
  }

  std::move(on_completion).Run();
}

void ImageCache::RunReconciliation() {
  metadata_store_->GetAllKeys(base::BindOnce(&ImageCache::ReconcileMetadataKeys,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::ReconcileMetadataKeys(std::vector<std::string> metadata_keys) {
  data_store_->GetAllKeys(base::BindOnce(&ImageCache::ReconcileDataKeys,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(metadata_keys)));
}

void ImageCache::ReconcileDataKeys(std::vector<std::string> metadata_keys,
                                   std::vector<std::string> data_keys) {
  std::sort(metadata_keys.begin(), metadata_keys.end());
  std::sort(data_keys.begin(), data_keys.end());

  std::vector<std::string> diff;
  // Get the keys that should be removed from metadata.
  std::set_difference(metadata_keys.begin(), metadata_keys.end(),
                      data_keys.begin(), data_keys.end(),
                      std::inserter(diff, diff.begin()));

  for (const std::string& key : diff) {
    metadata_store_->DeleteImageMetadata(key);
  }

  diff.clear();
  // Get the keys that should be removed from data.
  std::set_difference(data_keys.begin(), data_keys.end(), metadata_keys.begin(),
                      metadata_keys.end(), std::inserter(diff, diff.begin()));
  for (const std::string& key : diff) {
    data_store_->DeleteImage(key);
  }

  ImageFetcherMetricsReporter::ReportEvent(
      ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName,
      ImageFetcherEvent::kCacheStartupEvictionFinished);
}

}  // namespace image_fetcher
