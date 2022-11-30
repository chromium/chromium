// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/fake_content_suggestions_provider_observer.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

using testing::Eq;
using testing::Not;

FakeContentSuggestionsProviderObserver::
    FakeContentSuggestionsProviderObserver() = default;

FakeContentSuggestionsProviderObserver::
    ~FakeContentSuggestionsProviderObserver() = default;

void FakeContentSuggestionsProviderObserver::OnNewSuggestions(
    ContentSuggestionsProvider* provider,
    Category category,
    std::vector<ContentSuggestion> suggestions) {
  suggestions_[category] = std::move(suggestions);
}

void FakeContentSuggestionsProviderObserver::OnCategoryStatusChanged(
    ContentSuggestionsProvider* provider,
    Category category,
    CategoryStatus new_status) {
  statuses_[category] = new_status;
}

void FakeContentSuggestionsProviderObserver::OnSuggestionInvalidated(
    ContentSuggestionsProvider* provider,
    const ContentSuggestion::ID& suggestion_id) {
  FAIL() << "not implemented.";
}

const std::map<Category, CategoryStatus, Category::CompareByID>&
FakeContentSuggestionsProviderObserver::statuses() const {
  return statuses_;
}

CategoryStatus FakeContentSuggestionsProviderObserver::StatusForCategory(
    Category category) const {
  auto it = statuses_.find(category);
  EXPECT_THAT(it, Not(Eq(statuses_.end())));
  return it->second;
}

const std::vector<ContentSuggestion>&
FakeContentSuggestionsProviderObserver::SuggestionsForCategory(
    Category category) {
  return suggestions_[category];
}

}  // namespace ntp_snippets
