// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_

#include <string>

#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Image;
}

namespace safe_browsing::visual_utils {

// Enum used to represent the result of the function |CanExtractVisualFeatures|.
enum class CanExtractVisualFeaturesResult {
  kCanExtractVisualFeatures = 0,
  kUserNotOptedIn = 1,
  kOffTheRecord = 2,
  kBelowMinFrame = 3,
  kAboveZoomLevel = 4,
  kMaxValue = kAboveZoomLevel,
};

// Computes the BlurredImage for the given input image. This involves
// downsampling the image to a certain fixed resolution, then blurring
// by taking an average over fixed-size blocks of pixels.
bool GetBlurredImage(const SkBitmap& image,
                     VisualFeatures::BlurredImage* blurred_image);

// Generates VisualFeatures::Screenshot for the passed-in `screenshot`.
void EncodeScreenshot(const SkBitmap& screenshot,
                      VisualFeatures::Screenshot* encoded_screenshot);

// Downsizes an image by averaging all the pixels in the source image that
// contribute to the target image. Groups pixels into squares of size
// |block_size|, potentially with partial blocks at the edge. The output
// image has pixels the average of the pixels in each block.
std::unique_ptr<SkBitmap> BlockMeanAverage(const SkBitmap& image,
                                           int block_size);

// Whether we can extract visual features from a page with a given size and zoom
// level. For password protection, the safe browsing preference that allows the
// extraction are extended reporting and enhanced safe browsing, whereas for
// client side detection, it's just enhanced safe browsing.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
CanExtractVisualFeaturesResult CanExtractVisualFeatures(bool is_user_opted_in,
                                                        bool is_off_the_record,
                                                        gfx::Size size);
#else
CanExtractVisualFeaturesResult CanExtractVisualFeatures(bool is_user_opted_in,
                                                        bool is_off_the_record,
                                                        gfx::Size size,
                                                        double zoom_level);
#endif

// Converts and prepares a `gfx::Image` into an `SkBitmap` for visual feature
// extraction. Must be called on the sequence that owns `image`.
// On iOS, this uses platform-native hardware downscaling before bitmap
// conversion to avoid allocating full-resolution intermediate bitmaps. On other
// platforms, this returns the image's `SkBitmap` representation directly
// without performing any downscaling.
SkBitmap GetBitmapForVisualFeatures(const gfx::Image& image);

// Extract a VisualFeatures proto from the given `bitmap` (used by password
// protection and client-side phishing detection).
std::unique_ptr<VisualFeatures> ExtractVisualFeatures(const SkBitmap& bitmap);

}  // namespace safe_browsing::visual_utils

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_
