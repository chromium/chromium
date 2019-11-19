// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTED_ARTICLES_OBSERVER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTED_ARTICLES_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"

namespace ntp_snippets {
class Category;
}

namespace offline_pages {

// Observes the ContentSuggestionsService, listening for new suggestions in the
// ARTICLES category.  When those suggestions arrive, it then forwards them to
// the Prefetch Service, which does not know about Content Suggestions
// specifically.
class SuggestedArticlesObserver
    : public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  SuggestedArticlesObserver();
  ~SuggestedArticlesObserver() override;

  void SetPrefetchService(PrefetchService* service);
  void SetContentSuggestionsServiceAndObserve(
      ntp_snippets::ContentSuggestionsService* service);

  // ContentSuggestionsService::Observer overrides.
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void OnFullRefreshRequired() override;
  void ContentSuggestionsServiceShutdown() override;

  // Starts prefetching current suggestions if available.
  void ConsumeSuggestions();

  // Returns a pointer to the list of testing articles. If there is no such
  // list, allocates one before returning the list.  The observer owns the list.
  std::vector<ntp_snippets::ContentSuggestion>* GetTestingArticles();

 private:
  bool GetCurrentSuggestions(std::vector<PrefetchURL>* result);

  // Unowned, only used when we are called by observer methods (so the
  // pointer will be valid).
  ntp_snippets::ContentSuggestionsService* content_suggestions_service_ =
      nullptr;

  // Unowned, owns |this|.
  PrefetchService* prefetch_service_;

  // Normally null, but can be set in tests to override the default behavior.
  std::unique_ptr<std::vector<ntp_snippets::ContentSuggestion>> test_articles_;

  ntp_snippets::CategoryStatus category_status_ =
      ntp_snippets::CategoryStatus::INITIALIZING;

  DISALLOW_COPY_AND_ASSIGN(SuggestedArticlesObserver);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTED_ARTICLES_OBSERVER_H_
