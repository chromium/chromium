// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_util.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

namespace favicon_base {
namespace {

// Creates image reps of DIP size |favicon_size| for the subset of
// |favicon_scales| for which the image reps can be created without resizing
// or decoding the bitmap data.
std::vector<gfx::ImagePNGRep> SelectFaviconFramesFromPNGsWithoutResizing(
    const std::vector<favicon_base::FaviconRawBitmapResult>& png_data,
    const std::vector<float>& favicon_scales,
    int favicon_size) {
  TRACE_EVENT0("browser",
               "FaviconUtil::SelectFaviconFramesFromPNGsWithoutResizing");
  std::vector<gfx::ImagePNGRep> png_reps;
  if (png_data.empty())
    return png_reps;

  // A |favicon_size| of 0 indicates that the largest frame is desired.
  if (favicon_size == 0) {
    int maximum_area = 0;
    scoped_refptr<base::RefCountedMemory> best_candidate;
    for (size_t i = 0; i < png_data.size(); ++i) {
      int area = png_data[i].pixel_size.GetArea();
      if (area > maximum_area) {
        maximum_area = area;
        best_candidate = png_data[i].bitmap_data;
      }
    }
    png_reps.push_back(gfx::ImagePNGRep(best_candidate, 1.0f));
    return png_reps;
  }

  // Build a map which will be used to determine the scale used to
  // create a bitmap with given pixel size.
  std::map<int, float> desired_pixel_sizes;
  for (size_t i = 0; i < favicon_scales.size(); ++i) {
    int pixel_size =
        static_cast<int>(std::ceil(favicon_size * favicon_scales[i]));
    desired_pixel_sizes[pixel_size] = favicon_scales[i];
  }

  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      continue;

    const gfx::Size& pixel_size = png_data[i].pixel_size;
    if (pixel_size.width() != pixel_size.height())
      continue;

    auto it = desired_pixel_sizes.find(pixel_size.width());
    if (it == desired_pixel_sizes.end())
      continue;

    png_reps.push_back(gfx::ImagePNGRep(png_data[i].bitmap_data, it->second));
  }

  return png_reps;
}

// Returns a resampled bitmap of |desired_size| x |desired_size| by resampling
// the best bitmap out of |input_bitmaps|.
// ResizeBitmapByDownsamplingIfPossible() is similar to SelectFaviconFrames()
// but it operates on bitmaps which have already been resampled via
// SelectFaviconFrames().
SkBitmap ResizeBitmapByDownsamplingIfPossible(
    const std::vector<SkBitmap>& input_bitmaps,
    int desired_size) {
  DCHECK(!input_bitmaps.empty());
  DCHECK_NE(0, desired_size);

  SkBitmap best_bitmap;
  for (size_t i = 0; i < input_bitmaps.size(); ++i) {
    const SkBitmap& input_bitmap = input_bitmaps[i];
    if (input_bitmap.width() == desired_size &&
        input_bitmap.height() == desired_size) {
      return input_bitmap;
    } else if (best_bitmap.isNull()) {
      best_bitmap = input_bitmap;
    } else if (input_bitmap.width() >= best_bitmap.width() &&
               input_bitmap.height() >= best_bitmap.height()) {
      if (best_bitmap.width() < desired_size ||
          best_bitmap.height() < desired_size) {
        best_bitmap = input_bitmap;
      }
    } else {
      if (input_bitmap.width() >= desired_size &&
          input_bitmap.height() >= desired_size) {
        best_bitmap = input_bitmap;
      }
    }
  }

  if (desired_size % best_bitmap.width() == 0 &&
      desired_size % best_bitmap.height() == 0) {
    // Use nearest neighbour resampling if upsampling by an integer. This
    // makes the result look similar to the result of SelectFaviconFrames().
    SkBitmap bitmap;
    bitmap.allocN32Pixels(desired_size, desired_size);
    if (!best_bitmap.isOpaque())
      bitmap.eraseARGB(0, 0, 0, 0);

    SkCanvas canvas(bitmap);
    canvas.drawBitmapRect(best_bitmap,
                          SkRect::MakeIWH(desired_size, desired_size), nullptr);
    return bitmap;
  }
  return skia::ImageOperations::Resize(best_bitmap,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       desired_size,
                                       desired_size);
}

}  // namespace

std::vector<float> GetFaviconScales() {
  const float kScale1x = 1.0f;
  std::vector<ui::ScaleFactor> resource_scale_factors =
      ui::GetSupportedScaleFactors();

  // TODO(ios): 1.0f should not be necessary on iOS retina devices. However
  // the sync service only supports syncing 100p favicons. Until sync supports
  // other scales 100p is needed in the list of scales to retrieve and
  // store the favicons in both 100p for sync and 200p for display. cr/160503.
  std::vector<float> favicon_scales(1, kScale1x);
  for (size_t i = 0; i < resource_scale_factors.size(); ++i) {
    if (resource_scale_factors[i] != ui::SCALE_FACTOR_100P)
      favicon_scales.push_back(
          ui::GetScaleForScaleFactor(resource_scale_factors[i]));
  }
  return favicon_scales;
}

