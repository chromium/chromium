// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_
#define COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/qr_code_generator/error.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace qr_code_generator {

// How to render QR code "pixels".
// This does not affect the main locators.
enum class ModuleStyle {
  kSquares,
  kCircles,
};

// Style for the corner locators.
enum class LocatorStyle {
  kSquare,
  kRounded,
};

// The center image to superimpose over the QR code.
enum class CenterImage {
  kNoCenterImage,
  kDino,
  kPasskey,
};

// Structure for returning QR Code image data.
struct QRImage {
  // Image data for generated QR code.
  SkBitmap bitmap;

  // Size of the generated QR code in elements. Note that `bitmap` will be
  // upscaled, so this does not represent the returned image size.
  //
  // TODO: This member wouldn't be needed if `Generate` took care of
  // generating a "quiet zone" of 4 or more modules (instead of putting that
  // responsibility on the caller).  See also
  // https://www.qrcode.com/en/howto/code.html.
  gfx::Size data_size;
};

base::expected<QRImage, Error> GenerateBitmap(base::span<const uint8_t> data,
                                              ModuleStyle module_style,
                                              LocatorStyle locator_style,
                                              CenterImage center_image);

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_
