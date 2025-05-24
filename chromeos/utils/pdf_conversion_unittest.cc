// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/pdf_conversion.h"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"

namespace chromeos {

namespace {

// Returns a manually generated JPG image with specified width and height in
// pixels.
std::vector<uint8_t> CreateJpg(int width, int height) {
  gfx::Image original = gfx::test::CreateImage(width, height);
  std::optional<std::vector<uint8_t>> jpg_buffer =
      gfx::JPEG1xEncodedDataFromImage(original, /*quality=*/80);
  return jpg_buffer.value_or(std::vector<uint8_t>());
}

}  // namespace

using ConvertToPdfTest = testing::Test;

// Test that JPG image can be converted to pdf file successfully.
TEST_F(ConvertToPdfTest, ToFileNoDpi) {
  std::vector<std::string> images;
  std::vector<uint8_t> bytes = CreateJpg(100, 100);
  images.push_back(std::string(bytes.begin(), bytes.end()));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto output_path = temp_dir.GetPath().Append("temp.pdf");

  EXPECT_TRUE(ConvertJpgImagesToPdf(images, output_path,
                                    /*rotate_alternate_pages=*/false,
                                    /*dpi=*/std::nullopt));
  EXPECT_TRUE(base::PathExists(output_path));

  std::optional<int64_t> file_size = base::GetFileSize(output_path);
  ASSERT_TRUE(file_size.has_value());

  // Smallest PDF should be at least 20 bytes.
  EXPECT_GT(file_size.value(), 20u);
}

// Test that JPG image can be converted to pdf file successfully when scanner
// DPI is specified. Higher DPI results in larger pixel counts, but should
// result in the same PDF page size.
TEST_F(ConvertToPdfTest, ToFileWithDpi) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Generate and process 100 DPI image.
  std::vector<std::string> images_100;
  std::vector<uint8_t> bytes_100 = CreateJpg(100, 100);
  images_100.push_back(std::string(bytes_100.begin(), bytes_100.end()));
  auto output_path_100 = temp_dir.GetPath().Append("temp_100.pdf");
  EXPECT_TRUE(ConvertJpgImagesToPdf(images_100, output_path_100,
                                    /*rotate_alternate_pages=*/false,
                                    /*dpi=*/100));
  EXPECT_TRUE(base::PathExists(output_path_100));

  // Generate and process 200 DPI image.
  std::vector<std::string> images_200;
  std::vector<uint8_t> bytes_200 = CreateJpg(200, 200);
  images_200.push_back(std::string(bytes_200.begin(), bytes_200.end()));
  auto output_path_200 = temp_dir.GetPath().Append("temp_200.pdf");
  EXPECT_TRUE(ConvertJpgImagesToPdf(images_200, output_path_200,
                                    /*rotate_alternate_pages=*/false,
                                    /*dpi=*/200));
  EXPECT_TRUE(base::PathExists(output_path_200));

  // Generate and process 300 DPI image.
  std::vector<std::string> images_300;
  std::vector<uint8_t> bytes_300 = CreateJpg(300, 300);
  images_300.push_back(std::string(bytes_300.begin(), bytes_300.end()));
  auto output_path_300 = temp_dir.GetPath().Append("temp_300.pdf");
  EXPECT_TRUE(ConvertJpgImagesToPdf(images_300, output_path_300,
                                    /*rotate_alternate_pages=*/false,
                                    /*dpi=*/300));
  EXPECT_TRUE(base::PathExists(output_path_300));

  // Each file should increase in size as DPI increases.
  std::optional<int64_t> file_size_100 = base::GetFileSize(output_path_100);
  std::optional<int64_t> file_size_200 = base::GetFileSize(output_path_200);
  std::optional<int64_t> file_size_300 = base::GetFileSize(output_path_300);

  ASSERT_TRUE(file_size_100.has_value());
  ASSERT_TRUE(file_size_200.has_value());
  ASSERT_TRUE(file_size_300.has_value());
  EXPECT_GT(file_size_200.value(), file_size_100.value());
  EXPECT_GT(file_size_300.value(), file_size_200.value());

  // Verify that the media box is the same size across PDFs.
  const char kMediaBoxString[] = "[0 0 72 72]";
  std::string file_contents;

  EXPECT_TRUE(base::ReadFileToString(output_path_100, &file_contents));
  EXPECT_THAT(file_contents, testing::HasSubstr(kMediaBoxString));

  EXPECT_TRUE(base::ReadFileToString(output_path_200, &file_contents));
  EXPECT_THAT(file_contents, testing::HasSubstr(kMediaBoxString));

  EXPECT_TRUE(base::ReadFileToString(output_path_300, &file_contents));
  EXPECT_THAT(file_contents, testing::HasSubstr(kMediaBoxString));
}

// Test that JPG images can be converted to pdf and saved to vector
// successfully.
TEST_F(ConvertToPdfTest, ToVector) {
  std::vector<uint8_t> pdf_buffer;
  std::vector<uint8_t> buffer_1 = CreateJpg(1, 1);
  ASSERT_FALSE(buffer_1.empty());

  std::vector<std::vector<uint8_t>> jpg_buffers{buffer_1};
  EXPECT_TRUE(ConvertJpgImagesToPdf(jpg_buffers, &pdf_buffer));

  // Smallest PDF should be at least 20 bytes.
  size_t size_1 = pdf_buffer.size();
  EXPECT_GT(size_1, 20u);

  jpg_buffers.push_back(CreateJpg(200, 200));
  jpg_buffers.push_back(CreateJpg(300, 300));
  EXPECT_TRUE(ConvertJpgImagesToPdf(jpg_buffers, &pdf_buffer));
  EXPECT_GT(pdf_buffer.size(), size_1);
}

// Test that passing empty vectors should fail and print error messages.
TEST_F(ConvertToPdfTest, ToVectorEmpty) {
  std::vector<uint8_t> pdf_buffer;
  std::vector<uint8_t> buffer_empty;
  ASSERT_TRUE(buffer_empty.empty());

  std::vector<std::vector<uint8_t>> jpg_buffers{buffer_empty};
  EXPECT_FALSE(ConvertJpgImagesToPdf(jpg_buffers, &pdf_buffer));
}

}  // namespace chromeos
