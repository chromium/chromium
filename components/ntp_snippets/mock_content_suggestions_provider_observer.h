// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_
#define COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_

#include <list>
#include <vector>

#include "components/ntp_snippets/content_suggestions_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp_snippets {

class MockContentSuggestionsProviderObserver
    : public ContentSuggestionsProvider::Observer {
 public:
  MockContentSuggestionsProviderObserver();
  ~MockContentSuggestionsProviderObserver();

  // Call of this function is redirected to the mock function OnNewSuggestions
  // which takes const list of suggestions. We do this trick so that the
  // MOCK_METHOD behaves the same way in tests as the actual method and we can
  // keep this gMock issue limited to the mock class. MOCK_METHOD cannot be
  // applied here directly, since gMock does not support movable-only types
  // such as ContentSuggestion.
  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override;

  MOCK_METHOD3(OnNewSuggestions,
               void(ContentSuggestionsProvider* provider,
                    Category category,
                    const std::list<ContentSuggestion>& suggestions));
  MOCK_METHOD3(OnCategoryStatusChanged,
               void(ContentSuggestionsProvider* provider,
                    Category category,
                    CategoryStatus new_status));
  MOCK_METHOD2(OnSuggestionInvalidated,
               void(ContentSuggestionsProvider* provider,
                    const ContentSuggestion::ID& suggestion_id));
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_OBSERVER_H_
