// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"

#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "printing/page_number.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace print_to_pdf {

namespace {

using printing::PageRange;
using printing::PageRanges;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::VariantWith;

TEST(PageRangeTextToPagesTest, General) {
  EXPECT_THAT(
      TextPageRangesToPageRanges("-"),
      VariantWith<PageRanges>(ElementsAre(PageRange{0, PageRange::kMaxPage})));

  // If no start page is specified, we start at the first page.
  EXPECT_THAT(TextPageRangesToPageRanges("-5"),
              VariantWith<PageRanges>(ElementsAre(PageRange{0, 4})));

  // If no end page is specified, we end at the last page.
  EXPECT_THAT(
      TextPageRangesToPageRanges("5-"),
      VariantWith<PageRanges>(ElementsAre(PageRange{4, PageRange::kMaxPage})));

  // Multiple ranges are separated by commas.
  EXPECT_THAT(TextPageRangesToPageRanges("1-3,9-10,4-6"),
              VariantWith<PageRanges>(
                  ElementsAreArray<PageRange>({{0, 2}, {8, 9}, {3, 5}})));

  // White space is ignored.
  EXPECT_THAT(TextPageRangesToPageRanges("1- 3, 9-10,4 -6"),
              VariantWith<PageRanges>(
                  ElementsAreArray<PageRange>({{0, 2}, {8, 9}, {3, 5}})));

  // Range with a start page greater than the end page results in an error.
  EXPECT_THAT(
      TextPageRangesToPageRanges("6-4"),
      VariantWith<PdfPrintResult>(PdfPrintResult::kPageRangeInvalidRange));

  // Invalid input results in an error.
  EXPECT_THAT(
      TextPageRangesToPageRanges("abcd"),
      VariantWith<PdfPrintResult>(PdfPrintResult::kPageRangeSyntaxError));

  // Invalid input results in an error.
  EXPECT_THAT(
      TextPageRangesToPageRanges("1-3,9-a10,4-6"),
      VariantWith<PdfPrintResult>(PdfPrintResult::kPageRangeSyntaxError));
}

}  // namespace

}  // namespace print_to_pdf
