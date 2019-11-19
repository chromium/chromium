// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_SERVICE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace image_fetcher {

class ImageFetcher;
class ImageCache;
class ImageDecoder;

// Enumerate the possible image fetcher combinations to allow the service to
// configure the correct one. New values should be added at the end and things
// should not be renumbered.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.image_fetcher)
enum class ImageFetcherConfig {
  kNetworkOnly = 0,
  kDiskCacheOnly = 1,
  // In memory cache without disk caching. Currently only available in Java.
  kInMemoryOnly = 2,
  // In memory cache with disk caching. Currently only available in Java.
  kInMemoryWithDiskCache = 3,
  // Deferring image transcoding when fetching. This is because utility process
  // isn't created in the reduced mode, thus the image decoding in the utility
  // process is deferred until full browser starts. The ReducedModeImageFetcher
  // will ignore any ImageFetcherCallback which asks transcoded images.
  kReducedMode = 4,
  kMaxValue = kReducedMode
};

// Keyed service responsible for managing the lifetime of various ImageFetcher
// services. Allows access to instances of  the disk cache, network fetcher and
// cached image fetcher.
class ImageFetcherService : public KeyedService {
 public:
  explicit ImageFetcherService(
      std::unique_ptr<ImageDecoder> image_decoder,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<ImageCache> image_cache,
      bool read_only);
  ~ImageFetcherService() override;

  // Get an image fetcher according to the given config.
  ImageFetcher* GetImageFetcher(ImageFetcherConfig config);

  scoped_refptr<ImageCache> ImageCacheForTesting() const;

 private:
  // TODO(wylieb): Make this a unique_ptr.
  scoped_refptr<ImageCache> image_cache_;

  // This fetcher goes directly to the network.
  std::unique_ptr<ImageFetcher> image_fetcher_;
  // This fetcher goes through a disk cache before going to the network.
  std::unique_ptr<ImageFetcher> cached_image_fetcher_;
  // This fetcher goes through a disk cache before going to the network, but
  // defers image transcoding when fetching.
  std::unique_ptr<ImageFetcher> reduced_mode_image_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherService);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_SERVICE_H_
