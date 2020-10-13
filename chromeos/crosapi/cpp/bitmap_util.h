// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_BITMAP_UTIL_H_
#define CHROMEOS_CROSAPI_CPP_BITMAP_UTIL_H_

#include "base/component_export.h"

class SkBitmap;

namespace crosapi {

struct Bitmap;

// Converts an SkBitmap to a crosapi::Bitmap. Assumes that the bitmap is
// unpadded, and uses 4 bytes per pixel. CHECK fails if the bitmap has an
// invalid size (e.g. its size is not equal to width * height * 4).
COMPONENT_EXPORT(CROSAPI) Bitmap BitmapFromSkBitmap(const SkBitmap& sk_bitmap);

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_BITMAP_UTIL_H_
