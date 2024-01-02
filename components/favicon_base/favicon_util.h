// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_UTIL_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_UTIL_H_

#include <vector>

#include "components/favicon_base/favicon_types.h"

namespace gfx {
class Image;
}

namespace favicon_base {

// Returns the scales at which favicons should be fetched. This is
// different from ui::GetSupportedResourceScaleFactors() because clients which
// do not support 1x should still fetch a favicon for 1x to push to sync. This
// guarantees that the clients receiving sync updates pushed by this client
// receive a favicon (potentially of the wrong scale factor) and do not show
// the default favicon.
std::vector<float> GetFaviconScales();

// Takes a vector of PNG-encoded frames, and converts it to a gfx::Image of
// size |favicon_size| in DIPS. The result gfx::Image has a gfx::ImageSkia with
// gfx::ImageSkiaReps for each |favicon_scales|.
gfx::Image SelectFaviconFramesFromPNGs(
    const std::vector<favicon_base::FaviconRawBitmapResult>& png_data,
    const std::vector<float>& favicon_scales,
    int favicon_size);

// Generates a favicon_bitmap_result sized exactly to [desired_size,
// desired_size] from the provided result set.  If the exact size is found in
// the set, it just returns that; otherwise, it will decode the PNG, scale,
// and encode a new PNG.
favicon_base::FaviconRawBitmapResult ResizeFaviconBitmapResult(
    int desired_size_in_pixel,
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results);

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_UTIL_H_
