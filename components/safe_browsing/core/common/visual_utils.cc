// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/visual_utils.h"

#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/opencv/src/emd_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_utils.h"

namespace safe_browsing {
namespace visual_utils {

namespace {

// WARNING: The following parameters are highly privacy and performance
// sensitive. These should not be changed without thorough review.
#if defined(OS_ANDROID)
const int kPHashDownsampleWidth = 108;
const int kPHashDownsampleHeight = 192;
#else
const int kPHashDownsampleWidth = 288;
const int kPHashDownsampleHeight = 288;
#endif
const int kPHashBlockSize = 6;

// Constants for computing Earth Movers Distance (EMD) between ColorHistograms.
const float kMaxCentroidDistance = 1.414;
const float kMaxColorDistance = 14;

size_t GetBitCount(uint8_t byte) {
  size_t count = 0;
  for (; byte > 0; byte >>= 1) {
    if (byte & 1)
      count++;
  }

  return count;
}

bool GetHashDistance(const std::string& hash1,
                     const std::string& hash2,
                     size_t* out) {
  // The beginning of the hash is a Varint representing the stride. For values
  // at most 127, this is just the value of the first char. Chrome only
  // generates strides less than 128, so we can simply extract the first char.
  if (hash1[0] != hash2[0])
    return false;
  if (hash1[0] & 0x80)
    return false;

  size_t count = std::min(hash1.size(), hash2.size());
  size_t distance = 8 * (std::max(hash1.size(), hash2.size()) - count);
  for (size_t i = 1; i < count; i++) {
    distance += GetBitCount(hash1[i] ^ hash2[i]);
  }

  *out = distance;
  return true;
}

opencv::PointDistribution HistogramBinsToPointDistribution(
    const google::protobuf::RepeatedPtrField<VisualFeatures::ColorHistogramBin>&
        bins) {
  opencv::PointDistribution distribution;
  distribution.dimensions = 5;
  distribution.positions.reserve(bins.size());
  for (const VisualFeatures::ColorHistogramBin& bin : bins) {
    distribution.weights.push_back(bin.weight());
    std::vector<float> position(5);
    position[0] = bin.centroid_x() / kMaxCentroidDistance;
    position[1] = bin.centroid_y() / kMaxCentroidDistance;
    position[2] = bin.quantized_r() / kMaxColorDistance;
    position[3] = bin.quantized_g() / kMaxColorDistance;
    position[4] = bin.quantized_b() / kMaxColorDistance;
    distribution.positions.push_back(std::move(position));
  }

  return distribution;
}

// TODO(https://crbug.com/1196450): Remove this function once we are no longer
// using regular ColorRanges.
bool ImageHasColorInRange(const SkBitmap& image,
                          const MatchRule::ColorRange& color_range) {
  for (int i = 0; i < image.width(); i++) {
    for (int j = 0; j < image.height(); j++) {
      SkScalar hsv[3];
      SkColorToHSV(image.getColor(i, j), hsv);
      if (color_range.low() <= hsv[0] && hsv[0] <= color_range.high())
        return true;
    }
  }

  return false;
}

bool ImageHasColorInRange(const SkBitmap& image,
                          const MatchRule::FloatColorRange& color_range) {
  TRACE_EVENT0("safe_browsing", "ImageHasColorInRange");
  for (int i = 0; i < image.width(); i++) {
    for (int j = 0; j < image.height(); j++) {
      SkScalar hsv[3];
      SkColorToHSV(image.getColor(i, j), hsv);
      if (color_range.low() <= hsv[0] && hsv[0] <= color_range.high()) {
        return true;
      }
    }
  }

  return false;
}

uint8_t GetMedian(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> buffer;
  buffer.assign(data.begin(), data.end());
  std::vector<uint8_t>::iterator middle = buffer.begin() + (buffer.size() / 2);
  std::nth_element(buffer.begin(), middle, buffer.end());
  if (buffer.size() % 2 == 1) {
    return *middle;
  } else {
    // For even-sized sets, return the average of the two middle elements.
    return (*middle + *std::max_element(buffer.begin(), middle)) / 2;
  }
}

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

}  // namespace

// A QuantizedColor takes the highest 3 bits of R, G, and B, and concatenates
// them.
QuantizedColor SkColorToQuantizedColor(SkColor color) {
  return (SkColorGetR(color) >> 5) << 6 | (SkColorGetG(color) >> 5) << 3 |
         (SkColorGetB(color) >> 5);
}

int GetQuantizedR(QuantizedColor color) {
  return color >> 6;
}

int GetQuantizedG(QuantizedColor color) {
  return (color >> 3) & 7;
}

int GetQuantizedB(QuantizedColor color) {
  return color & 7;
}

struct ColorStats {
  int color_to_count = 0;
  double color_to_total_x = 0.0;
  double color_to_total_y = 0.0;

