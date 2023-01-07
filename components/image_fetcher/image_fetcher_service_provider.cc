// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_fetcher_service_provider.h"

#include "base/no_destructor.h"

namespace image_fetcher {

namespace {

ImageFetcherServiceProvider* GetImageFetcherServiceProvider() {
  static base::NoDestructor<ImageFetcherServiceProvider> provider;
  return provider.get();
}

ImageFetcherCachePathProvider* GetImageFetcherCachePathProvider() {
  static base::NoDestructor<ImageFetcherCachePathProvider> provider;
  return provider.get();
}

}  // namespace

// static
void SetImageFetcherServiceProvider(
    const ImageFetcherServiceProvider& provider) {
  *GetImageFetcherServiceProvider() = provider;
}

// static
ImageFetcherService* GetImageFetcherService(SimpleFactoryKey* key) {
  return GetImageFetcherServiceProvider()->Run(key);
}

// static
void SetImageFetcherCachePathProvider(
    const ImageFetcherCachePathProvider& provider) {
  *GetImageFetcherCachePathProvider() = provider;
}

// static
std::string GetImageFetcherCachePath(SimpleFactoryKey* key, std::string path) {
  return GetImageFetcherCachePathProvider()->Run(key, path);
}

}  // namespace image_fetcher