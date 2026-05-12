// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_sanitizer.h"

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

drive_picker_host::mojom::DriveFilePtr CreateValidFile() {
  auto file = drive_picker_host::mojom::DriveFile::New();
  file->id = "valid-id_123";
  file->mime_type = "application/pdf";
  file->name = "test.pdf";
  file->type = "document";
  file->size_bytes = 1024;
  return file;
}

}  // namespace

class DrivePickerSanitizerTest : public testing::Test {};

TEST_F(DrivePickerSanitizerTest, SanitizeValidDocument) {
  auto file = CreateValidFile();
  file->resource_key = "valid-key";

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  ASSERT_TRUE(sanitized.has_value());
  EXPECT_EQ(sanitized->drive_id, "valid-id_123");
  EXPECT_EQ(sanitized->mime_type, "application/pdf");
  EXPECT_EQ(sanitized->file_name, "test.pdf");
  EXPECT_EQ(sanitized->size_bytes, 1024u);
  EXPECT_EQ(sanitized->resource_key, "valid-key");
  EXPECT_FALSE(sanitized->thumbnail_url.has_value());
}

TEST_F(DrivePickerSanitizerTest, SanitizeValidPhotoWithThumbnail) {
  auto file = CreateValidFile();
  file->type = "photo";
  GURL valid_url(
      "https://lh3.googleusercontent.com/drive-storage/"
      "AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP-ZYLdkxOcPlUxsVOMr5jn-"
      "i261zxtYVcpA0kZeP"
      "sVWT2Ghrxdg03aoJYX7t9vlc4ojecNyW7QNrP6muY9-"
      "wvmO77SsiXHrwnU2GbRCt2V9NDv62u2"
      "R96rpvI=s220");
  file->thumbnail_url = valid_url;

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  ASSERT_TRUE(sanitized.has_value());
  ASSERT_TRUE(sanitized->thumbnail_url.has_value());
  EXPECT_EQ(sanitized->thumbnail_url.value(), valid_url);
}

TEST_F(DrivePickerSanitizerTest, BlocksInvalidThumbnailUrl) {
  {
    auto file = CreateValidFile();
    file->type = "photo";
    file->thumbnail_url = GURL("https://malicious.com/not-drive-storage/AJQWt");
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    ASSERT_TRUE(sanitized.has_value());
    EXPECT_FALSE(sanitized->thumbnail_url.has_value());
  }
  // Blocks http scheme even if host is valid.
  {
    auto file = CreateValidFile();
    file->type = "photo";
    file->thumbnail_url =
        GURL("http://lh3.googleusercontent.com/drive-storage/AJQWt");
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    ASSERT_TRUE(sanitized.has_value());
    EXPECT_FALSE(sanitized->thumbnail_url.has_value());
  }
  // Blocks chrome-extension scheme even if host is valid.
  {
    auto file = CreateValidFile();
    file->type = "photo";
    file->thumbnail_url = GURL(
        "chrome-extension://lh3.googleusercontent.com/drive-storage/AJQWt");
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    ASSERT_TRUE(sanitized.has_value());
    EXPECT_FALSE(sanitized->thumbnail_url.has_value());
  }
}

TEST_F(DrivePickerSanitizerTest, BlocksThumbnailForNonPhoto) {
  auto file = CreateValidFile();
  file->type = "document";
  file->thumbnail_url = GURL(
      "https://lh3.googleusercontent.com/drive-storage/"
      "AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP-ZYLdkxOcPlUxsVOMr5jn-"
      "i261zxtYVcpA0kZeP"
      "sVWT2Ghrxdg03aoJYX7t9vlc4ojecNyW7QNrP6muY9-"
      "wvmO77SsiXHrwnU2GbRCt2V9NDv62u2"
      "R96rpvI=s220");

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  ASSERT_TRUE(sanitized.has_value());
  EXPECT_FALSE(sanitized->thumbnail_url.has_value());
}

TEST_F(DrivePickerSanitizerTest, ThrowsOnInvalidId) {
  auto file = CreateValidFile();
  file->id = "invalid id with spaces";

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  EXPECT_FALSE(sanitized.has_value());
}

TEST_F(DrivePickerSanitizerTest, ThrowsOnUnsupportedType) {
  auto file = CreateValidFile();
  file->type = "unsupported";

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  EXPECT_FALSE(sanitized.has_value());
}

TEST_F(DrivePickerSanitizerTest, SanitizesInvalidResourceKey) {
  auto file = CreateValidFile();
  file->resource_key = "invalid key";

  auto sanitized = DrivePickerSanitizer::Sanitize(file);

  ASSERT_TRUE(sanitized.has_value());
  EXPECT_FALSE(sanitized->resource_key.has_value());
}

TEST_F(DrivePickerSanitizerTest, ThrowsOnInvalidMetadata) {
  {
    auto file = CreateValidFile();
    file->name = "";
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    EXPECT_FALSE(sanitized.has_value());
  }
  {
    auto file = CreateValidFile();
    file->mime_type = "";
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    EXPECT_FALSE(sanitized.has_value());
  }
  {
    auto file = CreateValidFile();
    file->mime_type = "invalid-mime-no-slash";
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    EXPECT_FALSE(sanitized.has_value());
  }
  {
    auto file = CreateValidFile();
    file->mime_type = "text / html";  // Spaces are generally not allowed in the
                                      // type/subtype tokens
    auto sanitized = DrivePickerSanitizer::Sanitize(file);
    EXPECT_FALSE(sanitized.has_value());
  }
}
