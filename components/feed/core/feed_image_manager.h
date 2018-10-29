// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_IMAGE_MANAGER_H_
#define COMPONENTS_FEED_CORE_FEED_IMAGE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/feed/core/feed_image_database.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace feed {

using ImageFetchedCallback =
    base::OnceCallback<void(const gfx::Image&, size_t)>;

// Enum for the result of the fetch, reported through UMA.
// New values should be added at the end and things should not be renumbered.
enum class FeedImageFetchResult {
  kSuccessCached = 0,
  kSuccessFetched = 1,
  kFailure = 2,
  kMaxValue = kFailure,
};

// FeedImageManager takes care of fetching images from the network and caching
// them in the database.
class FeedImageManager {
 public:
  FeedImageManager(std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                   std::unique_ptr<FeedImageDatabase> image_database);
  ~FeedImageManager();

  // Fetches an image from |urls|, and resize the image with |width_px| and
  // |height_px|. FeedImageManager will go through URLs in |urls| one by one
  // trying to fetch and decode them in order. If |width_px| and |height_px| are
  // not available/legal, FeedImageManager will not resize the image. Upon
  // success, a decoded image will be passed to |callback| as well as cached
  // locally. |urls| should be supplied in priority order, and the first success
  // will prevent any further processing. Failure to fetch or decode an image
  // will cause FeedImageManager to process the next URL in |urls|. If
  // FeedImageManager failed to fetch and decode all the URLs in |urls|, it will
  // pass an empty image to |callback|. |callback| will be called exactly once.
  void FetchImage(std::vector<std::string> urls,
                  int width_px,
                  int height_px,
                  ImageFetchedCallback callback);

 private:
  friend class FeedImageManagerTest;

  // Database
  void FetchImagesFromDatabase(size_t url_index,
                               std::vector<std::string> urls,
                               int width_px,
                               int height_px,
                               ImageFetchedCallback callback);
  void OnImageFetchedFromDatabase(size_t url_index,
                                  std::vector<std::string> urls,
                                  int width_px,
                                  int height_px,
                                  ImageFetchedCallback callback,
                                  const std::string& image_data);
  void OnImageDecodedFromDatabase(size_t url_index,
                                  std::vector<std::string> urls,
                                  int width_px,
                                  int height_px,
                                  ImageFetchedCallback callback,
                                  const gfx::Image& image);

  // Network
  void FetchImageFromNetwork(size_t url_index,
                             std::vector<std::string> urls,
                             int width_px,
                             int height_px,
                             ImageFetchedCallback callback);
  void OnImageFetchedFromNetwork(
      size_t url_index,
      std::vector<std::string> urls,
      int width_px,
      int height_px,
      ImageFetchedCallback callback,
      const std::string& image_data,
      const image_fetcher::RequestMetadata& request_metadata);
  void OnImageDecodedFromNetwork(size_t url_index,
                                 std::vector<std::string> urls,
                                 int width_px,
                                 int height_px,
                                 ImageFetchedCallback callback,
                                 const std::string& image_data,
                                 const gfx::Image& image);

  // Garbage collection will be run when FeedImageManager starts up, and then
  // once a day. Garbage collection will remove images, that have not been
  // touched for 30 days.
  void DoGarbageCollectionIfNeeded();
  void OnGarbageCollectionDone(base::Time garbage_collected_day, bool success);
  void StopGarbageCollection();

  void ClearUmaTimer(const std::string& url);

  // The day which image database already ran garbage collection against on.
  base::Time image_garbage_collected_day_;

  base::OneShotTimer garbage_collection_timer_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  std::unique_ptr<FeedImageDatabase> image_database_;

  // Track time it takes to get images.
  base::flat_map<std::string, base::ElapsedTimer> url_timers_;

  base::WeakPtrFactory<FeedImageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedImageManager);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_IMAGE_MANAGER_H_
