// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#import "components/image_fetcher/ios/webp_decoder.h"
#include "ios/web/public/thread/web_thread.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace image_fetcher {

class IOSImageDecoderImpl : public ImageDecoder {
 public:
  explicit IOSImageDecoderImpl();
  ~IOSImageDecoderImpl() override;

  // Note, that |desired_image_frame_size| is not supported
  // (http://crbug/697596).
  void DecodeImage(const std::string& image_data,
                   const gfx::Size& desired_image_frame_size,
                   ImageDecodedCallback callback) override;

 private:
  void CreateUIImageAndRunCallback(ImageDecodedCallback callback,
                                   NSData* image_data);

  // The task runner used to decode images if necessary.
  const scoped_refptr<base::TaskRunner> task_runner_ =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // The WeakPtrFactory is used to cancel callbacks if ImageFetcher is
  // destroyed during WebP decoding.
  base::WeakPtrFactory<IOSImageDecoderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSImageDecoderImpl);
};

IOSImageDecoderImpl::IOSImageDecoderImpl() : weak_factory_(this) {
  DCHECK(task_runner_.get());
}

IOSImageDecoderImpl::~IOSImageDecoderImpl() {}

void IOSImageDecoderImpl::DecodeImage(const std::string& image_data,
                                      const gfx::Size& desired_image_frame_size,
                                      ImageDecodedCallback callback) {
  // Convert the |image_data| std::string to an NSData buffer.
  // The data is copied as it may have to outlive the caller in
  // PostTaskAndReplyWithResult.
  NSData* data =
      [NSData dataWithBytes:image_data.data() length:image_data.size()];

  // The WebP image format is not supported by iOS natively. Therefore WebP
  // images need to be decoded explicitly,
  NSData* (^decodeBlock)();
  if (webp_transcode::WebpDecoder::IsWebpImage(image_data)) {
    decodeBlock = ^NSData*() {
      return webp_transcode::WebpDecoder::DecodeWebpImage(data);
    };
  } else {
    // Use block to prevent |data| from being released.
    decodeBlock = ^NSData*() { return data; };
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(decodeBlock),
      base::BindOnce(&IOSImageDecoderImpl::CreateUIImageAndRunCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IOSImageDecoderImpl::CreateUIImageAndRunCallback(
    ImageDecodedCallback callback,
    NSData* image_data) {
  // Decode the image data using UIImage.
  if (image_data) {
    // "Most likely" always returns 1x images.
    UIImage* ui_image = [UIImage imageWithData:image_data scale:1];
    if (ui_image) {
      gfx::Image gfx_image(ui_image);
      std::move(callback).Run(gfx_image);
      return;
    }
  }
  gfx::Image empty_image;
  std::move(callback).Run(empty_image);
}

std::unique_ptr<ImageDecoder> CreateIOSImageDecoder() {
  return std::make_unique<IOSImageDecoderImpl>();
}

}  // namespace image_fetcher
