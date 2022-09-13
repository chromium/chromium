// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/content/renderer/content_page_annotator_driver.h"

#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_image_annotation {

using ::testing::Eq;
using ::testing::IsEmpty;

TEST(ContentPageAnnotatorDriverTest, GenerateSourceIdFailure) {
  // Degenerate case: no info at all.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(GURL(""), ""),
              IsEmpty());

  // Empty page URL.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  GURL(""), "https://absolute.com/img.jpg"),
              IsEmpty());

  // Invalid page URL for a relative src.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  GURL("invalid_page_url"), "relative.jpg"),
              IsEmpty());

  // Invalid page URL for an absolute src.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  GURL("invalid_page_url"), "https://absolute.com/img.jpg"),
              IsEmpty());

  // No relative src.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  GURL("https://website.com"), ""),
              IsEmpty());

  // No data URI.
  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  GURL("http://website.com"), "data:"),
              IsEmpty());
}

TEST(ContentPageAnnotatorDriverTest, GenerateSourceIdSuccess) {
  const GURL page_url("https://website.com/folder/page.html");

  EXPECT_THAT(
      ContentPageAnnotatorDriver::GenerateSourceId(page_url, "relative.jpg"),
      Eq("https://website.com/folder/relative.jpg"));

  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  page_url, "https://absolute.com/img.jpg"),
              Eq("https://absolute.com/img.jpg"));

  // Arbitrary image data.
  constexpr char kImageData[] = "image/png;base64,ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  // The SHA256 hash of the above data encoded in base 64.
  constexpr char kBase64Sha256Output[] =
      "/xjMyCY9UxaB3RbVCyijwPy4/LI6NpO/hCDGQJrJttc=";

  EXPECT_THAT(ContentPageAnnotatorDriver::GenerateSourceId(
                  page_url, base::StrCat({"data:", kImageData})),
              Eq(kBase64Sha256Output));
}

}  // namespace page_image_annotation
