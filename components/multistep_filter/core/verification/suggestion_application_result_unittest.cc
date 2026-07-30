// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/verification/suggestion_application_result.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

TEST(SuggestionApplicationResultTest, ToString) {
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kAllFiltersApplied),
            "all_filters_applied");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kNotAllFiltersApplied),
            "not_all_filters_applied");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kAbandonedBeforeVerification),
            "abandoned_before_verification");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kFailedErrorPage),
            "failed_error_page");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kFailedNoExtractedAnnotations),
            "error_no_extracted_annotations");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kFailedCountMismatch),
            "error_filter_count_mismatch");
  EXPECT_EQ(SuggestionApplicationResultToString(
                SuggestionApplicationResult::kFailedAttributeMismatch),
            "error_attribute_mismatch");
}

}  // namespace multistep_filter
