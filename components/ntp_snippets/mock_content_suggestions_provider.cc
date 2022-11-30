// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/mock_content_suggestions_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"

namespace ntp_snippets {

MockContentSuggestionsProvider::MockContentSuggestionsProvider(
    Observer* observer,
    const std::vector<Category>& provided_categories)
    : ContentSuggestionsProvider(observer) {
  SetProvidedCategories(provided_categories);
}

MockContentSuggestionsProvider::~MockContentSuggestionsProvider() {
  if (destructor_callback_) {
    std::move(destructor_callback_).Run();
  }
}

void MockContentSuggestionsProvider::SetProvidedCategories(
    const std::vector<Category>& provided_categories) {
  statuses_.clear();
  provided_categories_ = provided_categories;
  for (Category category : provided_categories) {
    statuses_[category.id()] = CategoryStatus::AVAILABLE;
  }
}

CategoryStatus MockContentSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return statuses_[category.id()];
}

CategoryInfo MockContentSuggestionsProvider::GetCategoryInfo(
    Category category) {
  return CategoryInfo(u"Section title", ContentSuggestionsCardLayout::FULL_CARD,
                      ContentSuggestionsAdditionalAction::FETCH,
                      /*show_if_empty=*/false, u"No suggestions message");
}

void MockContentSuggestionsProvider::SetDestructorCallback(
    DestructorCallback callback) {
  destructor_callback_ = std::move(callback);
}

void MockContentSuggestionsProvider::Fetch(const Category& category,
                                           const std::set<std::string>& set,
                                           FetchDoneCallback callback) {
  FetchMock(category, set, &callback);
}

void MockContentSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& id,
    ImageFetchedCallback callback) {
  FetchSuggestionImageMock(id, callback);
}

void MockContentSuggestionsProvider::FetchSuggestionImageData(
    const ContentSuggestion::ID& id,
    ImageDataFetchedCallback callback) {
  FetchSuggestionImageDataMock(id, &callback);
}

void MockContentSuggestionsProvider::FireSuggestionsChanged(
    Category category,
    std::vector<ContentSuggestion> suggestions) {
  observer()->OnNewSuggestions(this, category, std::move(suggestions));
}

void MockContentSuggestionsProvider::FireCategoryStatusChanged(
    Category category,
    CategoryStatus new_status) {
  statuses_[category.id()] = new_status;
  observer()->OnCategoryStatusChanged(this, category, new_status);
}

void MockContentSuggestionsProvider::FireCategoryStatusChangedWithCurrentStatus(
    Category category) {
  observer()->OnCategoryStatusChanged(this, category, statuses_[category.id()]);
}

void MockContentSuggestionsProvider::FireSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  observer()->OnSuggestionInvalidated(this, suggestion_id);
}

void MockContentSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    DismissedSuggestionsCallback callback) {
  GetDismissedSuggestionsForDebuggingMock(category, callback);
}

}  // namespace ntp_snippets
