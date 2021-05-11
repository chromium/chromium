// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"

#include <utility>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace web_app {

namespace {

constexpr int kIconSizes[] = {
    icon_size::k32, icon_size::k64,  icon_size::k48,
    icon_size::k96, icon_size::k128, icon_size::k256,
};

}  // namespace

SkBitmap CreateSquareIcon(int size_px, SkColor solid_color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size_px, size_px);
  bitmap.eraseColor(solid_color);
  return bitmap;
}

void AddGeneratedIcon(std::map<SquareSizePx, SkBitmap>* icon_bitmaps,
                      int size_px,
                      SkColor solid_color) {
  (*icon_bitmaps)[size_px] = CreateSquareIcon(size_px, solid_color);
}

void AddIconToIconsMap(const GURL& icon_url,
                       int size_px,
                       SkColor solid_color,
                       IconsMap* icons_map) {
  SkBitmap bitmap = CreateSquareIcon(size_px, solid_color);

  std::vector<SkBitmap> bitmaps;
  bitmaps.push_back(std::move(bitmap));

  icons_map->emplace(icon_url, std::move(bitmaps));
}

bool AreColorsEqual(SkColor expected_color,
                    SkColor actual_color,
                    int threshold) {
  uint32_t expected_alpha = SkColorGetA(expected_color);
  int error_r = SkColorGetR(actual_color) - SkColorGetR(expected_color);
  int error_g = SkColorGetG(actual_color) - SkColorGetG(expected_color);
  int error_b = SkColorGetB(actual_color) - SkColorGetB(expected_color);
  int error_a = SkColorGetA(actual_color) - expected_alpha;
  int abs_error_r = std::abs(error_r);
  int abs_error_g = std::abs(error_g);
  int abs_error_b = std::abs(error_b);
  int abs_error_a = std::abs(error_a);

  // Colors are equal if error is below threshold.
  return abs_error_r <= threshold && abs_error_g <= threshold &&
         abs_error_b <= threshold && abs_error_a <= threshold;
}

base::FilePath GetAppIconsAnyDir(Profile* profile, const AppId& app_id) {
  base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  base::FilePath icons_dir = app_dir.AppendASCII("Icons");
  return icons_dir;
}

base::FilePath GetAppIconsMaskableDir(Profile* profile, const AppId& app_id) {
  base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  base::FilePath icons_dir = app_dir.AppendASCII("Icons Maskable");
  return icons_dir;
}

bool ReadBitmap(FileUtilsWrapper* utils,
                const base::FilePath& file_path,
                SkBitmap* bitmap) {
  std::string icon_data;
  if (!utils->ReadFileToString(file_path, &icon_data))
    return false;

  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(icon_data.c_str()),
      icon_data.size(), bitmap);
}

base::span<const int> GetIconSizes() {
  return base::span<const int>(kIconSizes, base::size(kIconSizes));
}

bool ContainsOneIconOfEachSize(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  for (int size_px : kIconSizes) {
    int num_icons_for_size = std::count_if(
        icon_bitmaps.begin(), icon_bitmaps.end(),
        [&size_px](const std::pair<SquareSizePx, SkBitmap>& icon) {
          return icon.first == size_px;
        });
    if (num_icons_for_size != 1)
      return false;
  }

  return true;
}

void ExpectImageSkiaRep(const gfx::ImageSkia& image_skia,
                        float scale,
                        SquareSizePx size_px,
                        SkColor color) {
  ASSERT_TRUE(image_skia.HasRepresentation(scale));

  EXPECT_EQ(size_px, image_skia.GetRepresentation(scale).GetBitmap().width());
  EXPECT_EQ(size_px, image_skia.GetRepresentation(scale).GetBitmap().height());

  EXPECT_EQ(color,
            image_skia.GetRepresentation(scale).GetBitmap().getColor(0, 0));
}

}  // namespace web_app
