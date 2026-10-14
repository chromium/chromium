// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/core/common/visual_utils.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/gfx/geometry/size.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_util.h"

namespace safe_browsing::visual_utils {

SkBitmap GetResizedImageAsSkBitmap(const gfx::Image& image,
                                   const gfx::Size& size) {
  if (image.IsEmpty() || image.Size().IsEmpty() || size.IsEmpty()) {
    return SkBitmap();
  }

  // `gfx::ResizedImage()` draws into a `UIGraphicsBeginImageContext()`, which
  // uses a scale of 1.0, so the returned `UIImage` is backed by a `CGImage` of
  // exactly `size` pixels. Converting that `CGImage` directly keeps the
  // operation to a single resample.
  //
  // `Image::AsBitmap()` is intentionally avoided: it routes through
  // `ImageSkiaFromUIImage()`, which requests a bitmap of
  // `size * GetScaleForMaxSupportedResourceScaleFactor()` and upsamples the
  // image that was just rendered at the target pixel dimensions.
  UIImage* resized_image = gfx::ResizedImage(image, size).ToUIImage();
  CGImageRef cg_image = resized_image.CGImage;
  if (!cg_image) {
    return SkBitmap();
  }

  // Verify that `gfx::ResizedImage()` rendered at scale 1.0. If it ever
  // switches to device screen scale (e.g. via `UIGraphicsImageRenderer`),
  // `cg_image` would be allocated at 2x/3x dimensions and `CGImageToSkBitmap()`
  // would silently downscale it back to `size`, causing an unnecessary
  // intermediate memory allocation and degrading visual quality via a second
  // resample pass.
  DCHECK_EQ(CGImageGetWidth(cg_image), static_cast<size_t>(size.width()));
  DCHECK_EQ(CGImageGetHeight(cg_image), static_cast<size_t>(size.height()));
  return skia::CGImageToSkBitmap(cg_image, size.ToCGSize(),
                                 /*is_opaque=*/false);
}

}  // namespace safe_browsing::visual_utils