void SetFaviconColorSpace(gfx::Image* image) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  image->SetSourceColorSpace(base::mac::GetSystemColorSpace());
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

gfx::Image SelectFaviconFramesFromPNGs(
    const std::vector<favicon_base::FaviconRawBitmapResult>& png_data,
    const std::vector<float>& favicon_scales,
    int favicon_size) {
  TRACE_EVENT0("browser", "FaviconUtil::SelectFaviconFramesFromPNGs");

  // Create image reps for as many scales as possible without resizing
  // the bitmap data or decoding it. FaviconHandler stores already resized
  // favicons into history so no additional resizing should be needed in the
  // common case.
  // Creating the gfx::Image from |png_data| without resizing or decoding if
  // possible is important because:
  // - Sync does a byte-to-byte comparison of gfx::Image::As1xPNGBytes() to
  //   the data it put into the database in order to determine whether any
  //   updates should be pushed to sync.
  // - The decoding occurs on the UI thread and the decoding can be a
  //   significant performance hit if a user has many bookmarks.
  // TODO(pkotwicz): Move the decoding off the UI thread.
  std::vector<gfx::ImagePNGRep> png_reps =
      SelectFaviconFramesFromPNGsWithoutResizing(
          png_data, favicon_scales, favicon_size);

  // SelectFaviconFramesFromPNGsWithoutResizing() should have selected the
  // largest favicon if |favicon_size| == 0.
  if (favicon_size == 0)
    return gfx::Image(png_reps);

  std::vector<float> favicon_scales_to_generate = favicon_scales;
  for (size_t i = 0; i < png_reps.size(); ++i) {
    auto iter = std::find(favicon_scales_to_generate.begin(),
                          favicon_scales_to_generate.end(), png_reps[i].scale);
    if (iter != favicon_scales_to_generate.end())
      favicon_scales_to_generate.erase(iter);
  }

  if (favicon_scales_to_generate.empty())
    return gfx::Image(png_reps);

  std::vector<SkBitmap> bitmaps;
  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      continue;

    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(png_data[i].bitmap_data->front(),
                              png_data[i].bitmap_data->size(),
                              &bitmap)) {
      bitmaps.push_back(bitmap);
    }
  }

  if (bitmaps.empty())
    return gfx::Image();

  gfx::ImageSkia resized_image_skia;
  for (size_t i = 0; i < favicon_scales_to_generate.size(); ++i) {
    float scale = favicon_scales_to_generate[i];
    int desired_size_in_pixel =
        static_cast<int>(std::ceil(favicon_size * scale));
    SkBitmap bitmap =
        ResizeBitmapByDownsamplingIfPossible(bitmaps, desired_size_in_pixel);
    resized_image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  if (png_reps.empty())
    return gfx::Image(resized_image_skia);

  std::vector<gfx::ImageSkiaRep> resized_image_skia_reps =
      resized_image_skia.image_reps();
  for (size_t i = 0; i < resized_image_skia_reps.size(); ++i) {
    scoped_refptr<base::RefCountedBytes> png_bytes(new base::RefCountedBytes());
    if (gfx::PNGCodec::EncodeBGRASkBitmap(
            resized_image_skia_reps[i].GetBitmap(), false,
            &png_bytes->data())) {
      png_reps.push_back(
          gfx::ImagePNGRep(png_bytes, resized_image_skia_reps[i].scale()));
    }
  }

  return gfx::Image(png_reps);
}

favicon_base::FaviconRawBitmapResult ResizeFaviconBitmapResult(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results,
    int desired_size_in_pixel) {
  TRACE_EVENT0("browser", "FaviconUtil::ResizeFaviconBitmapResult");

  if (favicon_bitmap_results.empty() || !favicon_bitmap_results[0].is_valid())
    return favicon_base::FaviconRawBitmapResult();

  favicon_base::FaviconRawBitmapResult bitmap_result =
      favicon_bitmap_results[0];

  // If the desired size is 0, SelectFaviconFrames() will return the largest
  // bitmap without doing any resizing. As |favicon_bitmap_results| has bitmap
  // data for a single bitmap, return it and avoid an unnecessary decode.
  if (desired_size_in_pixel == 0)
    return bitmap_result;

  // If history bitmap is already desired pixel size, return early.
  if (bitmap_result.pixel_size.width() == desired_size_in_pixel &&
      bitmap_result.pixel_size.height() == desired_size_in_pixel)
    return bitmap_result;

  // Convert raw bytes to SkBitmap, resize via SelectFaviconFrames(), then
  // convert back.
  std::vector<float> desired_favicon_scales;
  desired_favicon_scales.push_back(1.0f);

  gfx::Image resized_image = favicon_base::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results, desired_favicon_scales, desired_size_in_pixel);

  std::vector<unsigned char> resized_bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(resized_image.AsBitmap(), false,
                                         &resized_bitmap_data)) {
    return favicon_base::FaviconRawBitmapResult();
  }

  bitmap_result.bitmap_data = base::RefCountedBytes::TakeVector(
      &resized_bitmap_data);
  bitmap_result.pixel_size =
      gfx::Size(desired_size_in_pixel, desired_size_in_pixel);

  return bitmap_result;
}

}  // namespace favicon_base
