// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_GENERATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_GENERATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {

namespace icon_size {

// An iteration of valid icon sizes in pixels. Allows client code to declare a
// subset of supported sizes that are guaranteed to be valid.
enum {
  k512 = 512,
  k256 = 256,
  k128 = 128,
  k96 = 96,
  k64 = 64,
  k48 = 48,
  k32 = 32,
  k24 = 24,
  k16 = 16,
  kInvalid = 0,
};

}  // namespace icon_size

#if BUILDFLAG(IS_MAC)
constexpr int kInstallIconSize = icon_size::k96;
constexpr int kLauncherIconSize = icon_size::k256;
#elif BUILDFLAG(IS_CHROMEOS)
constexpr int kInstallIconSize = icon_size::k96;
constexpr int kLauncherIconSize = icon_size::k128;
#else
constexpr int kInstallIconSize = icon_size::k48;
constexpr int kLauncherIconSize = icon_size::k128;
#endif

using SizeToBitmap = std::map<SquareSizePx, SkBitmap>;

// Returns icon sizes to be generated from downloaded icons.
std::set<SquareSizePx> SizesToGenerate();

// This finds the closest not-smaller bitmap in |bitmaps| for each size in
// |sizes| and resizes it to that size. This returns a map of sizes to bitmaps
// which contains only bitmaps of a size in |sizes| and at most one bitmap of
// each size.
SizeToBitmap ConstrainBitmapsToSizes(const std::vector<SkBitmap>& bitmaps,
                                     const std::set<SquareSizePx>& sizes);

// Resize icons to the accepted sizes, and generate any that are missing.
// Note that |icon_letter| is the first letter of app name if available
// otherwise the first letter of app url.
// Output: |is_generated_icon| represents whether the icons were generated.
SizeToBitmap ResizeIconsAndGenerateMissing(
    const std::vector<SkBitmap>& icons,
    const std::set<SquareSizePx>& sizes_to_generate,
    char32_t icon_letter,
    bool* is_generated_icon);

// Generate icons for default sizes, using the first letter of the application
// name. |app_name| is encoded as UTF8.
SizeToBitmap GenerateIcons(const std::string& app_name);

// Converts any image with arbitrary RGB channels to a monochrome image
// according to the spec.
// https://www.w3.org/TR/appmanifest/#monochrome-icons-and-solid-fills
gfx::ImageSkia ConvertImageToSolidFillMonochrome(SkColor solid_color,
                                                 const gfx::ImageSkia& image);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_GENERATOR_H_
