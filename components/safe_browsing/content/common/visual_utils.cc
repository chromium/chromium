// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/visual_utils.h"

#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_utils.h"

namespace safe_browsing::visual_utils {

namespace {

// WARNING: The following parameters are highly privacy and performance
// sensitive. These should not be changed without thorough review.
#if BUILDFLAG(IS_ANDROID)
const int kPHashDownsampleWidth = 108;
const int kPHashDownsampleHeight = 192;
const int kMinWidthForVisualFeatures = 258;
const int kMinHeightForVisualFeatures = 258;
#else
const int kPHashDownsampleWidth = 288;
const int kPHashDownsampleHeight = 288;
const int kMinWidthForVisualFeatures = 576;
const int kMinHeightForVisualFeatures = 576;
#endif
const int kPHashBlockSize = 6;

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Parameters chosen to ensure privacy is preserved by visual features.
const float kMaxZoomForVisualFeatures = 2.0;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

int GetPHashDownsampleWidth() {
  if (base::FeatureList::IsEnabled(kVisualFeaturesSizes)) {
    return base::GetFieldTrialParamByFeatureAsInt(
        kVisualFeaturesSizes, "phash_width", kPHashDownsampleWidth);
  }

  return kPHashDownsampleWidth;
}

int GetPHashDownsampleHeight() {
  if (base::FeatureList::IsEnabled(kVisualFeaturesSizes)) {
    return base::GetFieldTrialParamByFeatureAsInt(
        kVisualFeaturesSizes, "phash_height", kPHashDownsampleHeight);
  }

  return kPHashDownsampleHeight;
}

int GetMinWidthForVisualFeatures() {
  if (base::FeatureList::IsEnabled(kVisualFeaturesSizes)) {
    return base::GetFieldTrialParamByFeatureAsInt(
        kVisualFeaturesSizes, "min_width", kMinWidthForVisualFeatures);
  }

  return kMinWidthForVisualFeatures;
}

int GetMinHeightForVisualFeatures() {
  if (base::FeatureList::IsEnabled(kVisualFeaturesSizes)) {
    return base::GetFieldTrialParamByFeatureAsInt(
        kVisualFeaturesSizes, "min_height", kMinHeightForVisualFeatures);
  }

  return kMinHeightForVisualFeatures;
}

}  // namespace

bool GetBlurredImage(const SkBitmap& image,
                     VisualFeatures::BlurredImage* blurred_image) {
  TRACE_EVENT0("safe_browsing", "GetBlurredImage");
  if (image.drawsNothing())
    return false;

  // Use the Rec. 2020 color space, in case the user input is wide-gamut.
  sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
      {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
      SkNamedGamut::kRec2020);

  // We scale down twice, once with medium quality, then with a block mean
  // average to be consistent with the backend.
  // TODO(drubery): Investigate whether this is necessary for performance or
  // not.
  SkBitmap downsampled = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_GOOD, GetPHashDownsampleWidth(),
      GetPHashDownsampleHeight());

  std::unique_ptr<SkBitmap> blurred =
      BlockMeanAverage(downsampled, kPHashBlockSize);

  blurred_image->set_width(blurred->width());
  blurred_image->set_height(blurred->height());

  const int data_size = blurred->width() * blurred->height();
  blurred_image->mutable_data()->reserve(data_size);

  for (int y = 0; y < blurred->height(); ++y) {
    for (int x = 0; x < blurred->width(); ++x) {
      SkColor color = blurred->getColor(x, y);
      *blurred_image->mutable_data() += static_cast<char>(SkColorGetR(color));
      *blurred_image->mutable_data() += static_cast<char>(SkColorGetG(color));
      *blurred_image->mutable_data() += static_cast<char>(SkColorGetB(color));
    }
  }

  return true;
}

std::unique_ptr<SkBitmap> BlockMeanAverage(const SkBitmap& image,
                                           int block_size) {
  // Compute the number of blocks in the target image, rounding up to account
  // for partial blocks.
  int num_blocks_high =
      std::ceil(static_cast<float>(image.height()) / block_size);
  int num_blocks_wide =
      std::ceil(static_cast<float>(image.width()) / block_size);

  SkImageInfo target_info = SkImageInfo::MakeN32(
      num_blocks_wide, num_blocks_high, SkAlphaType::kUnpremul_SkAlphaType,
      image.refColorSpace());
  auto target = std::make_unique<SkBitmap>();
  if (!target->tryAllocPixels(target_info))
    return target;

  for (int block_x = 0; block_x < num_blocks_wide; block_x++) {
    for (int block_y = 0; block_y < num_blocks_high; block_y++) {
      int r_total = 0, g_total = 0, b_total = 0, sample_count = 0;

      // Compute boundary for the current block, taking into account the
      // possibility of partial blocks near the edges.
      int x_start = block_x * block_size;
      int x_end = std::min(x_start + block_size, image.width());

      int y_start = block_y * block_size;
      int y_end = std::min(y_start + block_size, image.height());
      for (int i = x_start; i < x_end; i++) {
        for (int j = y_start; j < y_end; j++) {
          const SkColor color = image.getColor(i, j);
          r_total += SkColorGetR(color);
          g_total += SkColorGetG(color);
          b_total += SkColorGetB(color);
          sample_count++;
        }
      }

      int r_mean = r_total / sample_count;
      int g_mean = g_total / sample_count;
      int b_mean = b_total / sample_count;

      *target->getAddr32(block_x, block_y) =
          SkPackARGB32(255, r_mean, g_mean, b_mean);
    }
  }

  return target;
}

#if BUILDFLAG(IS_ANDROID)
CanExtractVisualFeaturesResult CanExtractVisualFeatures(
    bool is_extended_reporting,
    bool is_off_the_record,
    gfx::Size size) {
#else
CanExtractVisualFeaturesResult CanExtractVisualFeatures(
    bool is_extended_reporting,
    bool is_off_the_record,
    gfx::Size size,
    double zoom_level) {
#endif
  if (!is_extended_reporting)
    return CanExtractVisualFeaturesResult::kNotExtendedReporting;

  if (is_off_the_record)
    return CanExtractVisualFeaturesResult::kOffTheRecord;

  if (size.width() < GetMinWidthForVisualFeatures() ||
      size.height() < GetMinHeightForVisualFeatures())
    return CanExtractVisualFeaturesResult::kBelowMinFrame;

#if !BUILDFLAG(IS_ANDROID)
  if (zoom_level > kMaxZoomForVisualFeatures) {
    return CanExtractVisualFeaturesResult::kAboveZoomLevel;
  }
#endif
  return CanExtractVisualFeaturesResult::kCanExtractVisualFeatures;
}

std::unique_ptr<VisualFeatures> ExtractVisualFeatures(
    const SkBitmap& screenshot) {
  auto features = std::make_unique<VisualFeatures>();
  GetBlurredImage(screenshot, features->mutable_image());
  return features;
}

}  // namespace safe_browsing::visual_utils
