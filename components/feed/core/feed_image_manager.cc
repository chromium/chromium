// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_image_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/feed/core/time_serialization.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace feed {

namespace {

// Keep in sync with DIMENSION_UNKNOWN in third_party/feed/src/main/java/com/
//  google/android/libraries/feed/host/imageloader/ImageLoaderApi.java.
const int DIMENSION_UNKNOWN = -1;

const int kDefaultGarbageCollectionExpiredDays = 30;
const int kLongGarbageCollectionInterval = 12 * 60 * 60;  // 12 hours
const int kShortGarbageCollectionInterval = 5 * 60;       // 5 minutes
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("feed_image_fetcher", R"(
        semantics {
          sender: "Feed Library Image Fetch"
          description:
            "Retrieves images for content suggestions, for display on the "
            "New Tab page."
          trigger:
            "Triggered when the user looks at a content suggestion (and its "
            "thumbnail isn't cached yet)."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This can be disabled from the New Tab Page by collapsing the "
            "articles section."
        chrome_policy {
          NTPContentSuggestionsEnabled {
            NTPContentSuggestionsEnabled: false
          }
        }
      })");

void ReportFetchResult(FeedImageFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.Image.FetchResult",
                            result);
}

gfx::Size CreateGfxSize(int width_px, int height_px) {
  DCHECK_GE(width_px, DIMENSION_UNKNOWN);
  DCHECK_GE(height_px, DIMENSION_UNKNOWN);

  // Only resize the image when both |width_px| and |height_px| are available.
  if (width_px == DIMENSION_UNKNOWN || height_px == DIMENSION_UNKNOWN) {
    return gfx::Size();
  }
  return gfx::Size(width_px, height_px);
}

}  // namespace

FeedImageManager::FeedImageManager(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<FeedImageDatabase> image_database)
    : image_garbage_collected_day_(FromDatabaseTime(0)),
      image_fetcher_(std::move(image_fetcher)),
      image_database_(std::move(image_database)),
      weak_ptr_factory_(this) {
  DoGarbageCollectionIfNeeded();
}

FeedImageManager::~FeedImageManager() {
  StopGarbageCollection();
}

void FeedImageManager::FetchImage(std::vector<std::string> urls,
                                  int width_px,
                                  int height_px,
                                  ImageFetchedCallback callback) {
  DCHECK(image_database_);

  FetchImagesFromDatabase(0, std::move(urls), width_px, height_px,
                          std::move(callback));
}

void FeedImageManager::FetchImagesFromDatabase(size_t url_index,
                                               std::vector<std::string> urls,
                                               int width_px,
                                               int height_px,
                                               ImageFetchedCallback callback) {
  if (url_index >= urls.size()) {
    // Already reached the last entry. Return an empty image.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), gfx::Image(), -1));
    return;
  }

  const std::string& image_id = urls[url_index];
  // Only take the first instance of the url so we get the worst-case time.
  if (url_timers_.find(image_id) == url_timers_.end()) {
    url_timers_.insert(std::make_pair(image_id, base::ElapsedTimer()));
  }
  image_database_->LoadImage(
      image_id,
      base::BindOnce(&FeedImageManager::OnImageFetchedFromDatabase,
                     weak_ptr_factory_.GetWeakPtr(), url_index, std::move(urls),
                     width_px, height_px, std::move(callback)));
}

void FeedImageManager::OnImageFetchedFromDatabase(
    size_t url_index,
    std::vector<std::string> urls,
    int width_px,
    int height_px,
    ImageFetchedCallback callback,
    const std::string& image_data) {
  if (image_data.empty()) {
    // Fetching from the DB failed; start a network fetch.
    FetchImageFromNetwork(url_index, std::move(urls), width_px, height_px,
                          std::move(callback));
    return;
  }

  image_fetcher_->GetImageDecoder()->DecodeImage(
      image_data, CreateGfxSize(width_px, height_px),
      base::BindRepeating(&FeedImageManager::OnImageDecodedFromDatabase,
                          weak_ptr_factory_.GetWeakPtr(), url_index,
                          std::move(urls), width_px, height_px,
                          base::Passed(std::move(callback))));
}

void FeedImageManager::OnImageDecodedFromDatabase(size_t url_index,
                                                  std::vector<std::string> urls,
                                                  int width_px,
                                                  int height_px,
                                                  ImageFetchedCallback callback,
                                                  const gfx::Image& image) {
  const std::string& image_id = urls[url_index];
  if (image.IsEmpty()) {
    // If decoding the image failed, delete the DB entry.
    image_database_->DeleteImage(image_id);
    FetchImageFromNetwork(url_index, std::move(urls), width_px, height_px,
                          std::move(callback));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), image, url_index));

  // Report success if the url exists.
  // This check is for concurrent access to the same url.
  if (url_timers_.find(image_id) != url_timers_.end()) {
    UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.Image.LoadFromCacheTime",
                        url_timers_[image_id].Elapsed());
    ClearUmaTimer(image_id);
    ReportFetchResult(FeedImageFetchResult::kSuccessCached);
  }
}

