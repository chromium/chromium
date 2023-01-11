// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_DECODER_H_

#include "base/functional/callback.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace image_fetcher {

class MockImageDecoder : public image_fetcher::ImageDecoder {
 public:
  MockImageDecoder();
  ~MockImageDecoder() override;
  MOCK_METHOD4(DecodeImage,
               void(const std::string& image_data,
                    const gfx::Size& desired_image_frame_size,
                    data_decoder::DataDecoder* data_decoder,
                    image_fetcher::ImageDecodedCallback callback));
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_MOCK_IMAGE_DECODER_H_
