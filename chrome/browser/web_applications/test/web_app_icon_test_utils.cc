// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

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

void AddEmptyIconToIconsMap(const GURL& icon_url, IconsMap* icons_map) {
  std::vector<SkBitmap> bitmaps;
  bitmaps.emplace_back();

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

base::FilePath GetAppIconsAnyDir(Profile* profile,
                                 const webapps::AppId& app_id) {
  base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  base::FilePath icons_dir = app_dir.AppendASCII("Icons");
  return icons_dir;
}

base::FilePath GetAppIconsMaskableDir(Profile* profile,
                                      const webapps::AppId& app_id) {
  base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  base::FilePath icons_dir = app_dir.AppendASCII("Icons Maskable");
  return icons_dir;
}

base::FilePath GetOtherIconsDir(Profile* profile,
                                const webapps::AppId& app_id) {
  base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  base::FilePath icons_dir = app_dir.AppendASCII("Image Cache");
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
  return base::span<const int>(kIconSizes, std::size(kIconSizes));
}

bool ContainsOneIconOfEachSize(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  for (int size_px : kIconSizes) {
    int num_icons_for_size = base::ranges::count(
        icon_bitmaps, size_px, &std::pair<const SquareSizePx, SkBitmap>::first);
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

  EXPECT_EQ(
      color_utils::SkColorToRgbaString(color),
      color_utils::SkColorToRgbaString(
          image_skia.GetRepresentation(scale).GetBitmap().getColor(0, 0)));
}

blink::Manifest::ImageResource CreateSquareImageResource(
    const GURL& src,
    int size_px,
    const std::vector<IconPurpose>& purposes) {
  blink::Manifest::ImageResource r;
  r.src = src;
  r.type = u"image/png";
  r.sizes = {gfx::Size{size_px, size_px}};
  r.purpose = purposes;
  return r;
}

std::map<SquareSizePx, SkBitmap> ReadPngsFromDirectory(
    FileUtilsWrapper* file_utils,
    const base::FilePath& icons_dir) {
  std::map<SquareSizePx, SkBitmap> pngs;

  base::FileEnumerator enumerator(icons_dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

    SkBitmap bitmap;
    EXPECT_TRUE(ReadBitmap(file_utils, path, &bitmap));

    EXPECT_EQ(bitmap.width(), bitmap.height());

    const int size_px = bitmap.width();
    EXPECT_FALSE(base::Contains(pngs, size_px));

    base::FilePath size_file_name;
    size_file_name =
        size_file_name.AppendASCII(base::StringPrintf("%i.png", size_px));
    EXPECT_EQ(size_file_name, path.BaseName());

    pngs[size_px] = bitmap;
  }

  return pngs;
}

GeneratedIconsInfo::GeneratedIconsInfo() = default;

GeneratedIconsInfo::GeneratedIconsInfo(const GeneratedIconsInfo&) = default;

GeneratedIconsInfo::GeneratedIconsInfo(IconPurpose purpose,
                                       std::vector<SquareSizePx> sizes_px,
                                       std::vector<SkColor> colors)
    : purpose(purpose),
      sizes_px(std::move(sizes_px)),
      colors(std::move(colors)) {}

GeneratedIconsInfo::~GeneratedIconsInfo() = default;

apps::IconInfo CreateIconInfo(const GURL& icon_base_url,
                              IconPurpose purpose,
                              SquareSizePx size_px) {
  apps::IconInfo apps_icon_info;

  apps_icon_info.url =
      icon_base_url.Resolve("icon-" + base::NumberToString(size_px) + ".png");
  apps_icon_info.square_size_px = size_px;
  apps_icon_info.purpose = ManifestPurposeToIconInfoPurpose(purpose);

  return apps_icon_info;
}

void AddIconsToWebAppInstallInfo(
    WebAppInstallInfo* install_info,
    const GURL& icons_base_url,
    const std::vector<GeneratedIconsInfo>& icons_info) {
  for (const GeneratedIconsInfo& info : icons_info) {
    DCHECK_EQ(info.sizes_px.size(), info.colors.size());

    std::map<SquareSizePx, SkBitmap> generated_bitmaps;

    for (size_t i = 0; i < info.sizes_px.size(); ++i) {
      apps::IconInfo apps_icon_info =
          CreateIconInfo(icons_base_url, info.purpose, info.sizes_px[i]);
      install_info->manifest_icons.push_back(std::move(apps_icon_info));

      AddGeneratedIcon(&generated_bitmaps, info.sizes_px[i], info.colors[i]);
    }

    install_info->icon_bitmaps.SetBitmapsForPurpose(
        info.purpose, std::move(generated_bitmaps));
  }
}

void IconManagerWriteGeneratedIcons(
    WebAppIconManager& icon_manager,
    const webapps::AppId& app_id,
    const std::vector<GeneratedIconsInfo>& icons_info) {
  IconBitmaps icon_bitmaps;

  for (const GeneratedIconsInfo& info : icons_info) {
    DCHECK_EQ(info.sizes_px.size(), info.colors.size());

    std::map<SquareSizePx, SkBitmap> generated_bitmaps;

    for (size_t i = 0; i < info.sizes_px.size(); ++i)
      AddGeneratedIcon(&generated_bitmaps, info.sizes_px[i], info.colors[i]);

    icon_bitmaps.SetBitmapsForPurpose(info.purpose,
                                      std::move(generated_bitmaps));
  }

  base::RunLoop run_loop;
  icon_manager.WriteData(app_id, std::move(icon_bitmaps), {}, {},
                         base::BindLambdaForTesting([&](bool success) {
                           DCHECK(success);
                           run_loop.Quit();
                         }));
  run_loop.Run();
}

SkColor IconManagerReadAppIconPixel(WebAppIconManager& icon_manager,
                                    const webapps::AppId& app_id,
                                    SquareSizePx size_px,
                                    int x,
                                    int y) {
  SkColor result = SK_ColorTRANSPARENT;
  base::RunLoop run_loop;
  icon_manager.ReadIcons(
      app_id, IconPurpose::ANY, {size_px},
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            DCHECK(base::Contains(icon_bitmaps, size_px));
            result = icon_bitmaps.at(size_px).getColor(x, y);
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

}  // namespace web_app
