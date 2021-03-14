// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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

using SizeToBitmap = std::map<SquareSizePx, SkBitmap>;

// Returns icon sizes to be generated from downloaded icons.
std::set<SquareSizePx> SizesToGenerate();

// This finds the closest not-smaller bitmap in |bitmaps| for each size in
// |sizes| and resizes it to that size. This returns a map of sizes to bitmaps
// which contains only bitmaps of a size in |sizes| and at most one bitmap of
// each size.
SizeToBitmap ConstrainBitmapsToSizes(const std::vector<SkBitmap>& bitmaps,
                                     const std::set<SquareSizePx>& sizes);

// Generates a square container icon of |output_size| by drawing the given
// |icon_letter| into a rounded background of |color|.
SkBitmap GenerateBitmap(SquareSizePx output_size,
                        SkColor color,
                        char16_t icon_letter);

// Returns the first letter from |app_url| that will be painted on the generated
// icon.
char16_t GenerateIconLetterFromUrl(const GURL& app_url);

// Returns the first letter from |app_name| that will be painted on the
// generated icon.
char16_t GenerateIconLetterFromAppName(const std::u16string& app_name);

// Resize icons to the accepted sizes, and generate any that are missing.
// Note that |icon_letter| is the first letter of app name if available
// otherwise the first letter of app url.
// Output: |generated_icon_color| is the color to use if an icon needs to be
// generated for the web app. |is_generated_icon| represents whether the icons
// were generated.
SizeToBitmap ResizeIconsAndGenerateMissing(
    const std::vector<SkBitmap>& icons,
    const std::set<SquareSizePx>& sizes_to_generate,
    char16_t icon_letter,
    SkColor* generated_icon_color,
    bool* is_generated_icon);

// Generate icons for default sizes, using the first letter of the application
// name and some background color. |app_name| is encoded as UTF8.
SizeToBitmap GenerateIcons(const std::string& app_name,
                           SkColor background_icon_color);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_
