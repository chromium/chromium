// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

struct WebApplicationIconInfo;

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

// Returns icon sizes to be generated from downloaded icons.
std::set<int> SizesToGenerate();

struct BitmapAndSource {
  BitmapAndSource();
  BitmapAndSource(const GURL& source_url_p, const SkBitmap& bitmap_p);
  ~BitmapAndSource();

  GURL source_url;
  SkBitmap bitmap;
};

// This finds the closest not-smaller bitmap in |bitmaps| for each size in
// |sizes| and resizes it to that size. This returns a map of sizes to bitmaps
// which contains only bitmaps of a size in |sizes| and at most one bitmap of
// each size.
std::map<int, BitmapAndSource> ConstrainBitmapsToSizes(
    const std::vector<BitmapAndSource>& bitmaps,
    const std::set<int>& sizes);

// Generates a square container icon of |output_size| by drawing the given
// |letter| into a rounded background of |color|.
SkBitmap GenerateBitmap(int output_size, SkColor color, base::char16 letter);

// Returns the letter that will be painted on the generated icon.
base::char16 GenerateIconLetterFromUrl(const GURL& app_url);

// Resize icons to the accepted sizes, and generate any that are missing.
// Note that |app_url| is the launch URL for the app.
// Output: |generated_icon_color| is the color to use if an icon needs to be
// generated for the web app.
std::map<int, BitmapAndSource> ResizeIconsAndGenerateMissing(
    const std::vector<BitmapAndSource>& icons,
    const std::set<int>& sizes_to_generate,
    const GURL& app_url,
    SkColor* generated_icon_color);

// Generate icons for default sizes, using the first letter of the application
// name and some background color. |app_name| is encoded as UTF8.
std::vector<WebApplicationIconInfo> GenerateIcons(
    const std::string& app_name,
    SkColor background_icon_color);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_GENERATOR_H_
