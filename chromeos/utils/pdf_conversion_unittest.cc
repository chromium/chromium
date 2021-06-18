// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/pdf_conversion.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"

namespace chromeos {

namespace {

// Returns a manually generated JPG image.
std::vector<uint8_t> CreateJpg() {
  gfx::Image original = gfx::test::CreateImage(100, 100);
  std::vector<uint8_t> jpg_buffer;
  if (!gfx::JPEG1xEncodedDataFromImage(original, 80, &jpg_buffer)) {
    return {};
  }
  return jpg_buffer;
}

}  // namespace

using ConvertToPdfTest = testing::Test;

// Test that JPG image can be converted to pdf file successfully.
TEST_F(ConvertToPdfTest, ToFile) {
  std::vector<std::string> images;
  std::vector<uint8_t> bytes = CreateJpg();
  images.push_back(std::string(bytes.begin(), bytes.end()));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto output_path = temp_dir.GetPath().Append("temp.pdf");

  EXPECT_TRUE(ConvertJpgImagesToPdf(images, output_path,
                                    /*rotate_alternate_pages=*/false));
  EXPECT_TRUE(base::PathExists(output_path));

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(output_path, &file_size));

  // Smallest PDF should be at least 20 bytes.
  EXPECT_GT(file_size, 20u);
}

// Test that JPG image can be converted to pdf and saved to vector successfully.
TEST_F(ConvertToPdfTest, ToVector) {
  std::vector<uint8_t> jpg_buffer = CreateJpg();
  ASSERT_FALSE(jpg_buffer.empty());

  std::vector<uint8_t> pdf_buffer;
  EXPECT_TRUE(ConvertJpgImageToPdf(jpg_buffer, &pdf_buffer));

  // Smallest PDF should be at least 20 bytes.
  EXPECT_GT(pdf_buffer.size(), 20u);
}

}  // namespace chromeos
