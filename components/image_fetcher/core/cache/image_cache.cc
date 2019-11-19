// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_cache.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/sha1.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
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

void OnStartupEvictionQueued() {}

}  // namespace

namespace image_fetcher {

// static
std::string ImageCache::HashUrlToKey(const std::string& input) {
  return base32::Base32Encode(base::SHA1HashString(input));
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
    : initialization_attempted_(false),
      data_store_(std::move(data_store)),
      metadata_store_(std::move(metadata_store)),
      pref_service_(pref_service),
      clock_(clock),
      task_runner_(task_runner) {}

ImageCache::~ImageCache() = default;

void ImageCache::SaveImage(std::string url,
                           std::string image_data,
                           bool needs_transcoding) {
  // If the image data is larger than the cache's max size, bail out.
  if (image_data.length() > kCacheMaxSize) {
    return;
  }

  base::OnceClosure request =
      base::BindOnce(&ImageCache::SaveImageImpl, weak_ptr_factory_.GetWeakPtr(),
                     url, std::move(image_data), needs_transcoding);
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
  if (!AreAllDependenciesInitialized()) {
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
  data_store_->Initialize(base::BindOnce(&ImageCache::OnDependencyInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
  metadata_store_->Initialize(base::BindOnce(
      &ImageCache::OnDependencyInitialized, weak_ptr_factory_.GetWeakPtr()));
}

bool ImageCache::AreAllDependenciesInitialized() const {
  return data_store_->IsInitialized() && metadata_store_->IsInitialized();
}

void ImageCache::OnDependencyInitialized() {
  if (!AreAllDependenciesInitialized()) {
    return;
  }

  // Everything is initialized, take care of the queued requests.
  for (base::OnceClosure& request : queued_requests_) {
    std::move(request).Run();
  }
  queued_requests_.clear();

  // TODO(wylieb): Consider delaying eviction as new requests come in via
  // separate weak pointers.
  ImageFetcherMetricsReporter::ReportEvent(
      ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName,
      ImageFetcherEvent::kCacheStartupEvictionStarted);

  // Once all the queued requests are taken care of, run eviction.
  base::PostTaskAndReply(FROM_HERE,
                         {base::ThreadPool(), base::TaskPriority::BEST_EFFORT},
                         base::BindOnce(OnStartupEvictionQueued),
                         base::BindOnce(&ImageCache::RunEvictionOnStartup,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::SaveImageImpl(const std::string& url,
                               std::string image_data,
                               bool needs_transcoding) {
  std::string key = ImageCache::HashUrlToKey(url);

  // If the cache is full, evict some stuff.
  RunEvictionWhenFull();

  size_t length = image_data.length();
  data_store_->SaveImage(key, std::move(image_data), needs_transcoding);
  metadata_store_->SaveImageMetadata(key, length, needs_transcoding);
}

void ImageCache::LoadImageImpl(bool read_only,
                               const std::string& url,
                               ImageDataCallback callback) {
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
    base::Optional<CachedImageMetadataProto> metadata) {
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
  std::string key = ImageCache::HashUrlToKey(url);

  data_store_->DeleteImage(key);
  metadata_store_->DeleteImageMetadata(key);
}

void ImageCache::RunEvictionOnStartup() {
  base::Time last_eviction_time =
      pref_service_->GetTime(kPrefLastStartupEviction);
  // If we've already garbage collected in the past interval, bail out.
  if (last_eviction_time >
      clock_->Now() -
          base::TimeDelta::FromHours(kImageCacheEvictionIntervalHours)) {
    return;
  }

  // Update the time we did startup eviction so it can used for reporting.
  pref_service_->SetTime(kPrefLastStartupEviction, clock_->Now());

  RunEviction(kCacheMaxSize, base::BindOnce(&ImageCache::RunReconciliation,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::RunEvictionWhenFull() {
  // Storage is within limits, bail out.
  if (metadata_store_->GetEstimatedSize() < kCacheMaxSize) {
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
      clock_->Now() - base::TimeDelta::FromDays(kCacheItemsTimeToLiveDays),
      bytes_left,
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
