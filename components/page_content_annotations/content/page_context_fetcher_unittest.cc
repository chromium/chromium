// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace page_content_annotations {

TEST(PageContextFetcherTest, RedactScreenshotOnWorkerThread) {
  base::HistogramTester histograms;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  std::vector<gfx::Rect> redaction_rects;
  redaction_rects.emplace_back(10, 10, 20, 20);

  base::expected<SkBitmap, std::string> redacted =
      PageContextFetcher::RedactScreenshotOnWorkerThread(
          bitmap, redaction_rects, SkColors::kRed);

  ASSERT_TRUE(redacted.has_value());
  ASSERT_EQ(redacted->width(), 100);
  ASSERT_EQ(redacted->height(), 100);

  // Check a pixel that should NOT be redacted (remains blue).
  EXPECT_EQ(redacted->getColor(5, 5), SK_ColorBLUE);
  EXPECT_EQ(redacted->getColor(50, 50), SK_ColorBLUE);

  // Check a pixel that SHOULD be redacted (becomes red).
  EXPECT_EQ(redacted->getColor(15, 15), SK_ColorRED);
  EXPECT_EQ(redacted->getColor(25, 25), SK_ColorRED);

  histograms.ExpectUniqueSample("Glic.PageContextFetcher.ScreenshotRedacted",
                                true, 1);
}

TEST(PageContextFetcherTest, RedactScreenshotOnWorkerThreadNoRedaction) {
  base::HistogramTester histograms;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  std::vector<gfx::Rect> redaction_rects;

  base::expected<SkBitmap, std::string> redacted =
      PageContextFetcher::RedactScreenshotOnWorkerThread(
          bitmap, redaction_rects, SkColors::kRed);

  ASSERT_TRUE(redacted.has_value());
  // Verify the result is correct.
  EXPECT_EQ(redacted->getColor(50, 50), SK_ColorBLUE);
  // Verify the optimization: the original bitmap should be returned without
  // unnecessary copies when no redaction is performed.
  EXPECT_EQ(bitmap.getPixels(), redacted->getPixels());

  histograms.ExpectUniqueSample("Glic.PageContextFetcher.ScreenshotRedacted",
                                false, 1);
}

}  // namespace page_content_annotations
