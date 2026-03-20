// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

#include <utility>

#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

TEST(UrlFilterSuggestionTest, ConstructorFromFilterSuggestionCandidate) {
  FilterSuggestionCandidate candidate("test_id", GURL("https://example.com"),
                                      {});

  UrlFilterSuggestion suggestion(std::move(candidate));

  EXPECT_EQ(suggestion.url(), GURL("https://example.com"));
  EXPECT_EQ(suggestion.text(), "Recall info from previous tabs?");
}

}  // namespace multistep_filter
