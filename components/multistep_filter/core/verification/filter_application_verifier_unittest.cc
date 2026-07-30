// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/verification/filter_application_verifier.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/verification/suggestion_application_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {
namespace {

UrlFilterSuggestion CreateSuggestion(
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  std::vector<FilterAttributeUiLabel> ui_labels;
  for (const auto& [key, val] : attributes) {
    ui_labels.emplace_back(FilterSuggestionCandidateAttribute(key, u"label"),
                           FilterAttribute(key, val));
  }
  return UrlFilterSuggestion(UrlFilterSuggestion::Params{
      .navigation_url = GURL("https://example.com/filter"),
      .source_host = u"example.com",
      .extraction_timestamp = base::Time::Now(),
      .attribute_ui_labels = std::move(ui_labels),
      .triggering_navigation_id = 1,
      .triggering_host = "example.com",
      .task_type = "test_task",
      .suggestion_message = u"Suggestion",
      .short_suggestion_message = u"Short",
  });
}

FilterAnnotation CreateAnnotation(
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  std::vector<FilterAttribute> attrs;
  for (const auto& [key, val] : attributes) {
    attrs.emplace_back(key, val);
  }
  return FilterAnnotation(base::Uuid::GenerateRandomV4(), "test_task",
                          "example.com", base::Time::Now(), std::move(attrs));
}

TEST(FilterApplicationVerifierTest, VerifyOutcome_Success) {
  UrlFilterSuggestion suggestion =
      CreateSuggestion({{"color", "red"}, {"size", "XL"}});
  FilterAnnotation annotation =
      CreateAnnotation({{"color", "red"}, {"size", "XL"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_TRUE(result.is_success());
  EXPECT_EQ(result.outcome, SuggestionApplicationResult::kAllFiltersApplied);
  EXPECT_TRUE(result.missing_keys.empty());
}

TEST(FilterApplicationVerifierTest, VerifyOutcome_NoExtractedAnnotations) {
  UrlFilterSuggestion suggestion = CreateSuggestion({{"color", "red"}});
  FilterAnnotation empty_annotation = CreateAnnotation({});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, empty_annotation);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedNoExtractedAnnotations);
}

TEST(FilterApplicationVerifierTest, VerifyOutcome_NullAnnotation) {
  UrlFilterSuggestion suggestion = CreateSuggestion({{"color", "red"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, std::nullopt);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedNoExtractedAnnotations);
}

TEST(FilterApplicationVerifierTest,
     VerifyOutcome_MissingSuggestedFilters_Failure) {
  UrlFilterSuggestion suggestion =
      CreateSuggestion({{"color", "red"}, {"size", "XL"}});
  FilterAnnotation annotation = CreateAnnotation({{"color", "red"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedAttributeMismatch);
  ASSERT_EQ(result.missing_keys.size(), 1u);
  EXPECT_EQ(result.missing_keys[0], "size");
}

TEST(FilterApplicationVerifierTest,
     VerifyOutcome_ExtraExtractedAnnotations_Success) {
  UrlFilterSuggestion suggestion = CreateSuggestion({{"color", "red"}});
  FilterAnnotation annotation =
      CreateAnnotation({{"color", "red"}, {"size", "XL"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_TRUE(result.is_success());
  EXPECT_EQ(result.outcome, SuggestionApplicationResult::kAllFiltersApplied);
  EXPECT_TRUE(result.missing_keys.empty());
}

TEST(FilterApplicationVerifierTest, VerifyOutcome_AttributeMismatch) {
  UrlFilterSuggestion suggestion =
      CreateSuggestion({{"color", "red"}, {"size", "XL"}});
  FilterAnnotation annotation =
      CreateAnnotation({{"color", "red"}, {"brand", "Acme"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedAttributeMismatch);
  ASSERT_EQ(result.missing_keys.size(), 1u);
  EXPECT_EQ(result.missing_keys[0], "size");
}

TEST(FilterApplicationVerifierTest, VerifyOutcome_ValueMismatch) {
  UrlFilterSuggestion suggestion =
      CreateSuggestion({{"color", "red"}, {"size", "XL"}});
  FilterAnnotation annotation =
      CreateAnnotation({{"color", "blue"}, {"size", "XL"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedAttributeMismatch);
  ASSERT_EQ(result.missing_keys.size(), 1u);
  EXPECT_EQ(result.missing_keys[0], "color");
}

TEST(FilterApplicationVerifierTest,
     VerifyOutcome_DuplicateMissingKeys_Uniqued) {
  UrlFilterSuggestion suggestion =
      CreateSuggestion({{"color", "red"}, {"color", "blue"}});
  FilterAnnotation annotation = CreateAnnotation({{"size", "XL"}});

  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(suggestion, annotation);

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.outcome,
            SuggestionApplicationResult::kFailedAttributeMismatch);
  ASSERT_EQ(result.missing_keys.size(), 1u);
  EXPECT_EQ(result.missing_keys[0], "color");
}

}  // namespace
}  // namespace multistep_filter
