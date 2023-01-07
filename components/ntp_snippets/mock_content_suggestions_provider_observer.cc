// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"

namespace ntp_snippets {

MockContentSuggestionsProviderObserver::
    MockContentSuggestionsProviderObserver() = default;

MockContentSuggestionsProviderObserver::
    ~MockContentSuggestionsProviderObserver() = default;

void MockContentSuggestionsProviderObserver::OnNewSuggestions(
    ContentSuggestionsProvider* provider,
    Category category,
    std::vector<ContentSuggestion> suggestions) {
  std::list<ContentSuggestion> suggestions_list;
  for (ContentSuggestion& suggestion : suggestions) {
    suggestions_list.push_back(std::move(suggestion));
  }
  OnNewSuggestions(provider, category, suggestions_list);
}

}  // namespace ntp_snippets
