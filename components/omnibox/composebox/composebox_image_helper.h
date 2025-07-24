// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMOSEBOX_IMAGE_HELPER_H_
#define COMPONENTS_OMNIBOX_COMOSEBOX_IMAGE_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace composebox {

// Downscales and encodes the provided bitmap and then stores it in a
// lens::ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions exceed configured flag
// values.
lens::ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    const ImageEncodingOptions& image_options);

}  // namespace composebox

#endif  // COMPONENTS_OMNIBOX_COMOSEBOX_IMAGE_HELPER_H_
