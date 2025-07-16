// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/omnibox/composebox/composebox_image_helper.h"

#include <numbers>
#include <optional>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/search/ntp_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

SkBitmap DownscaleImageIfNeeded(
    const SkBitmap& image,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  auto size = gfx::Size(image.width(), image.height());
  int max_image_size =
      ntp_features::kNtpSearchboxComposeEntrypointDownscaleMaxImageSizeParam
          .Get();
  int max_image_width =
      ntp_features::kNtpSearchboxComposeEntrypointDownscaleMaxImageWidthParam
          .Get();
  int max_image_height =
      ntp_features::kNtpSearchboxComposeEntrypointDownscaleMaxImageHeightParam
          .Get();
  if (lens::ShouldDownscaleSize(size, max_image_size, max_image_width,
                                max_image_height)) {
    return lens::DownscaleImage(image, max_image_width, max_image_height,
                                client_logs);
  }

  // No downscaling needed.
  return image;
}

}  // namespace

namespace composebox {

lens::ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  lens::ImageData image_data;
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();

  auto resized_bitmap = DownscaleImageIfNeeded(image, client_logs);
  if (lens::EncodeImageMaybeWithTransparency(
          resized_bitmap,
          ntp_features::
              kNtpSearchboxComposeEntrypointImageCompressionQualityParam.Get(),
          data, client_logs)) {
    image_data.mutable_image_metadata()->set_height(resized_bitmap.height());
    image_data.mutable_image_metadata()->set_width(resized_bitmap.width());

    image_data.mutable_payload()->mutable_image_bytes()->assign(data->begin(),
                                                                data->end());
  }
  return image_data;
}

}  // namespace composebox
