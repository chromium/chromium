// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace image_fetcher {

using ImageDecodedCallback = base::OnceCallback<void(const gfx::Image&)>;

// ImageDecoder defines the common interface for decoding images. This is
// expected to process untrusted input from the web so implementations must be
// sure to decode safely.
class ImageDecoder {
 public:
  ImageDecoder() {}
  virtual ~ImageDecoder() {}

  // Decodes the passed |image_data| and runs the given callback. The callback
  // is run even if decoding the image fails. In case an error occured during
  // decoding the image an empty image is passed to the callback.
  // For images with multiple frames (e.g. ico files), a frame with a size as
  // close as possible to |desired_image_frame_size| is chosen (tries to take
  // one in larger size if there's no precise match). Passing gfx::Size() as
  // |desired_image_frame_size| is also supported and will result in chosing the
  // smallest available size.
  virtual void DecodeImage(const std::string& image_data,
                           const gfx::Size& desired_image_frame_size,
                           ImageDecodedCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_
