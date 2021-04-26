// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_

#include <memory>

#include "components/image_fetcher/core/image_fetcher.h"
#include "components/query_tiles/internal/image_loader.h"

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace query_tiles {

// Loads image with image fetcher service, which provides a disk cache to reduce
// network data consumption.
class CachedImageLoader : public ImageLoader {
 public:
  CachedImageLoader(image_fetcher::ImageFetcher* cached_image_fetcher,
                    image_fetcher::ImageFetcher* reduced_mode_image_fetcher);
  ~CachedImageLoader() override;

 private:
  // ImageLoader implementation.
  void FetchImage(const GURL& url, BitmapCallback callback) override;
  void PrefetchImage(const GURL& url, SuccessCallback callback) override;

  // Used to load the image bitmap for UI. Owned by ImageFetcherService.
  // Outlives TileService.
  image_fetcher::ImageFetcher* cached_image_fetcher_;

  // Used to prefetch the image in reduced mode. The data is downloaded to disk
  // without decoding. Owned by ImageFetcherService. Outlives TileService.
  image_fetcher::ImageFetcher* reduced_mode_image_fetcher_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_
