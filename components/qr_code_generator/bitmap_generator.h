// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_
#define COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/qr_code_generator/error.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

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
#if !BUILDFLAG(IS_IOS)
  kPasskey,
  kProductLogo,
#endif
};

// Whether `GenerateBitmap` will include the necessary quiet zone around the
// generated QR code.
enum class QuietZone {
  kIncluded,

  // WARNING: This option means that the caller of `GenerateBitmap` is
  // responsible for adding light-colored margins of `kQuietZoneSize` pixels
  // around the returned image.  See also `kQuietZoneSizePixels` below.
  //
  // TODO(https://crbug.com/325664342): Audit callers of `GenerateBitmap` and
  // see if they can/should use `kIncluded` instead (testing if the callers are
  // okay with differently-sized images and margins).  Note that the quiet zone
  // may help to detect the QR codes even for small codes.   Once all the users
  // of `kWillBeAddedByClient` are removed, the `QuietZone` enum can be removed
  // altogether.
  kWillBeAddedByClient,
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

// Generates a gfx::ImageSkia with a QR code that encodes the `data`.
base::expected<gfx::ImageSkia, Error> GenerateImage(
    base::span<const uint8_t> data,
    ModuleStyle module_style,
    LocatorStyle locator_style,
    CenterImage center_image,
    QuietZone quiet_zone);

// Generates an `SkBitmap` with a QR code that encodes the `data`.
//
// Use GenerateImage() if the QR code might be displayed on a high density
// (retina) display.
// TODO(lukasza, petewil): Consider letting the caller specify the exact light
// and dark colors.  (Currently white and black are always used.)
base::expected<SkBitmap, Error> GenerateBitmap(base::span<const uint8_t> data,
                                               ModuleStyle module_style,
                                               LocatorStyle locator_style,
                                               CenterImage center_image,
                                               QuietZone quiet_zone);

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_BITMAP_GENERATOR_H_
