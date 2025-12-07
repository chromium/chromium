// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/composebox/ios/composebox_image_helper_ios.h"

#include "base/apple/foundation_util.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "components/lens/lens_bitmap_processing.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Resizes a UIImage to a target size in pixels.
UIImage* ResizeImage(UIImage* image, const gfx::Size& pixel_size) {
  if (!image) {
    return nil;
  }

  CGRect rect = CGRectMake(0, 0, pixel_size.width(), pixel_size.height());
  UIGraphicsImageRendererFormat* format =
      [[UIGraphicsImageRendererFormat alloc] init];
  // Ensure the output image is 1x scale, as we are working with raw pixels.
  format.scale = 1.0;
  // The image may have transparency.
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:rect.size format:format];

  UIImage* resized_image = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        [image drawInRect:rect];
      }];

  return resized_image;
}

}  // namespace

namespace composebox {

lens::ImageData DownscaleAndEncodeImage(
    UIImage* image,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    const lens::ImageEncodingOptions& image_options) {
  lens::ImageData image_data;
  if (!image) {
    return image_data;
  }

  gfx::Size original_pixel_size(image.size.width * image.scale,
                                image.size.height * image.scale);

  UIImage* image_to_encode = image;
  gfx::Size final_pixel_size = original_pixel_size;

  if (lens::ShouldDownscaleSize(original_pixel_size, image_options.max_size,
                                image_options.max_width,
                                image_options.max_height)) {
    final_pixel_size = lens::GetPreferredSize(
        original_pixel_size, image_options.max_width, image_options.max_height);
    lens::AddClientLogsForDownscale(client_logs, original_pixel_size,
                                    final_pixel_size);
    image_to_encode = ResizeImage(image, final_pixel_size);
  }

  if (!image_to_encode) {
    return image_data;
  }

  // For iOS, we'll use JPEG encoding. WebP is not natively supported.
  NSData* data = UIImageJPEGRepresentation(
      image_to_encode,
      static_cast<CGFloat>(image_options.compression_quality) / 100.0);

  if (data) {
    scoped_refptr<base::RefCountedBytes> encoded_data =
        base::MakeRefCounted<base::RefCountedBytes>(
            base::ToVector(base::apple::NSDataToSpan(data)));
    lens::AddClientLogsForEncode(client_logs, encoded_data);

    image_data.mutable_image_metadata()->set_height(final_pixel_size.height());
    image_data.mutable_image_metadata()->set_width(final_pixel_size.width());
    image_data.mutable_payload()->mutable_image_bytes()->assign(
        encoded_data->begin(), encoded_data->end());
  }

  return image_data;
}

}  // namespace composebox
