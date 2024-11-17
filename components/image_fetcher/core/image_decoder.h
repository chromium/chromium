// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

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
  ImageDecoder() = default;

  ImageDecoder(const ImageDecoder&) = delete;
  ImageDecoder& operator=(const ImageDecoder&) = delete;

  virtual ~ImageDecoder() = default;

  // Decodes the passed |image_data| and runs the given callback. The callback
  // is run even if decoding the image fails. In case an error occured during
  // decoding the image an empty image is passed to the callback.
  // For images with multiple frames (e.g. ico files), a frame with a size as
  // close as possible to |desired_image_frame_size| is chosen (tries to take
  // one in larger size if there's no precise match). Passing gfx::Size() as
  // |desired_image_frame_size| is also supported and will result in chosing the
  // smallest available size. Pass |data_decoder| to batch multiple image
  // decodes in the same process. If |data_decoder| is null, a new process will
  // be created to decode this image. |data_decoder| must outlive the
  // ImageDecoder.
  virtual void DecodeImage(const std::string& image_data,
                           const gfx::Size& desired_image_frame_size,
                           data_decoder::DataDecoder* data_decoder,
                           ImageDecodedCallback callback) = 0;
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DECODER_H_
