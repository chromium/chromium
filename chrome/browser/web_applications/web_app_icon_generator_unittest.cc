// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/web_applications/web_app_icon_generator.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace web_app {

namespace {

const int kIconSizeSmallBetweenMediumAndLarge = 63;
const int kIconSizeLargeBetweenMediumAndLarge = 96;

std::set<int> TestSizesToGenerate() {
  const int kIconSizesToGenerate[] = {
      icon_size::k32,
      icon_size::k48,
      icon_size::k128,
  };
  return std::set<int>(kIconSizesToGenerate,
                       kIconSizesToGenerate + std::size(kIconSizesToGenerate));
}

void ValidateAllIconsWithURLsArePresent(
    const std::vector<SkBitmap>& bitmaps_to_check,
    const SizeToBitmap& size_map) {
  EXPECT_EQ(bitmaps_to_check.size(), size_map.size());

  // Check that every icon has a mapped icon.
  for (const auto& bitmap : bitmaps_to_check) {
    bool found = false;
    int width = bitmap.width();
    if (base::Contains(size_map, width)) {
      const SkBitmap& mapped_icon = size_map.at(width);
      if (mapped_icon.width() == width)
        found = true;
    }
    EXPECT_TRUE(found);
  }
}

std::vector<SkBitmap>::const_iterator FindLargestSkBitmapVector(
    const std::vector<SkBitmap>& bitmap_vector) {
  auto result = bitmap_vector.end();
  int largest = -1;
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->width() > largest) {
      result = it;
    }
  }
  return result;
}

std::vector<SkBitmap>::const_iterator FindMatchingSkBitmapVector(
    const std::vector<SkBitmap>& bitmap_vector,
    int size) {
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->width() == size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

std::vector<SkBitmap>::const_iterator FindEqualOrLargerSkBitmapVector(
    const std::vector<SkBitmap>& bitmap_vector,
    int size) {
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->width() >= size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

void ValidateIconsGeneratedAndResizedCorrectly(
    const std::vector<SkBitmap>& downloaded,
    const SizeToBitmap& size_map,
    const std::set<int>& sizes_to_generate,
    int expected_generated,
    int expected_resized) {
  GURL empty_url("");
  int number_generated = 0;
  int number_resized = 0;

  auto icon_largest = FindLargestSkBitmapVector(downloaded);
  for (const auto& size : sizes_to_generate) {
    auto icon_downloaded = FindMatchingSkBitmapVector(downloaded, size);
    auto icon_larger = FindEqualOrLargerSkBitmapVector(downloaded, size);
    if (icon_downloaded == downloaded.end()) {
      auto icon_resized = size_map.find(size);
      if (icon_largest == downloaded.end()) {
        // There are no downloaded icons. Expect an icon to be generated.
        EXPECT_NE(size_map.end(), icon_resized);
        EXPECT_EQ(size, icon_resized->second.width());
        EXPECT_EQ(size, icon_resized->second.height());
        ++number_generated;
      } else {
        // If there is a larger downloaded icon, it should be resized. Otherwise
        // the largest downloaded icon should be resized.
        auto icon_to_resize = icon_largest;
        if (icon_larger != downloaded.end())
          icon_to_resize = icon_larger;
        EXPECT_NE(size_map.end(), icon_resized);
        EXPECT_EQ(size, icon_resized->second.width());
        EXPECT_EQ(size, icon_resized->second.height());
        ++number_resized;
      }
    } else {
      // There is an icon of exactly this size downloaded. Expect no icon to be
      // generated, and the existing downloaded icon to be used.
      auto icon_resized = size_map.find(size);
      EXPECT_NE(size_map.end(), icon_resized);
      EXPECT_EQ(size, icon_resized->second.width());
      EXPECT_EQ(size, icon_resized->second.height());
      EXPECT_EQ(size, icon_downloaded->width());
      EXPECT_EQ(size, icon_downloaded->height());
    }
  }
  EXPECT_EQ(expected_generated, number_generated);
  EXPECT_EQ(expected_resized, number_resized);
}

void ValidateBitmapSizeAndColor(const SkBitmap& bitmap,
                                int size,
                                SkColor color) {
  // Obtain pixel lock to access pixels.
  EXPECT_EQ(color, bitmap.getColor(0, 0));
  EXPECT_EQ(size, bitmap.width());
  EXPECT_EQ(size, bitmap.height());
}

void TestIconGeneration(int icon_size,
                        int expected_generated,
                        int expected_resized) {
  std::vector<SkBitmap> downloaded;

  // Add an icon. 'Download' it.
  downloaded.push_back(CreateSquareIcon(icon_size, SK_ColorRED));

  // Now run the resizing/generation and validation.
  bool is_generated_icon = true;
  auto size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), U'T', &is_generated_icon);
  EXPECT_FALSE(is_generated_icon);

  ValidateIconsGeneratedAndResizedCorrectly(
      downloaded, size_map, TestSizesToGenerate(), expected_generated,
      expected_resized);
}

}  // namespace

