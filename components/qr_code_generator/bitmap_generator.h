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

// This gives the size of the required "quiet zone" that needs to be added to
// the image returned by `GenerateBitmap`.  The size is expressed in pixels of
// the returned `SkBitmap`.  A margin of light-colored pixels (that is this many
// pixels wide) needs to be added to the returned image on the left, right, top,
// and bottom.
//
// See also https://www.qrcode.com/en/howto/code.html which has diagrams and
// additional explanation about the "quiet zone".
extern const int kQuietZoneSizePixels;

// Generates an `SkBitmap` with a QR code that encodes the `data`.
//
// WARNING: The caller is responsible for adding light-colored margins of
// `kQuietZoneSize` pixels around the returned image.
// TODO(lukasza): Move "quiet zone" painting/resizing into `GenerateBitmap`
// (this may require testing that callers that didn't add the quiet zone are
// okay with differently-sized images and margins). See also
// `QRCodeGeneratorBubble::AddQRCodeQuietZone`.
base::expected<SkBitmap, Error> GenerateBitmap(base::span<const uint8_t> data,
                                               ModuleStyle module_style,
                                               LocatorStyle locator_style,
                                               CenterImage center_image);

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_
