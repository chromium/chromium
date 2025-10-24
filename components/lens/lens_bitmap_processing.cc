// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_bitmap_processing.h"

#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace lens {

bool ShouldDownscaleSize(const gfx::Size& size,
                         int max_area,
                         int max_width,
                         int max_height) {
  // This returns true if the area is larger than the max area AND one of the
  // width OR height exceeds the configured max values.
  return size.GetArea() > max_area &&
         (size.width() > max_width || size.height() > max_height);
}

double GetPreferredScale(const gfx::Size& original_size,
                         int target_width,
                         int target_height) {
  return std::min(
      base::ClampDiv(static_cast<double>(target_width), original_size.width()),
      base::ClampDiv(static_cast<double>(target_height),
                     original_size.height()));
}

gfx::Size GetPreferredSize(const gfx::Size& original_size,
                           int target_width,
                           int target_height) {
  double scale = GetPreferredScale(original_size, target_width, target_height);
  int width = std::clamp<int>(scale * original_size.width(), 1, target_width);
  int height =
      std::clamp<int>(scale * original_size.height(), 1, target_height);
  return gfx::Size(width, height);
}

void AddClientLogsForEncode(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    scoped_refptr<base::RefCountedBytes> output_bytes) {
  auto* encode_phase = client_logs->client_logs()
                           .mutable_phase_latencies_metadata()
                           ->add_phase();
  encode_phase->mutable_image_encode_data()->set_encoded_image_size_bytes(
      output_bytes->as_vector().size());
}

void AddClientLogsForDownscale(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const gfx::Size& original_pixel_size,
    const gfx::Size& downscaled_pixel_size) {
  auto* downscale_phase = client_logs->client_logs()
                              .mutable_phase_latencies_metadata()
                              ->add_phase();
  downscale_phase->mutable_image_downscale_data()->set_original_image_size(
      original_pixel_size.GetArea());
  downscale_phase->mutable_image_downscale_data()->set_downscaled_image_size(
      downscaled_pixel_size.GetArea());
}

#if !BUILDFLAG(IS_IOS)

void AddClientLogsForDownscale(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const SkBitmap& original_image,
    const SkBitmap& downscaled_image) {
  auto* downscale_phase = client_logs->client_logs()
                              .mutable_phase_latencies_metadata()
                              ->add_phase();
  downscale_phase->mutable_image_downscale_data()->set_original_image_size(
      original_image.width() * original_image.height());
  downscale_phase->mutable_image_downscale_data()->set_downscaled_image_size(
      downscaled_image.width() * downscaled_image.height());
}

SkBitmap DownscaleImage(
    const SkBitmap& image,
    int target_width,
    int target_height,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs) {
  auto size = gfx::Size(image.width(), image.height());
  auto preferred_size = GetPreferredSize(size, target_width, target_height);
  SkBitmap downscaled_image = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, preferred_size.width(),
      preferred_size.height());
  AddClientLogsForDownscale(client_logs, image, downscaled_image);
  return downscaled_image;
}

bool EncodeImage(const SkBitmap& image,
                 int compression_quality,
                 scoped_refptr<base::RefCountedBytes> output,
                 scoped_refptr<RefCountedLensOverlayClientLogs> client_logs) {
  std::optional<std::vector<uint8_t>> encoded_image =
      gfx::JPEGCodec::Encode(image, compression_quality);
  if (encoded_image) {
    output->as_vector() = std::move(encoded_image.value());
    AddClientLogsForEncode(client_logs, output);
    return true;
  }
  return false;
}

bool EncodeImageMaybeWithTransparency(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs) {
  if (image.isOpaque()) {
    return EncodeImage(image, compression_quality, output, client_logs);
  }
  std::optional<std::vector<uint8_t>> encoded_image =
      gfx::WebpCodec::Encode(image, compression_quality);
  if (encoded_image) {
    output->as_vector() = std::move(encoded_image.value());
    AddClientLogsForEncode(client_logs, output);
    return true;
  }
  return false;
}

SkBitmap DownscaleBitmap(
    const SkBitmap& image,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const ImageEncodingOptions& image_options) {
  auto size = gfx::Size(image.width(), image.height());
  if (ShouldDownscaleSize(size, image_options.max_size, image_options.max_width,
                          image_options.max_height)) {
    return DownscaleImage(image, image_options.max_width,
                          image_options.max_height, client_logs);
  }

  // No downscaling needed.
  return image;
}

ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const ImageEncodingOptions& image_options) {
  ImageData image_data;
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();

  auto resized_bitmap = DownscaleBitmap(image, client_logs, image_options);
  if (image_options.enable_webp_encoding
          ? EncodeImageMaybeWithTransparency(resized_bitmap,
                                             image_options.compression_quality,
                                             data, client_logs)
          : EncodeImage(resized_bitmap, image_options.compression_quality, data,
                        client_logs)) {
    image_data.mutable_image_metadata()->set_height(resized_bitmap.height());
    image_data.mutable_image_metadata()->set_width(resized_bitmap.width());

    image_data.mutable_payload()->mutable_image_bytes()->assign(data->begin(),
                                                                data->end());
  }
  return image_data;
}

#endif  // !BUILDFLAG(IS_IOS)

}  // namespace lens