class WebAppIconGeneratorTest : public testing::Test {
 public:
  WebAppIconGeneratorTest() = default;
  WebAppIconGeneratorTest(const WebAppIconGeneratorTest&) = delete;
  WebAppIconGeneratorTest& operator=(const WebAppIconGeneratorTest&) = delete;

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(WebAppIconGeneratorTest, ConstrainBitmapsToSizes) {
  std::set<int> desired_sizes;
  desired_sizes.insert(16);
  desired_sizes.insert(32);
  desired_sizes.insert(48);
  desired_sizes.insert(96);
  desired_sizes.insert(128);
  desired_sizes.insert(256);

  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareIcon(16, SK_ColorRED));
    bitmaps.push_back(CreateSquareIcon(32, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareIcon(144, SK_ColorYELLOW));

    SizeToBitmap results = ConstrainBitmapsToSizes(bitmaps, desired_sizes);

    EXPECT_EQ(6u, results.size());
    ValidateBitmapSizeAndColor(results[16], 16, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[32], 32, SK_ColorGREEN);
    ValidateBitmapSizeAndColor(results[48], 48, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[96], 96, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[128], 128, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[256], 256, SK_ColorYELLOW);
  }
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareIcon(512, SK_ColorRED));
    bitmaps.push_back(CreateSquareIcon(18, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareIcon(33, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareIcon(17, SK_ColorYELLOW));

    SizeToBitmap results = ConstrainBitmapsToSizes(bitmaps, desired_sizes);

    EXPECT_EQ(6u, results.size());
    ValidateBitmapSizeAndColor(results[16], 16, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[32], 32, SK_ColorBLUE);
    ValidateBitmapSizeAndColor(results[48], 48, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[96], 96, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[128], 128, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[256], 256, SK_ColorRED);
  }
}

TEST_F(WebAppIconGeneratorTest, LinkedAppIconsAreNotChanged) {
  std::vector<SkBitmap> icons;

  const SkColor color = SK_ColorBLACK;

  icons.push_back(CreateSquareIcon(icon_size::k48, color));
  icons.push_back(CreateSquareIcon(icon_size::k32, color));
  icons.push_back(CreateSquareIcon(icon_size::k128, color));

  // 'Download' one of the icons without a size or bitmap.
  std::vector<SkBitmap> downloaded;
  downloaded.push_back(CreateSquareIcon(icon_size::k128, color));

  const auto& sizes = TestSizesToGenerate();

  // Now run the resizing and generation into a new web icons info.
  bool is_generated_icon = true;
  SizeToBitmap size_map = ResizeIconsAndGenerateMissing(downloaded, sizes, U'T',
                                                        &is_generated_icon);
  EXPECT_EQ(sizes.size(), size_map.size());
  EXPECT_FALSE(is_generated_icon);

  // Now check that the linked app icons are matching.
  ValidateAllIconsWithURLsArePresent(icons, size_map);
}

TEST_F(WebAppIconGeneratorTest, IconsResizedFromOddSizes) {
  std::vector<SkBitmap> downloaded;

  const SkColor color = SK_ColorRED;

  // Add three icons. 'Download' each of them.
  downloaded.push_back(CreateSquareIcon(icon_size::k32, color));
  downloaded.push_back(
      CreateSquareIcon(kIconSizeSmallBetweenMediumAndLarge, color));
  downloaded.push_back(
      CreateSquareIcon(kIconSizeLargeBetweenMediumAndLarge, color));

  // Now run the resizing and generation.
  bool is_generated_icon = true;
  SizeToBitmap size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), U'T', &is_generated_icon);
  EXPECT_FALSE(is_generated_icon);

  // No icons should be generated. The LARGE and MEDIUM sizes should be resized.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);
}

TEST_F(WebAppIconGeneratorTest, IconsResizedFromLarger) {
  std::vector<SkBitmap> downloaded;

  // Add three icons. 'Download' two of them and pretend
  // the third failed to download.
  downloaded.push_back(CreateSquareIcon(icon_size::k32, SK_ColorRED));
  downloaded.push_back(CreateSquareIcon(icon_size::k512, SK_ColorBLACK));

  // Now run the resizing and generation.
  bool is_generated_icon = true;
  SizeToBitmap size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), U'T', &is_generated_icon);
  EXPECT_FALSE(is_generated_icon);

  // Expect icon for MEDIUM and LARGE to be resized from the gigantor icon
  // as it was not downloaded.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);
}

TEST_F(WebAppIconGeneratorTest, AllIconsGeneratedWhenNotDownloaded) {
  // Add three icons. 'Download' none of them.
  std::vector<SkBitmap> downloaded;

  // Now run the resizing and generation.
  bool is_generated_icon = false;
  SizeToBitmap size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), U'T', &is_generated_icon);
  EXPECT_TRUE(is_generated_icon);

  // Expect all icons to be generated.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 3, 0);
}

