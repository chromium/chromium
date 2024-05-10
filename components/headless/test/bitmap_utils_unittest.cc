// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/bitmap_utils.h"

#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace headless {

namespace {

bool DecodePNG(const std::string& png_data, SkBitmap* bitmap) {
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(png_data.data()), png_data.size(),
      bitmap);
}

SkBitmap LoadTestImage(std::string_view file_name) {
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
  path = path.AppendASCII("components")
             .AppendASCII("test")
             .AppendASCII("data")
             .AppendASCII("headless")
             .AppendASCII(file_name);
  CHECK(base::PathExists(path)) << path;

  std::string png_data;
  CHECK(base::ReadFileToString(path, &png_data)) << path;

  SkBitmap bitmap;
  CHECK(DecodePNG(png_data, &bitmap)) << path;

  return bitmap;
}

}  // namespace

struct ColorRectTestData {
  const char* file_name;
  const bool expected_result;
};

constexpr ColorRectTestData kColorRectTestData[] = {
    {"green_box.png", true},
    {"green_box_anti_aliasing.png", true},
    {"green_box_with_red.png", false},
};

class HeadlessBitmapUtilsColorRectTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ColorRectTestData> {
 public:
  const char* GetFileName() { return GetParam().file_name; }
  bool GetExpectedResult() { return GetParam().expected_result; }
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessBitmapUtilsColorRectTest,
                         ::testing::ValuesIn(kColorRectTestData));

TEST_P(HeadlessBitmapUtilsColorRectTest, Basic) {
  SkBitmap bitmap = LoadTestImage(GetFileName());

  // Expect a centered green rectangle on white background.
  EXPECT_EQ(CheckColoredRect(bitmap, SkColorSetRGB(0x00, 0xff, 0x00),
                             SkColorSetRGB(0xff, 0xff, 0xff)),
            GetExpectedResult());
}

}  // namespace headless
