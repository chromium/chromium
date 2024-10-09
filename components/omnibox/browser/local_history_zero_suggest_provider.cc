// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/local_history_zero_suggest_provider.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "url/gurl.h"

using metrics::OmniboxInputType;

namespace {

// Extracts the search terms from |url|. Collapses whitespaces, converts them to
// lowercase and returns them. |template_url_service| must not be null.
std::u16string GetSearchTermsFromURL(const GURL& url,
                                     TemplateURLService* template_url_service) {
  DCHECK(template_url_service);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);

  std::u16string search_terms;
  default_provider->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);
  return base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
}

// Whether zero suggest suggestions are allowed in the given context.
// Invoked early, confirms all the conditions for zero suggestions are met.
bool AllowLocalHistoryZeroSuggestSuggestions(AutocompleteProviderClient* client,
                                             const AutocompleteInput& input) {
  // Allow local history zero-suggest only when the user is not in incognito
  // mode.
  if (client->IsOffTheRecord())
    return false;

  // Allow local history zero-suggest only when the user has set up Google as
  // their default search engine.
  if (!search::DefaultSearchProviderIsGoogle(client->GetTemplateURLService())) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          omnibox::kLocalHistoryZeroSuggestBeyondNTP)) {
    // Allow local history zero-suggest where remote zero-suggest is eligible.
    const auto [result_type, _] =
        ZeroSuggestProvider::GetResultTypeAndEligibility(client, input);
    return result_type != ZeroSuggestProvider::ResultType::kNone;
  }

  // Allow local history query suggestions only when the omnibox is empty and is
  // focused from the NTP.
  return input.focus_type() == metrics::OmniboxFocusType::INTERACTION_FOCUS &&
         input.type() == OmniboxInputType::EMPTY &&
         omnibox::IsNTPPage(input.current_page_classification());
}

}  // namespace

// static
LocalHistoryZeroSuggestProvider* LocalHistoryZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new LocalHistoryZeroSuggestProvider(client, listener);
}

void LocalHistoryZeroSuggestProvider::Start(const AutocompleteInput& input,
                                            bool minimal_changes) {
  TRACE_EVENT0("omnibox", "LocalHistoryZeroSuggestProvider::Start");
  Stop(true, false);

  if (!AllowLocalHistoryZeroSuggestSuggestions(client_, input)) {
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
  opts.begin_time = base::Time::Now() - base::Days(90);  // Full history length.
  history_service->QueryHistory(
      base::ASCIIToUTF16(google_search_url), opts,
      base::BindOnce(&LocalHistoryZeroSuggestProvider::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), match.contents,
                     base::TimeTicks::Now()),
      &history_task_tracker_);

  // Immediately update the list of matches to reflect the match was deleted.
  std::erase_if(matches_, [&](const auto& item) {
    return match.contents == item.contents;
  });
}

LocalHistoryZeroSuggestProvider::LocalHistoryZeroSuggestProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY),
      max_matches_(AutocompleteResult::GetMaxMatches(true)),
      client_(client) {
  AddListener(listener);
}

LocalHistoryZeroSuggestProvider::~LocalHistoryZeroSuggestProvider() = default;

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

  std::vector<std::unique_ptr<history::KeywordSearchTermVisit>> results;
  const base::ElapsedTimer db_query_timer;
  auto enumerator = url_db->CreateKeywordSearchTermVisitEnumerator(
      template_url_service->GetDefaultSearchProvider()->id());
  if (enumerator) {
    history::GetAutocompleteSearchTermsFromEnumerator(
        *enumerator, max_matches_, history::SearchTermRankingPolicy::kFrecency,
        &results);
  }
  DCHECK_LE(results.size(), max_matches_);
  base::UmaHistogramTimes(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTimeV2",
      db_query_timer.Elapsed());

  int relevance =
      OmniboxFieldTrial::kLocalHistoryZeroSuggestRelevanceScore.Get();
  for (const auto& result : results) {
    SearchSuggestionParser::SuggestResult suggestion(
        /*suggestion=*/result->normalized_term,
        AutocompleteMatchType::SEARCH_HISTORY,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME,
        /*subtypes=*/{}, /*from_keyword=*/false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE, relevance--,
        /*relevance_from_server=*/false,
        /*input_text=*/base::ASCIIToUTF16(std::string()));

    // If the appropriate header text and section are not provided by the server
    // the default omnibox::SECTION_PERSONALIZED_ZERO_SUGGEST will be used and
    // the local history zero-prefix suggestions will be shown without a header.
    suggestion.set_suggestion_group_id(
        omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST);

    AutocompleteMatch match = BaseSearchProvider::CreateSearchSuggestion(
        this, input, /*in_keyword_mode=*/false, suggestion,
        template_url_service->GetDefaultSearchProvider(),
        template_url_service->search_terms_data(),
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
        /*append_extra_query_params_from_command_line*/ true);
    match.deletable = client_->AllowDeletingBrowserHistory();
    matches_.push_back(match);
  }
}

void LocalHistoryZeroSuggestProvider::OnHistoryQueryResults(
    const std::u16string& suggestion,
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
    std::u16string search_terms =
        GetSearchTermsFromURL(result.url(), template_url_service);
    if (search_terms == suggestion)
      urls_to_delete.push_back(result.url());
  }
  history_service->DeleteURLs(urls_to_delete);

  UMA_HISTOGRAM_TIMES("Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime",
                      base::TimeTicks::Now() - query_time);
}
