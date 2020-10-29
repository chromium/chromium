// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_

#include <string>

#include "base/optional.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
namespace visual_utils {

using QuantizedColor = uint32_t;

// Utility methods for working with QuantizedColors.
QuantizedColor SkColorToQuantizedColor(SkColor color);
int GetQuantizedR(QuantizedColor color);
int GetQuantizedG(QuantizedColor color);
int GetQuantizedB(QuantizedColor color);

// Computes the color histogram for the image. This buckets the pixels according
// to their QuantizedColor, then reports their weight and centroid.
bool GetHistogramForImage(const SkBitmap& image,
                          VisualFeatures::ColorHistogram* histogram);

// Computes the BlurredImage for the given input image. This involves
// downsampling the image to a certain fixed resolution, then blurring
// by taking an average over fixed-size blocks of pixels.
bool GetBlurredImage(const SkBitmap& image,
                     VisualFeatures::BlurredImage* blurred_image);

// Downsizes an image by averaging all the pixels in the source image that
// contribute to the target image. Groups pixels into squares of size
// |block_size|, potentially with partial blocks at the edge. The output
// image has pixels the average of the pixels in each block.
std::unique_ptr<SkBitmap> BlockMeanAverage(const SkBitmap& image,
                                           int block_size);

// Returns the hash used to compare blurred images.
std::string GetHashFromBlurredImage(VisualFeatures::BlurredImage blurred_image);

// Returns whether the given |image| is a match for the |target|. Returns
// nullopt in the case of no match, and the VisionMatchResult if it is a match.
base::Optional<VisionMatchResult> IsVisualMatch(const SkBitmap& image,
                                                const VisualTarget& target);

}  // namespace visual_utils
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_VISUAL_UTILS_H_
