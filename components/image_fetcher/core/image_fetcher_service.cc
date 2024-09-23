// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_service.h"

#include <utility>

#include "base/time/clock.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/reduced_mode_image_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace image_fetcher {

ImageFetcherService::ImageFetcherService(
    std::unique_ptr<ImageDecoder> image_decoder,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<ImageCache> image_cache,
    bool read_only)
    : image_cache_(image_cache),
      image_fetcher_(
          std::make_unique<ImageFetcherImpl>(std::move(image_decoder),
                                             url_loader_factory)),
      cached_image_fetcher_(
          std::make_unique<CachedImageFetcher>(image_fetcher_.get(),
                                               image_cache,
                                               read_only)),
      reduced_mode_image_fetcher_(std::make_unique<ReducedModeImageFetcher>(
          cached_image_fetcher_.get())) {}

ImageFetcherService::~ImageFetcherService() = default;

ImageFetcher* ImageFetcherService::GetImageFetcher(ImageFetcherConfig config) {
  switch (config) {
    case ImageFetcherConfig::kNetworkOnly:
      return image_fetcher_.get();
    case ImageFetcherConfig::kDiskCacheOnly:
      return cached_image_fetcher_.get();
    // Only available in Java, so return a plain image fetcher here.
    case ImageFetcherConfig::kInMemoryOnly:
      return image_fetcher_.get();
    // In memory portion is only available in Java.
    case ImageFetcherConfig::kInMemoryWithDiskCache:
      return cached_image_fetcher_.get();
    case ImageFetcherConfig::kReducedMode:
      return reduced_mode_image_fetcher_.get();
    default:
      // Provided ImageFetcherConfig not in the enum.
      NOTREACHED_IN_MIGRATION();
  }

  return nullptr;
}

scoped_refptr<ImageCache> ImageFetcherService::ImageCacheForTesting() const {
  return image_cache_;
}

}  // namespace image_fetcher
