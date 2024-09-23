// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icns_encoder.h"

#include <ImageIO/ImageIO.h>

#include <numeric>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace web_app {

TEST(IcnsEncoderTest, AppendRLEImageData) {
  struct TestCase {
    std::vector<unsigned char> input;
    std::vector<unsigned char> expected;
    const char* description;
  } cases[] = {
      {{}, {}, "Empty input"},
      {{1, 2, 3, 4, 5}, {4, 1, 2, 3, 4, 5}, "Non compressible data"},
      {{1, 2, 3, 3, 3, 4, 5},
       {1, 1, 2, 0x80, 3, 1, 4, 5},
       "Compressible run in the middle"},
      {{5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6},
       {0x82, 5, 0x83, 6},
       "Multiple compressible runs"},
      {std::vector<unsigned char>(300, 5),
       {0xff, 5, 0xff, 5, 0xa5, 5},
       "Long compressible data"},
      {{}, {}, "Long uncompressible data"},
  };
  // Fill the input and expected output for the long uncompressible data case.
  auto& long_case = cases[5];
  long_case.input.resize(200);
  std::iota(long_case.input.begin(), long_case.input.end(), 0);
  long_case.expected = long_case.input;
  long_case.expected.insert(long_case.expected.begin(), 0x7F);
  long_case.expected.insert(long_case.expected.begin() + 0x7F + 2, 0x47);

  for (const auto& c : cases) {
    SCOPED_TRACE(c.description);
    std::vector<unsigned char> result;
    IcnsEncoder::AppendRLEImageData(c.input, &result);
    EXPECT_EQ(c.expected, result);
  }
}

namespace {

gfx::Image LoadTestPNG(const base::FilePath::CharType* path) {
  base::FilePath data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_root);
  base::FilePath image_path = data_root.Append(path);
  std::string png_data;
  ReadFileToString(image_path, &png_data);
  return gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(png_data));
}

}  // namespace

TEST(IcnsEncoderTest, RoundTrip) {
  // Load a couple of test images.
  gfx::Image basic_192 =
      LoadTestPNG(FILE_PATH_LITERAL("chrome/test/data/web_apps/basic-192.png"));
  ASSERT_TRUE(!basic_192.IsEmpty());
  gfx::Image green_128 = LoadTestPNG(FILE_PATH_LITERAL(
      "chrome/test/data/web_apps/standalone/128x128-green.png"));
  ASSERT_TRUE(!green_128.IsEmpty());
  gfx::Image basic_48 =
      LoadTestPNG(FILE_PATH_LITERAL("chrome/test/data/web_apps/basic-48.png"));
  ASSERT_TRUE(!basic_48.IsEmpty());
  gfx::Image chromium_32 = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/chromium-32.png"));
  ASSERT_TRUE(!chromium_32.IsEmpty());

  // Add the images to a IcnsEncoder.
  IcnsEncoder encoder;
  // 192x192 is not a supported image size
  EXPECT_FALSE(encoder.AddImage(basic_192));
  // 128, 48 and 32 are valid sizes, and should cover both encoding paths.
  EXPECT_TRUE(encoder.AddImage(green_128));
  EXPECT_TRUE(encoder.AddImage(basic_48));
  EXPECT_TRUE(encoder.AddImage(chromium_32));

  // Save the .icns file to disk.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath icon_path = temp_dir.GetPath().AppendASCII("app.icns");
  EXPECT_TRUE(encoder.WriteToFile(icon_path));

  // Now use Image I/O methods to load the .icns file back in.
  base::apple::ScopedCFTypeRef<CFDictionaryRef> empty_dict(
      CFDictionaryCreate(nullptr, nullptr, nullptr, 0, nullptr, nullptr));
  base::apple::ScopedCFTypeRef<CFURLRef> url =
      base::apple::FilePathToCFURL(icon_path);
  base::apple::ScopedCFTypeRef<CGImageSourceRef> source(
      CGImageSourceCreateWithURL(url.get(), nullptr));

  // And make sure we got back the same images that were written to the file.
  EXPECT_EQ(3u, CGImageSourceGetCount(source.get()));
  for (size_t i = 0; i < CGImageSourceGetCount(source.get()); ++i) {
    base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
        CGImageSourceCreateImageAtIndex(source.get(), i, empty_dict.get()));
    SkBitmap bitmap = skia::CGImageToSkBitmap(cg_image.get());
    EXPECT_EQ(bitmap.width(), bitmap.height());
    EXPECT_TRUE(bitmap.width() == 32 || bitmap.width() == 48 ||
                bitmap.width() == 128)
        << "Loaded width was " << bitmap.width();
    SCOPED_TRACE(bitmap.width());
    SkBitmap reference = bitmap.width() == 32   ? chromium_32.AsBitmap()
                         : bitmap.width() == 48 ? basic_48.AsBitmap()
                                                : green_128.AsBitmap();
    for (int y = 0; y < bitmap.height(); ++y) {
      std::vector<testing::Matcher<uint8_t>> expected_r, expected_g, expected_b,
          expected_a;
      std::vector<uint8_t> bitmap_r, bitmap_g, bitmap_b, bitmap_a;
      for (int x = 0; x < bitmap.width(); ++x) {
        SkColor expected_c = reference.getColor(x, y);
        uint8_t alpha = SkColorGetA(expected_c);
        expected_a.push_back(testing::Eq(alpha));
        expected_r.push_back(testing::Eq(SkColorGetR(expected_c)));
        expected_g.push_back(testing::Eq(SkColorGetG(expected_c)));
        expected_b.push_back(testing::Eq(SkColorGetB(expected_c)));

        SkColor bitmap_c = bitmap.getColor(x, y);
        bitmap_a.push_back(SkColorGetA(bitmap_c));
        bitmap_r.push_back(SkColorGetR(bitmap_c));
        bitmap_g.push_back(SkColorGetG(bitmap_c));
        bitmap_b.push_back(SkColorGetB(bitmap_c));
      }
      EXPECT_THAT(bitmap_r, testing::ElementsAreArray(expected_r))
          << "Row " << y;
      EXPECT_THAT(bitmap_g, testing::ElementsAreArray(expected_g))
          << "Row " << y;
      EXPECT_THAT(bitmap_b, testing::ElementsAreArray(expected_b))
          << "Row " << y;
      EXPECT_THAT(bitmap_a, testing::ElementsAreArray(expected_a))
          << "Row " << y;
    }
  }
}

}  // namespace web_app
