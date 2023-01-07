// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_FAKE_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_
#define COMPONENTS_NTP_SNIPPETS_FAKE_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_

#include <map>
#include <vector>

#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"

namespace ntp_snippets {

class FakeContentSuggestionsProviderObserver final
    : public ContentSuggestionsProvider::Observer {
 public:
  FakeContentSuggestionsProviderObserver();
  FakeContentSuggestionsProviderObserver(
      const FakeContentSuggestionsProviderObserver&) = delete;
  FakeContentSuggestionsProviderObserver& operator=(
      const FakeContentSuggestionsProviderObserver&) = delete;
  ~FakeContentSuggestionsProviderObserver();

  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override;

  void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                               Category category,
                               CategoryStatus new_status) override;

  void OnSuggestionInvalidated(
      ContentSuggestionsProvider* provider,
      const ContentSuggestion::ID& suggestion_id) override;

  const std::map<Category, CategoryStatus, Category::CompareByID>& statuses()
      const;

  CategoryStatus StatusForCategory(Category category) const;

  const std::vector<ContentSuggestion>& SuggestionsForCategory(
      Category category);

 private:
  std::map<Category, CategoryStatus, Category::CompareByID> statuses_;
  std::map<Category, std::vector<ContentSuggestion>, Category::CompareByID>
      suggestions_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FAKE_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_
