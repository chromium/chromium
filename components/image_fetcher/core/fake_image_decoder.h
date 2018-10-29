// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_FAKE_IMAGE_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_FAKE_IMAGE_DECODER_H_

#include <string>

#include "base/bind.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_types.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace image_fetcher {

class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  FakeImageDecoder();
  ~FakeImageDecoder() override;

  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override;
  void SetEnabled(bool enabled);
  void SetBeforeImageDecoded(const base::RepeatingClosure& callback);
  void SetDecodingValid(bool valid);

 private:
  bool enabled_;
  bool valid_;
  base::RepeatingClosure before_image_decoded_;
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_FAKE_IMAGE_DECODER_H_
