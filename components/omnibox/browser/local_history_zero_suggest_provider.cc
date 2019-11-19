// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/local_history_zero_suggest_provider.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

namespace {

// Default relevance for the LocalHistoryZeroSuggestProvider query suggestions.
const int kLocalHistoryZeroSuggestRelevance = 500;

// Extracts the search terms from |url|. Collapses whitespaces, converts them to
// lowercase and returns them. |template_url_service| must not be null.
base::string16 GetSearchTermsFromURL(const GURL& url,
                                     TemplateURLService* template_url_service) {
  DCHECK(template_url_service);
  base::string16 search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);
  return base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
}

}  // namespace

// static
const char LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant[] =
    "Local";

// static
LocalHistoryZeroSuggestProvider* LocalHistoryZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new LocalHistoryZeroSuggestProvider(client, listener);
}

void LocalHistoryZeroSuggestProvider::Start(const AutocompleteInput& input,
                                            bool minimal_changes) {
  TRACE_EVENT0("omnibox", "LocalHistoryZeroSuggestProvider::Start");

  done_ = true;
  matches_.clear();

  // Allow local history query suggestions only when the user is not in an
  // off-the-record context.
  if (client_->IsOffTheRecord())
    return;

  // Allow local history query suggestions only when the omnibox is empty and is
  // focused from the NTP.
  if (!input.from_omnibox_focus() ||
      input.type() != metrics::OmniboxInputType::EMPTY ||
      !BaseSearchProvider::IsNTPPage(input.current_page_classification())) {
    return;
  }

  // Allow local history query suggestions only when the user has set up Google
  // as their default search engine.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider() ||
      template_url_service->GetDefaultSearchProvider()->GetEngineType(
          template_url_service->search_terms_data()) != SEARCH_ENGINE_GOOGLE) {
    return;
  }

  if (!base::Contains(OmniboxFieldTrial::GetZeroSuggestVariants(
                          input.current_page_classification()),
                      kZeroSuggestLocalVariant)) {
    return;
  }

  QueryURLDatabase(input);
}

void LocalHistoryZeroSuggestProvider::DeleteMatch(
    const AutocompleteMatch& match) {
  SCOPED_UMA_HISTOGRAM_TIMER("Omnibox.LocalHistoryZeroSuggest.SyncDeleteTime");

  history::HistoryService* history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return;

  // Deletes all the search terms matching the query suggestion.
  url_db->DeleteKeywordSearchTermForNormalizedTerm(
      template_url_service->GetDefaultSearchProvider()->id(), match.contents);

  // Generate a Google search URL. Note that the search URL returned by
  // TemplateURL::GenerateSearchURL() cannot be used here as it contains
  // Chrome specific query params and therefore only matches search queries
  // issued from Chrome and not those from the Web.
  GURL google_base_url(
      template_url_service->search_terms_data().GoogleBaseURLValue());
  std::string google_search_url =
      google_util::GetGoogleSearchURL(google_base_url).spec();

  // Query the HistoryService for fresh Google search URLs. Note that the
  // performance overhead of querying the HistoryService can be tolerated here
  // due to the small percentage of suggestions getting deleted relative to the
  // number of suggestions shown and the async nature of this lookup.
  history::QueryOptions opts;
  opts.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  opts.begin_time = history::AutocompleteAgeThreshold();
  history_service->QueryHistory(
      base::ASCIIToUTF16(google_search_url), opts,
      base::BindOnce(&LocalHistoryZeroSuggestProvider::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), match.contents,
                     base::TimeTicks::Now()),
      &history_task_tracker_);

  // Immediately update the list of matches to reflect the match was deleted.
  base::EraseIf(matches_, [&](const auto& item) {
    return match.contents == item.contents;
  });
}

LocalHistoryZeroSuggestProvider::LocalHistoryZeroSuggestProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY),
      max_matches_(AutocompleteResult::GetMaxMatches(/*is_zero_suggest=*/true)),
      client_(client),
      listener_(listener) {}

LocalHistoryZeroSuggestProvider::~LocalHistoryZeroSuggestProvider() {}

void LocalHistoryZeroSuggestProvider::QueryURLDatabase(
    const AutocompleteInput& input) {
  done_ = true;
  matches_.clear();

  history::HistoryService* const history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  // Fail if the in-memory URL database is not available.
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return;

  // Fail if we can't set the clickthrough URL for query suggestions.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // Request 5x more search terms than the number of matches the provider
  // intends to return hoping to have enough left once ineligible ones are
  // filtered out.
  const auto& results = url_db->GetMostRecentKeywordSearchTerms(
      template_url_service->GetDefaultSearchProvider()->id(), max_matches_ * 5);

  // Used to filter out duplicate query suggestions.
  std::set<base::string16> seen_suggestions_set;

  int relevance = kLocalHistoryZeroSuggestRelevance;
  size_t search_terms_seen_count = 0;
  for (const auto& result : results) {
    search_terms_seen_count++;
    // Discard the result if it is not fresh enough.
    if (result.time < history::AutocompleteAgeThreshold())
      continue;

    base::string16 search_terms = result.normalized_term;
    if (search_terms.empty())
      continue;

    // Filter out duplicate query suggestions.
    if (seen_suggestions_set.count(search_terms))
      continue;
    seen_suggestions_set.insert(search_terms);

    SearchSuggestionParser::SuggestResult suggestion(
        /*suggestion=*/search_terms, AutocompleteMatchType::SEARCH_HISTORY,
        /*subtype_identifier=*/0, /*from_keyword=*/false, relevance--,
        /*relevance_from_server=*/0,
        /*input_text=*/base::ASCIIToUTF16(std::string()));

    AutocompleteMatch match = BaseSearchProvider::CreateSearchSuggestion(
        this, input, /*in_keyword_mode=*/false, suggestion,
        template_url_service->GetDefaultSearchProvider(),
        template_url_service->search_terms_data(),
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
        /*append_extra_query_params_from_command_line*/ true);
    match.deletable = true;

    matches_.push_back(match);
    if (matches_.size() >= max_matches_)
      break;
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsSeenCount",
      search_terms_seen_count);
  UMA_HISTOGRAM_COUNTS_1000("Omnibox.LocalHistoryZeroSuggest.MaxMatchesCount",
                            max_matches_);

  listener_->OnProviderUpdate(true);
}

void LocalHistoryZeroSuggestProvider::OnHistoryQueryResults(
    const base::string16& suggestion,
    const base::TimeTicks& query_time,
    history::QueryResults results) {
  history::HistoryService* history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // Delete the matching URLs that would generate |suggestion|.
  std::vector<GURL> urls_to_delete;
  for (const auto& result : results) {
    base::string16 search_terms =
        GetSearchTermsFromURL(result.url(), template_url_service);
    if (search_terms == suggestion)
      urls_to_delete.push_back(result.url());
  }
  history_service->DeleteURLs(urls_to_delete);

  UMA_HISTOGRAM_TIMES("Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime",
                      base::TimeTicks::Now() - query_time);
}
