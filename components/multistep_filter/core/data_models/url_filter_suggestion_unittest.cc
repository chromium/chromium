// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {
constexpr int64_t kTriggeringNavigationId = 123;

TEST(FilterAttributeUiLabelTest, ToString) {
  FilterAttributeUiLabel label(
      FilterSuggestionCandidateAttribute("key", u"label"),
      FilterAttribute("key", "value"));
  EXPECT_EQ(label.ToString(),
            "FilterAttributeUiLabel(label=label, value=value)");
}

TEST(FilterAttributeUiLabelTest, Equality) {
  FilterAttributeUiLabel label1(
      FilterSuggestionCandidateAttribute("key", u"label"),
      FilterAttribute("key", "value"));
  FilterAttributeUiLabel label2(
      FilterSuggestionCandidateAttribute("key", u"label"),
      FilterAttribute("key", "value"));
  EXPECT_EQ(label1, label2);

  label2.attribute_value = u"other";
  EXPECT_NE(label1, label2);
}

TEST(UrlFilterSuggestionTest, CopyAndMove) {
  UrlFilterSuggestion suggestion(
      GURL("https://example.com"), u"domain", base::Time::Now(),
      {FilterAttributeUiLabel(
          FilterSuggestionCandidateAttribute("key1", u"label1"),
          FilterAttribute("key1", "val1"))},
      kTriggeringNavigationId, "example.com");

  UrlFilterSuggestion copy = suggestion;
  EXPECT_EQ(copy, suggestion);

  UrlFilterSuggestion moved = std::move(suggestion);
  EXPECT_EQ(moved, copy);
}

TEST(UrlFilterSuggestionTest, Equality) {
  UrlFilterSuggestion suggestion1(
      GURL("https://example.com"), u"domain", base::Time::Now(),
      {FilterAttributeUiLabel(
          FilterSuggestionCandidateAttribute("key1", u"label1"),
          FilterAttribute("key1", "val1"))},
      kTriggeringNavigationId, "example.com");
  UrlFilterSuggestion suggestion2 = suggestion1;

  EXPECT_EQ(suggestion1, suggestion2);

  suggestion2.source_domain = u"other";
  EXPECT_NE(suggestion1, suggestion2);

  suggestion2 = suggestion1;
  suggestion2.triggering_domain = "other.com";
  EXPECT_NE(suggestion1, suggestion2);

  suggestion2 = suggestion1;
  suggestion2.navigation_url = GURL("https://other.com");
  EXPECT_NE(suggestion1, suggestion2);

  suggestion2 = suggestion1;
  suggestion2.extraction_timestamp += base::Seconds(1);
  EXPECT_NE(suggestion1, suggestion2);

  suggestion2 = suggestion1;
  suggestion2.attribute_ui_labels.clear();
  EXPECT_NE(suggestion1, suggestion2);

  suggestion2 = suggestion1;
  suggestion2.triggering_navigation_id = 456;
  EXPECT_NE(suggestion1, suggestion2);
}

TEST(UrlFilterSuggestionTest, ToString) {
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromUTCString("2026-03-23 17:00:00 UTC", &timestamp));

  UrlFilterSuggestion suggestion(
      GURL("https://example.com"), u"domain", timestamp,
      {FilterAttributeUiLabel(
          FilterSuggestionCandidateAttribute("key1", u"label1"),
          FilterAttribute("key1", "val1"))},
      kTriggeringNavigationId, "example.com");

  EXPECT_EQ(suggestion.ToString(),
            "UrlFilterSuggestion(navigation_url=https://example.com/, "
            "source_domain=domain, "
            "extraction_timestamp=" +
                base::NumberToString(
                    timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()) +
                ", attribute_ui_labels=[FilterAttributeUiLabel(label=label1, "
                "value=val1)], triggering_navigation_id=" +
                base::NumberToString(kTriggeringNavigationId) +
                ", triggering_domain=example.com)");
}

TEST(UrlFilterSuggestionTest, ToStringMultipleAttributes) {
  base::Time timestamp = base::Time::Now();

  UrlFilterSuggestion suggestion(
      GURL("https://example.com"), u"domain", timestamp,
      {FilterAttributeUiLabel(
           FilterSuggestionCandidateAttribute("key1", u"label1"),
           FilterAttribute("key1", "val1")),
       FilterAttributeUiLabel(
           FilterSuggestionCandidateAttribute("key2", u"label2"),
           FilterAttribute("key2", "val2"))},
      kTriggeringNavigationId, "example.com");

  EXPECT_EQ(suggestion.ToString(),
            "UrlFilterSuggestion(navigation_url=https://example.com/, "
            "source_domain=domain, "
            "extraction_timestamp=" +
                base::NumberToString(
                    timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()) +
                ", attribute_ui_labels=[FilterAttributeUiLabel(label=label1, "
                "value=val1), FilterAttributeUiLabel(label=label2, "
                "value=val2)], triggering_navigation_id=" +
                base::NumberToString(kTriggeringNavigationId) +
                ", triggering_domain=example.com)");
}

}  // namespace
}  // namespace multistep_filter
