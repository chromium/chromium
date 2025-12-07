// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_IMAGE_HELPER_IOS_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_IMAGE_HELPER_IOS_H_

#import <UIKit/UIKit.h>

#include "components/lens/lens_bitmap_processing.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"

namespace composebox {

// Downscales (if necessary) and encodes the given image according to the
// provided options. The target dimensions in `image_options` are treated as
// pixels.
lens::ImageData DownscaleAndEncodeImage(
    UIImage* image,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    const lens::ImageEncodingOptions& image_options);

}  // namespace composebox

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_IMAGE_HELPER_IOS_H_
