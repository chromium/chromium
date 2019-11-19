// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/reduced_mode_image_fetcher.h"

namespace image_fetcher {

ReducedModeImageFetcher::ReducedModeImageFetcher(ImageFetcher* image_fetcher)
    : image_fetcher_(image_fetcher) {
  DCHECK(image_fetcher_);
}

ReducedModeImageFetcher::~ReducedModeImageFetcher() = default;

ImageDecoder* ReducedModeImageFetcher::GetImageDecoder() {
  return nullptr;
}

void ReducedModeImageFetcher::FetchImageAndData(
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    ImageFetcherParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image_data_callback.is_null());

  params.set_skip_transcoding(true);
  params.set_allow_needs_transcoding_file(true);

  image_fetcher_->FetchImageAndData(image_url, std::move(image_data_callback),
                                    ImageFetcherCallback(), params);
}

}  //  namespace image_fetcher