void FeedImageManager::FetchImageFromNetwork(size_t url_index,
                                             std::vector<std::string> urls,
                                             int width_px,
                                             int height_px,
                                             ImageFetchedCallback callback) {
  const std::string& image_id = urls[url_index];
  GURL url(image_id);
  if (!url.is_valid()) {
    // Report failure.
    ReportFetchResult(FeedImageFetchResult::kFailure);
    ClearUmaTimer(image_id);

    // url is not valid, go to next URL.
    FetchImagesFromDatabase(url_index + 1, std::move(urls), width_px, height_px,
                            std::move(callback));
    return;
  }

  image_fetcher_->FetchImageData(
      url.spec(), url,
      base::BindOnce(&FeedImageManager::OnImageFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), url_index, std::move(urls),
                     width_px, height_px, std::move(callback)),
      kTrafficAnnotation);
}

void FeedImageManager::OnImageFetchedFromNetwork(
    size_t url_index,
    std::vector<std::string> urls,
    int width_px,
    int height_px,
    ImageFetchedCallback callback,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& request_metadata) {
  if (image_data.empty()) {
    // Report failure.
    ReportFetchResult(FeedImageFetchResult::kFailure);
    ClearUmaTimer(urls[url_index]);

    // Fetching image failed, let's move to the next url.
    FetchImagesFromDatabase(url_index + 1, std::move(urls), width_px, height_px,
                            std::move(callback));
    return;
  }

  image_fetcher_->GetImageDecoder()->DecodeImage(
      image_data, CreateGfxSize(width_px, height_px),
      base::BindRepeating(&FeedImageManager::OnImageDecodedFromNetwork,
                          weak_ptr_factory_.GetWeakPtr(), url_index,
                          std::move(urls), width_px, height_px,
                          base::Passed(std::move(callback)), image_data));
}

void FeedImageManager::OnImageDecodedFromNetwork(size_t url_index,
                                                 std::vector<std::string> urls,
                                                 int width_px,
                                                 int height_px,
                                                 ImageFetchedCallback callback,
                                                 const std::string& image_data,
                                                 const gfx::Image& image) {
  std::string image_id = urls[url_index];
  if (image.IsEmpty()) {
    // Report failure.
    ReportFetchResult(FeedImageFetchResult::kFailure);
    ClearUmaTimer(image_id);

    // Decoding failed, let's move to the next url.
    FetchImagesFromDatabase(url_index + 1, std::move(urls), width_px, height_px,
                            std::move(callback));
    return;
  }

  image_database_->SaveImage(image_id, image_data);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), image, url_index));

  // Report success if the url exists.
  // This check is for concurrent access to the same url.
  if (url_timers_.find(image_id) != url_timers_.end()) {
    UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.Image.LoadFromNetworkTime",
                        url_timers_[image_id].Elapsed());
    ClearUmaTimer(image_id);
    ReportFetchResult(FeedImageFetchResult::kSuccessFetched);
  }
}

void FeedImageManager::DoGarbageCollectionIfNeeded() {
  // For saving resource purpose(ex. cpu, battery), We round up garbage
  // collection age to day, so we only run GC once a day.
  base::Time to_be_expired =
      base::Time::Now().LocalMidnight() -
      base::TimeDelta::FromDays(kDefaultGarbageCollectionExpiredDays);
  if (image_garbage_collected_day_ != to_be_expired) {
    image_database_->GarbageCollectImages(
        to_be_expired,
        base::BindOnce(&FeedImageManager::OnGarbageCollectionDone,
                       weak_ptr_factory_.GetWeakPtr(), to_be_expired));
  }
}

void FeedImageManager::OnGarbageCollectionDone(base::Time garbage_collected_day,
                                               bool success) {
  base::TimeDelta gc_delay =
      base::TimeDelta::FromSeconds(kShortGarbageCollectionInterval);
  if (success) {
    if (image_garbage_collected_day_ < garbage_collected_day)
      image_garbage_collected_day_ = garbage_collected_day;
    gc_delay = base::TimeDelta::FromSeconds(kLongGarbageCollectionInterval);
  }

  garbage_collection_timer_.Start(
      FROM_HERE, gc_delay, this,
      &FeedImageManager::DoGarbageCollectionIfNeeded);
}

void FeedImageManager::StopGarbageCollection() {
  garbage_collection_timer_.Stop();
}

void FeedImageManager::ClearUmaTimer(const std::string& url) {
  url_timers_.erase(url);
}

}  // namespace feed
