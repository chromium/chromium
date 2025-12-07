// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#import "base/ios/ios_util.h"
#include "base/task/thread_pool.h"
#include "ios/web/public/thread/web_thread.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

class IOSImageDecoderImpl : public ImageDecoder {
 public:
  explicit IOSImageDecoderImpl();

  IOSImageDecoderImpl(const IOSImageDecoderImpl&) = delete;
  IOSImageDecoderImpl& operator=(const IOSImageDecoderImpl&) = delete;

  ~IOSImageDecoderImpl() override;

  // Note, that |desired_image_frame_size| is not supported
  // (http://crbug/697596).
  void DecodeImage(const std::string& image_data,
                   const gfx::Size& desired_image_frame_size,
                   data_decoder::DataDecoder* data_decoder,
                   ImageDecodedCallback callback) override;
};

IOSImageDecoderImpl::IOSImageDecoderImpl() = default;

IOSImageDecoderImpl::~IOSImageDecoderImpl() = default;

void IOSImageDecoderImpl::DecodeImage(const std::string& image_data,
                                      const gfx::Size& desired_image_frame_size,
                                      data_decoder::DataDecoder* data_decoder,
                                      ImageDecodedCallback callback) {
  // Convert the |image_data| std::string to an NSData buffer.
  NSData* data =
      [NSData dataWithBytes:image_data.data() length:image_data.size()];

  // Decode the image data using UIImage.
  // "Most likely" always returns 1x images.
  UIImage* ui_image = [UIImage imageWithData:data scale:1];
  gfx::Image gfx_image;
  if (ui_image) {
    gfx_image = gfx::Image(ui_image);
  }
  std::move(callback).Run(gfx_image);
}

std::unique_ptr<ImageDecoder> CreateIOSImageDecoder() {
  return std::make_unique<IOSImageDecoderImpl>();
}

}  // namespace image_fetcher
