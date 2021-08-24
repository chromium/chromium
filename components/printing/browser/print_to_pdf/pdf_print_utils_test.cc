// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace print_to_pdf {

namespace {

std::vector<uint32_t> GetPages(
    absl::variant<printing::PageRanges, PageRangeError> result) {
  return printing::PageRange::GetPages(absl::get<printing::PageRanges>(result));
}

PageRangeError GetError(
    absl::variant<printing::PageRanges, PageRangeError> result) {
  return absl::get<PageRangeError>(result);
}

TEST(PageRangeTextToPagesTest, General) {
  absl::variant<printing::PageRanges, PageRangeError> result;
  std::vector<uint32_t> expected_pages;

  // "-" is the full range of pages.
  result = TextPageRangesToPageRanges("-", false, 10);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(GetPages(result), expected_pages);

  // If no start page is specified, we start at the first page.
  result = TextPageRangesToPageRanges("-5", false, 10);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(GetPages(result), expected_pages);

  // If no end page is specified, we end at the last page.
  result = TextPageRangesToPageRanges("5-", false, 10);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {4, 5, 6, 7, 8, 9};
  EXPECT_EQ(GetPages(result), expected_pages);

  // Multiple ranges are separated by commas.
  result = TextPageRangesToPageRanges("1-3,9-10,4-6", false, 10);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(GetPages(result), expected_pages);

  // White space is ignored.
  result = TextPageRangesToPageRanges("1- 3, 9-10,4 -6", false, 10);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(GetPages(result), expected_pages);

  // End page beyond number of pages is supported and capped to number
  // of pages.
  result = TextPageRangesToPageRanges("1-10", false, 5);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(GetPages(result), expected_pages);

  // Start page beyond number of pages results in an error.
  result = TextPageRangesToPageRanges("1-3,9-10,4-6", false, 5);
  EXPECT_FALSE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_TRUE(absl::holds_alternative<PageRangeError>(result));
  EXPECT_EQ(GetError(result), PageRangeError::LIMIT_ERROR);

  // Invalid page ranges are ignored if |ignore_invalid_page_ranges| is
  // in effect.
  result = TextPageRangesToPageRanges("9-10,4-6,3-1", true, 5);
  EXPECT_TRUE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_FALSE(absl::holds_alternative<PageRangeError>(result));
  expected_pages = {3, 4};
  EXPECT_EQ(GetPages(result), expected_pages);

  // Invalid input results in an error.
  result = TextPageRangesToPageRanges("abcd", false, 10);
  EXPECT_FALSE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_TRUE(absl::holds_alternative<PageRangeError>(result));
  EXPECT_EQ(GetError(result), PageRangeError::SYNTAX_ERROR);

  // Invalid input results in an error.
  result = TextPageRangesToPageRanges("1-3,9-a10,4-6", false, 10);
  EXPECT_FALSE(absl::holds_alternative<printing::PageRanges>(result));
  EXPECT_TRUE(absl::holds_alternative<PageRangeError>(result));
  EXPECT_EQ(GetError(result), PageRangeError::SYNTAX_ERROR);
}

}  // namespace

}  // namespace print_to_pdf