TEST_F(WebAppIconGeneratorTest, IconResizedFromLargerAndSmaller) {
  std::vector<SkBitmap> downloaded;

  // Pretend the huge icon wasn't downloaded but two smaller ones were.
  downloaded.push_back(CreateSquareIcon(icon_size::k16, SK_ColorRED));
  downloaded.push_back(CreateSquareIcon(icon_size::k48, SK_ColorBLUE));

  // Now run the resizing and generation.
  bool is_generated_icon = true;
  SizeToBitmap size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), U'T', &is_generated_icon);
  EXPECT_FALSE(is_generated_icon);

  // Expect no icons to be generated, but the LARGE and SMALL icons to be
  // resized from the MEDIUM icon.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);

  // Verify specifically that the LARGE icons was resized from the medium icon.
  const auto it = size_map.find(icon_size::k128);
  EXPECT_NE(size_map.end(), it);
}

TEST_F(WebAppIconGeneratorTest, IconsResizedWhenOnlyATinyOneIsProvided) {
  // When only a tiny icon is downloaded (smaller than the three desired
  // sizes), 3 icons should be resized.
  TestIconGeneration(icon_size::k16, 0, 3);
}

TEST_F(WebAppIconGeneratorTest, IconsResizedWhenOnlyAGigantorOneIsProvided) {
  // When an enormous icon is provided, each desired icon size should be resized
  // from it, and no icons should be generated.
  TestIconGeneration(icon_size::k512, 0, 3);
}

TEST_F(WebAppIconGeneratorTest, GenerateIcons) {
  std::set<int> sizes = SizesToGenerate();
  constexpr SkColor bg_color = SK_ColorDKGRAY;

  // The |+| character guarantees that there is some letter_color area at the
  // center of the generated icon.
  const std::map<SquareSizePx, SkBitmap> icon_bitmaps = GenerateIcons("+");
  EXPECT_EQ(sizes.size(), icon_bitmaps.size());

  for (const std::pair<const SquareSizePx, SkBitmap>& icon : icon_bitmaps) {
    SquareSizePx size = icon.first;
    const SkBitmap& bitmap = icon.second;
    EXPECT_EQ(size, bitmap.width());
    EXPECT_EQ(size, bitmap.height());

    const int border_radius = size / 16;
    const int center_x = size / 2;
    const int center_y = size / 2;

    // We don't check corner colors here: the icon is rounded by border_radius.
    EXPECT_EQ(bg_color, bitmap.getColor(border_radius * 2, center_y));
    EXPECT_EQ(bg_color, bitmap.getColor(center_x, border_radius * 2));

    // Only for large icons with a sharp letter: Peek a pixel at the center of
    // icon. This is tested on Linux and ChromeOS only because different OSes
    // use different text shaping engines.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    const SkColor letter_color = color_utils::GetColorWithMaxContrast(bg_color);
    if (size >= icon_size::k256) {
      SkColor center_color = bitmap.getColor(center_x, center_y);
      SCOPED_TRACE(letter_color);
      SCOPED_TRACE(center_color);
      EXPECT_TRUE(AreColorsEqual(letter_color, center_color, /*threshold=*/50));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    sizes.erase(size);
  }

  EXPECT_TRUE(sizes.empty());
}

TEST_F(WebAppIconGeneratorTest, ConvertImageToSolidFillMonochrome) {
  const int size_px = 512;
  const float scale = 1.0f;

  SkBitmap mixed_bitmap = CreateSquareIcon(size_px, SK_ColorBLUE);

  SkIRect transparent_area = SkIRect::MakeXYWH(128, 128, 256, 256);
  mixed_bitmap.erase(SK_ColorTRANSPARENT, transparent_area);

  gfx::ImageSkia weird_image;
  weird_image.AddRepresentation(gfx::ImageSkiaRep(mixed_bitmap, scale));

  gfx::ImageSkia converted =
      ConvertImageToSolidFillMonochrome(SK_ColorRED, weird_image);

  EXPECT_TRUE(converted.IsThreadSafe());
  ASSERT_TRUE(converted.HasRepresentation(scale));

  const SkBitmap& converted_bitmap =
      converted.GetRepresentation(scale).GetBitmap();

  EXPECT_EQ(converted_bitmap.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(converted_bitmap.getColor(256, 256), SK_ColorTRANSPARENT);
}

TEST_F(WebAppIconGeneratorTest, ConvertImageToSolidFillMonochrome_Empty) {
  gfx::ImageSkia converted =
      ConvertImageToSolidFillMonochrome(SK_ColorRED, gfx::ImageSkia());
  EXPECT_TRUE(converted.isNull());
}

}  // namespace web_app
