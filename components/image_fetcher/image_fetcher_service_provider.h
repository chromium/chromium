// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_PROVIDER_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_PROVIDER_H_

#include "base/functional/callback.h"

class SimpleFactoryKey;

namespace image_fetcher {

class ImageFetcherService;

using ImageFetcherServiceProvider =
    base::RepeatingCallback<ImageFetcherService*(SimpleFactoryKey* key)>;
using ImageFetcherCachePathProvider =
    base::RepeatingCallback<std::string(SimpleFactoryKey* key,
                                        std::string path)>;

void SetImageFetcherServiceProvider(
    const ImageFetcherServiceProvider& provider);
ImageFetcherService* GetImageFetcherService(SimpleFactoryKey* key);

void SetImageFetcherCachePathProvider(
    const ImageFetcherCachePathProvider& provider);
std::string GetImageFetcherCachePath(SimpleFactoryKey* key, std::string path);

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_PROVIDER_H_