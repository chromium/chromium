// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/mock_image_fetcher.h"

#include <utility>

#include "ui/gfx/geometry/size.h"

namespace image_fetcher {

MockImageFetcher::MockImageFetcher() = default;
MockImageFetcher::~MockImageFetcher() = default;

void MockImageFetcher::FetchImageAndData(
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    ImageFetcherParams params) {
  FetchImageAndData_(image_url, &image_data_callback, &image_callback,
                     std::move(params));
}

}  // namespace image_fetcher
