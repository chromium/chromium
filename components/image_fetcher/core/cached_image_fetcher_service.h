// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_SERVICE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace image_fetcher {

class CachedImageFetcher;
class ImageCache;
class ImageDecoder;

using CreateImageDecoderCallback =
    base::RepeatingCallback<std::unique_ptr<ImageDecoder>()>;

// Keyed service responsible for managing the lifetime of CachedImageFetcher.
// Persists the ImageCache, and uses it to create instances of the
// CachedImageFethcer.
class CachedImageFetcherService : public KeyedService {
 public:
  explicit CachedImageFetcherService(
      CreateImageDecoderCallback create_image_decoder_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<ImageCache> image_cache,
      bool read_only);
  ~CachedImageFetcherService() override;

  // Create an instance of CachedImageFetcher based on the ImageCache.
  std::unique_ptr<CachedImageFetcher> CreateCachedImageFetcher();

 private:
  CreateImageDecoderCallback create_image_decoder_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<ImageCache> image_cache_;

  // If true, the CachedImageFetcher will be started in read-only mode. Read-
  // only mode doesn't perform write operations on the cache.
  bool read_only_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherService);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_SERVICE_H_
