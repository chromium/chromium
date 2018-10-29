// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher_service.h"

#include <utility>

#include "base/time/clock.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace image_fetcher {

CachedImageFetcherService::CachedImageFetcherService(
    CreateImageDecoderCallback create_image_decoder_fn,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<ImageCache> image_cache,
    bool read_only)
    : create_image_decoder_callback_(create_image_decoder_fn),
      url_loader_factory_(url_loader_factory),
      image_cache_(image_cache),
      read_only_(read_only) {}

CachedImageFetcherService::~CachedImageFetcherService() = default;

// TODO(wylieb): Store CachedImageFetcher once it's stateless.
std::unique_ptr<CachedImageFetcher>
CachedImageFetcherService::CreateCachedImageFetcher() {
  return std::make_unique<CachedImageFetcher>(
      std::make_unique<ImageFetcherImpl>(create_image_decoder_callback_.Run(),
                                         url_loader_factory_),
      image_cache_, read_only_);
}

}  // namespace image_fetcher