  absl::optional<QuantizedColor> quantized_color;
};

bool GetHistogramForImage(const SkBitmap& image,
                          VisualFeatures::ColorHistogram* histogram) {
  TRACE_EVENT0("safe_browsing", "GetHistogramForImage");
  if (image.drawsNothing())
    return false;

  int normalization_factor;
  if (!base::CheckMul(image.width(), image.height())
           .AssignIfValid(&normalization_factor))
    return false;

  // Indexed with colors in rgba order.
  std::unordered_map<uint32_t, ColorStats> stats_map;
  auto it = stats_map.end();

  // This code uses the unsafe getAddr32 function which assumes 32bpp depth.
  // Make sure the color type reflects that.
  CHECK_EQ(image.colorType(), SkColorType::kN32_SkColorType);
  for (int x = 0; x < image.width(); x++) {
    for (int y = 0; y < image.height(); y++) {
      // The conversions to QuantizedColor are avoided when possible to save
      // cycles but colors still need to be grouped in the same way they would
      // with the conversion. To achieve this mask out the bits that would be
      // ignored in the conversion.
      constexpr uint32_t kMask = 0xe0e0e000;
      uint32_t rgba = *image.getAddr32(x, y) & kMask;

      // If we're updating the same color as the last don't fetch uselessly
      // in the map and reuse the iterator we have.
      if (it == stats_map.end() || it->first != rgba) {
        it = stats_map.insert({rgba, ColorStats{}}).first;
      }

      // Update the color stats with the information from the current pixel.
      ColorStats& stats = it->second;
      stats.color_to_count++;
      stats.color_to_total_x += static_cast<float>(x);
      stats.color_to_total_y += static_cast<float>(y);

      // If we've never encountered this color before do the conversion. Avoid
      // it otherwise to save the overhead.
      if (!stats.quantized_color.has_value()) {
        stats.quantized_color.emplace(
            SkColorToQuantizedColor(image.getColor(x, y)));
      }
    }
  }

  for (const auto& entry : stats_map) {
    QuantizedColor color = entry.second.quantized_color.value();

    const ColorStats& stats = entry.second;
    int count = entry.second.color_to_count;

    VisualFeatures::ColorHistogramBin* bin = histogram->add_bins();
    bin->set_weight(static_cast<float>(count) / normalization_factor);
    bin->set_centroid_x(stats.color_to_total_x / count / image.width());
    bin->set_centroid_y(stats.color_to_total_y / count / image.height());
    bin->set_quantized_r(GetQuantizedR(color));
    bin->set_quantized_g(GetQuantizedG(color));
    bin->set_quantized_b(GetQuantizedB(color));
  }

  return true;
}

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
  SkImageInfo downsampled_info = SkImageInfo::MakeN32(
      GetPHashDownsampleWidth(), GetPHashDownsampleHeight(),
      SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  SkBitmap downsampled;
  if (!downsampled.tryAllocPixels(downsampled_info))
    return false;
  image.pixmap().scalePixels(
      downsampled.pixmap(),
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest));

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
          const QuantizedColor color = image.getColor(i, j);
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

std::string GetHashFromBlurredImage(
    VisualFeatures::BlurredImage blurred_image) {
  TRACE_EVENT0("safe_browsing", "GetHashFromBlurredImage");
  DCHECK_EQ(blurred_image.data().size(),
            3u * blurred_image.width() * blurred_image.height());
  // Convert the blurred image to grayscale.
  std::vector<uint8_t> luma_values;
  luma_values.resize(blurred_image.width() * blurred_image.height());
  for (size_t i = 0; i < luma_values.size(); i++) {
    luma_values[i] = color_utils::GetLuma(SkColorSetRGB(
        static_cast<unsigned char>(blurred_image.data()[3 * i]),
        static_cast<unsigned char>(blurred_image.data()[3 * i + 1]),
        static_cast<unsigned char>(blurred_image.data()[3 * i + 2])));
  }

  uint8_t median_luma = GetMedian(luma_values);

  std::string output;

  // The server-side hash generation writes a Varint here, rather than just an
  // int. These encodings coincide as long as the width is at most 127. If
  // we begin using larger widths, we need to implement the Varint encoding used
  // on the server.
  DCHECK_LT(blurred_image.width(), 128);
  output.push_back(static_cast<unsigned char>(blurred_image.width()));

  // Write the output bitstring.
  unsigned char next_byte = 0;
  int bits_encoded_so_far = 0;
  for (const uint8_t luma_value : luma_values) {
    next_byte <<= 1;
    if (luma_value >= median_luma)
      next_byte |= 1;

    ++bits_encoded_so_far;
    if (bits_encoded_so_far == 8) {
      output.push_back(next_byte);
      bits_encoded_so_far = 0;
    }
  }

  if (bits_encoded_so_far != 0) {
    next_byte <<= 8 - bits_encoded_so_far;
    output.push_back(next_byte);
  }

  return output;
}

absl::optional<VisionMatchResult> IsVisualMatch(
    const SkBitmap& image,
    const std::string& blurred_image_hash,
    const VisualFeatures::ColorHistogram& histogram,
    const VisualTarget& target) {
  TRACE_EVENT0("safe_browsing", "IsVisualMatch");

  TRACE_EVENT_BEGIN0("safe_browsing", "IsVisualMatch_ComputeHashDistance");
  size_t hash_distance;
  bool has_hash_distance =
      GetHashDistance(blurred_image_hash, target.hash(), &hash_distance);
  TRACE_EVENT_END0("safe_browsing", "IsVisualMatch_ComputeHashDistance");

  TRACE_EVENT_BEGIN0("safe_browsing", "IsVisualMatch_ComputeEMD");
  opencv::PointDistribution point_distribution =
      HistogramBinsToPointDistribution(histogram.bins());
  absl::optional<double> color_distance = opencv::EMD(
      point_distribution, HistogramBinsToPointDistribution(target.bins()));
  TRACE_EVENT_END0("safe_browsing", "IsVisualMatch_ComputeEMD");

  for (const MatchRule& match_rule : target.match_config().match_rule()) {
    bool is_match = true;
    if (match_rule.has_hash_distance()) {
      is_match &=
          (has_hash_distance && hash_distance <= match_rule.hash_distance());
    }

    if (match_rule.has_color_distance()) {
      is_match &= (color_distance.has_value() &&
                   color_distance.value() <= match_rule.color_distance());
    }

    for (const MatchRule::ColorRange& color_range : match_rule.color_range()) {
      is_match &= ImageHasColorInRange(image, color_range);
    }

    for (const MatchRule::FloatColorRange& color_range :
         match_rule.float_color_range()) {
      is_match &= ImageHasColorInRange(image, color_range);
    }

    if (is_match) {
      VisionMatchResult result;
      result.set_matched_target_digest(target.digest());
      if (has_hash_distance)
        result.set_vision_matched_phash_score(hash_distance);
      if (color_distance.has_value())
        result.set_vision_matched_emd_score(color_distance.value());
      return result;
    }
  }

  return absl::nullopt;
}

}  // namespace visual_utils
}  // namespace safe_browsing
