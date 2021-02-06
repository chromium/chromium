// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_REDUCED_MODE_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_REDUCED_MODE_IMAGE_FETCHER_H_

#include <string>

#include "base/sequence_checker.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "url/gurl.h"

namespace image_fetcher {

class ImageFetcher;

// ReducedModeImageFetcher is used when Chrome is running in reduced mode. This
// image fetcher defers image decoding during fetching, since decoding in the
// utility process isn't available in the reduced mode. It ignores the
// ImageFetcherCallback, but will return the fetched but not-transcoding image
// to users by calling ImageDataFetcherCallback.
class ReducedModeImageFetcher : public ImageFetcher {
 public:
  ReducedModeImageFetcher(ImageFetcher* image_fetcher);
  ~ReducedModeImageFetcher() override;

  // ImageFetcher:
  void FetchImageAndData(const GURL& image_url,
                         ImageDataFetcherCallback image_data_callback,
                         ImageFetcherCallback image_callback,
                         ImageFetcherParams params) override;
  ImageDecoder* GetImageDecoder() override;

 private:
  // Owned by ImageFetcherService.
  ImageFetcher* image_fetcher_;

  // Used to ensure that operations are performed on the sequence that this
  // object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ReducedModeImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_REDUCED_MODE_IMAGE_FETCHER_H_
