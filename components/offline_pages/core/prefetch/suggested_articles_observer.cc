// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

#include <unordered_set>

#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

using ntp_snippets::Category;
using ntp_snippets::ContentSuggestion;

namespace offline_pages {

namespace {

const ntp_snippets::Category& ArticlesCategory() {
  static ntp_snippets::Category articles =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  return articles;
}

}  // namespace

SuggestedArticlesObserver::SuggestedArticlesObserver() = default;
SuggestedArticlesObserver::~SuggestedArticlesObserver() {
  if (content_suggestions_service_)
    content_suggestions_service_->RemoveObserver(this);
}

void SuggestedArticlesObserver::SetPrefetchService(PrefetchService* service) {
  DCHECK(service);

  prefetch_service_ = service;
}

void SuggestedArticlesObserver::SetContentSuggestionsServiceAndObserve(
    ntp_snippets::ContentSuggestionsService* service) {
  DCHECK(service);

  content_suggestions_service_ = service;
  content_suggestions_service_->AddObserver(this);
}

bool SuggestedArticlesObserver::GetCurrentSuggestions(
    std::vector<PrefetchURL>* result) {
  DCHECK(result);

  std::vector<PrefetchURL> prefetch_urls;
  if (category_status_ != ntp_snippets::CategoryStatus::AVAILABLE)
    return false;

  const std::vector<ContentSuggestion>& suggestions =
      test_articles_ ? *test_articles_
                     : content_suggestions_service_->GetSuggestionsForCategory(
                           ArticlesCategory());
  for (const ContentSuggestion& suggestion : suggestions) {
    prefetch_urls.push_back({suggestion.id().id_within_category(),
                             suggestion.url(), suggestion.title()});
  }

  *result = prefetch_urls;
  return true;
}

void SuggestedArticlesObserver::OnNewSuggestions(Category category) {
  // TODO(dewittj): Change this to check whether a given category is not
  // a _remote_ category.
  if (category != ArticlesCategory())
    return;

  ConsumeSuggestions();
}

void SuggestedArticlesObserver::OnCategoryStatusChanged(
    Category category,
    ntp_snippets::CategoryStatus new_status) {
  if (category != ArticlesCategory() || category_status_ == new_status)
    return;

  category_status_ = new_status;

  if (category_status_ ==
          ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED ||
      category_status_ ==
          ntp_snippets::CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED) {
    prefetch_service_->GetPrefetchDispatcher()
        ->RemoveAllUnprocessedPrefetchURLs(kSuggestedArticlesNamespace);
  }
}

void SuggestedArticlesObserver::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(dewittj): Change this to check whether a given category is not
  // a _remote_ category.
  if (suggestion_id.category() != ArticlesCategory())
    return;

  prefetch_service_->GetPrefetchDispatcher()->RemovePrefetchURLsByClientId(
      ClientId(kSuggestedArticlesNamespace,
               suggestion_id.id_within_category()));
}

void SuggestedArticlesObserver::OnFullRefreshRequired() {
  prefetch_service_->GetPrefetchDispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  OnNewSuggestions(ArticlesCategory());
}

void SuggestedArticlesObserver::ContentSuggestionsServiceShutdown() {
  // No need to do anything here, we will just stop getting events.
}

void SuggestedArticlesObserver::ConsumeSuggestions() {
  std::vector<PrefetchURL> prefetch_urls;
  if (!GetCurrentSuggestions(&prefetch_urls))
    return;

  prefetch_service_->GetPrefetchDispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, prefetch_urls);
}

std::vector<ntp_snippets::ContentSuggestion>*
SuggestedArticlesObserver::GetTestingArticles() {
  if (!test_articles_) {
    test_articles_ =
        std::make_unique<std::vector<ntp_snippets::ContentSuggestion>>();
  }
  return test_articles_.get();
}

}  // namespace offline_pages
