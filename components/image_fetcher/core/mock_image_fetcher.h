// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_FETCHER_H_

#include "components/image_fetcher/core/image_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace image_fetcher {

class MockImageFetcher : public ImageFetcher {
 public:
  MockImageFetcher();
  ~MockImageFetcher() override;

  MOCK_METHOD4(FetchImageAndData_,
               void(const GURL&,
                    ImageDataFetcherCallback*,
                    ImageFetcherCallback*,
                    ImageFetcherParams));
  void FetchImageAndData(const GURL& image_url,
                         ImageDataFetcherCallback image_data_callback,
                         ImageFetcherCallback image_callback,
                         ImageFetcherParams params) override;
  MOCK_METHOD0(GetImageDecoder, image_fetcher::ImageDecoder*());
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_FETCHER_H_
