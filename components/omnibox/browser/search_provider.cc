// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_scoring_signals_annotator.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"
#include "url/url_util.h"

using metrics::OmniboxEventProto;

// Helpers --------------------------------------------------------------------

namespace {

// Increments the appropriate event in the histogram by one.
void LogOmniboxSuggestRequest(RemoteRequestEvent request_event) {
  base::UmaHistogramEnumeration("Omnibox.SearchSuggest.Requests",
                                request_event);
}

bool HasMultipleWords(const std::u16string& text) {
  base::i18n::BreakIterator i(text, base::i18n::BreakIterator::BREAK_WORD);
  bool found_word = false;
  if (i.Init()) {
    while (i.Advance()) {
      if (i.IsWord()) {
        if (found_word)
          return true;
        found_word = true;
      }
    }
  }
  return false;
}

}  // namespace

// SearchProvider::Providers --------------------------------------------------

SearchProvider::Providers::Providers(TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {}

const TemplateURL* SearchProvider::Providers::GetDefaultProviderURL() const {
  if (default_provider_.empty())
    return nullptr;
  DCHECK(template_url_service_);
  return template_url_service_->GetTemplateURLForKeyword(default_provider_);
}

const TemplateURL* SearchProvider::Providers::GetKeywordProviderURL() const {
  if (keyword_provider_.empty())
    return nullptr;
  DCHECK(template_url_service_);
  return template_url_service_->GetTemplateURLForKeyword(keyword_provider_);
}

// SearchProvider::CompareScoredResults ---------------------------------------

class SearchProvider::CompareScoredResults {
 public:
  bool operator()(const SearchSuggestionParser::Result& a,
                  const SearchSuggestionParser::Result& b) {
    // Sort in descending relevance order.
    return a.relevance() > b.relevance();
  }
};

// SearchProvider -------------------------------------------------------------

SearchProvider::SearchProvider(AutocompleteProviderClient* client,
                               AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_SEARCH, client),
      providers_(client->GetTemplateURLService()),
      answers_cache_(10) {
  AddListener(listener);

  TemplateURLService* template_url_service = client->GetTemplateURLService();

  // |template_url_service| can be null in tests.
  if (template_url_service)
    observation_.Observe(template_url_service);
}

void SearchProvider::RegisterDisplayedAnswers(
    const AutocompleteResult& result) {
  if (result.empty())
    return;

  // The answer must be in the first or second slot to be considered. It should
  // only be in the second slot if AutocompleteController ranked a local search
  // history or a verbatim item higher than the answer.
  auto match = result.begin();
  if (match->answer_type == omnibox::ANSWER_TYPE_UNSPECIFIED &&
      result.size() > 1) {
    ++match;
  }

  if (match->answer_type == omnibox::ANSWER_TYPE_UNSPECIFIED ||
      match->fill_into_edit.empty()) {
    return;
  }

  // Valid answer encountered, cache it for further queries.
  answers_cache_.UpdateRecentAnswers(match->fill_into_edit, match->answer_type);
}

// static
int SearchProvider::CalculateRelevanceForKeywordVerbatim(
    metrics::OmniboxInputType type,
    bool allow_exact_keyword_match,
    bool prefer_keyword) {
  // This function is responsible for scoring verbatim query matches
  // for non-extension substituting keywords.
  // KeywordProvider::CalculateRelevance() scores all other types of
  // keyword verbatim matches.
  if (allow_exact_keyword_match && prefer_keyword)
    return 1500;
  return (allow_exact_keyword_match &&
          (type == metrics::OmniboxInputType::QUERY))
             ? 1450
             : 1100;
}

bool SearchProvider::CanSendCurrentPageURLInRequest(
    const GURL& current_page_url,
    metrics::OmniboxEventProto::PageClassification page_classification,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    const AutocompleteProviderClient* client) {
  // Send the current page URL if the request eligiblility and the user settings
  // requirements are met and the URL is valid with an HTTP(S) scheme.
  // Don't bother sending the URL of an NTP page; it's not useful. The server
  // already gets equivalent information in the form of the current page
  // classification.
  return !omnibox::IsNTPPage(page_classification) &&
         PageURLIsEligibleForSuggestRequest(current_page_url) &&
         CanSendSuggestRequestWithPageURL(current_page_url, template_url,
                                          search_terms_data, client);
}

SearchProvider::~SearchProvider() = default;

// static
void SearchProvider::UpdateOldResults(
    bool minimal_changes,
    SearchSuggestionParser::Results* results) {
  // When called without |minimal_changes|, it likely means the user has
  // pressed a key.  Revise the cached results appropriately.
  if (!minimal_changes) {
    for (auto sug_it = results->suggest_results.begin();
         sug_it != results->suggest_results.end();) {
      if (sug_it->type() == AutocompleteMatchType::CALCULATOR) {
        sug_it = results->suggest_results.erase(sug_it);
      } else {
        sug_it->set_received_after_last_keystroke(false);
        ++sug_it;
      }
    }
    for (auto& navigation_result : results->navigation_results)
      navigation_result.set_received_after_last_keystroke(false);
  }
}

void SearchProvider::Start(const AutocompleteInput& input,
                           bool minimal_changes) {
  TRACE_EVENT0("omnibox", "SearchProvider::Start");
  // Do our best to load the model as early as possible.  This will reduce
  // odds of having the model not ready when really needed (a non-empty input).
  TemplateURLService* model = client()->GetTemplateURLService();
  DCHECK(model);
  model->Load();

  matches_.clear();

  // At this point, we could exit early if the input is on-focus or empty,
  // because offering suggestions in those scenarios is handled by
  // ZeroSuggestProvider. But we continue here anyway in order to send a request
  // to warm up the suggest server. It's possible this warmup request could be
  // combined or deduped with the request from ZeroSuggestProvider but that
  // provider doesn't always run, based on a variety of factors (sign in state,
  // experiments, input type (on-focus vs. on-clobber)). Ensuring that we always
  // send a request here allows the suggest server to, for example, load
  // per-user models into memory.  Having a per-user model in memory allows the
  // suggest server to respond more quickly with personalized suggestions as the
  // user types.
  //
  // 2024-01 Adding a feature flag for experiment to ablate the warmup request.
  if (base::FeatureList::IsEnabled(omnibox::kAblateSearchProviderWarmup) &&
      (input.IsZeroSuggest() ||
       input.type() == metrics::OmniboxInputType::EMPTY)) {
    Stop(true, false);
    return;
  }

  keyword_input_ = input;
  const TemplateURL* keyword_provider =
      KeywordProvider::GetSubstitutingTemplateURLForInput(model,
                                                          &keyword_input_);
  if (keyword_provider == nullptr)
    keyword_input_.Clear();
  else if (keyword_input_.text().empty())
    keyword_provider = nullptr;

  const TemplateURL* default_provider = model->GetDefaultSearchProvider();
  if (default_provider &&
      !default_provider->SupportsReplacement(model->search_terms_data()))
    default_provider = nullptr;

  if (keyword_provider == default_provider)
    default_provider = nullptr;  // No use in querying the same provider twice.

  if (!default_provider && !keyword_provider) {
    // No valid providers.
    Stop(true, false);
    return;
  }

  // If we're still running an old query but have since changed the query text
  // or the providers, abort the query.
  std::u16string default_provider_keyword(
      default_provider ? default_provider->keyword() : std::u16string());
  std::u16string keyword_provider_keyword(
      keyword_provider ? keyword_provider->keyword() : std::u16string());
  if (!minimal_changes ||
      !providers_.equal(default_provider_keyword, keyword_provider_keyword)) {
    // Cancel any in-flight suggest requests.
    if (!done_)
      Stop(false, false);
  }

  providers_.set(default_provider_keyword, keyword_provider_keyword);

  if (input.IsZeroSuggest()) {
    // Don't display any suggestions for on-focus requests.
    ClearAllResults();
  } else if (input.text().empty()) {
    // User typed "?" alone.  Give them a placeholder result indicating what
    // this syntax does.
    if (default_provider) {
      AutocompleteMatch match;
      match.provider = this;
      match.contents.assign(l10n_util::GetStringUTF16(IDS_EMPTY_KEYWORD_VALUE));
      match.contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::NONE));
      match.keyword = providers_.default_provider();
      match.allowed_to_be_default_match = true;
      matches_.push_back(match);
    }
    Stop(true, false);
    return;
  }

  input_ = input;

  // Don't search the history database for on-focus inputs or lens searchboxes.
  // On-focus inputs should only be used to warm up the suggest server; and Lens
  // searchboxes do not show suggestions from the history database.
  if (!input.IsZeroSuggest() &&
      !omnibox::IsLensSearchbox(input_.current_page_classification())) {
    DoHistoryQuery(minimal_changes);
    // Answers needs scored history results before any suggest query has been
    // started, since the query for answer-bearing results needs additional
    // prefetch information based on the highest-scored local history result.
    ScoreHistoryResults(raw_default_history_results_, false,
                        &transformed_default_history_results_);
    ScoreHistoryResults(raw_keyword_history_results_, true,
                        &transformed_keyword_history_results_);
    prefetch_data_ = FindAnswersPrefetchData();

    // Raw results are not needed any more.
    raw_default_history_results_.clear();
    raw_keyword_history_results_.clear();
  }

  StartOrStopSuggestQuery(minimal_changes);
  UpdateMatches();
}

void SearchProvider::Stop(bool clear_cached_results,
                          bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  StopSuggest();
  if (clear_cached_results)
    ClearAllResults();
}

bool SearchProvider::ShouldAppendExtraParams(
    const SearchSuggestionParser::SuggestResult& result) const {
  return !result.from_keyword() || providers_.default_provider().empty();
}

void SearchProvider::RecordDeletionResult(bool success) {
  if (success) {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ServerSuggestDelete.Success"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ServerSuggestDelete.Failure"));
  }
}

void SearchProvider::OnTemplateURLServiceChanged() {
  // Only update matches at this time if we haven't already claimed we're done
  // processing the query.
  if (done_)
    return;

  // Check that the engines we're using weren't renamed or deleted.  (In short,
  // require that an engine still exists with the keywords in use.)  For each
  // deleted engine, cancel the in-flight request if any, drop its suggestions,
  // and, in the case when the default provider was affected, point the cached
  // default provider keyword name at the new name for the default provider.

  // Get...ProviderURL() looks up the provider using the cached keyword name
  // stored in |providers_|.
  const TemplateURL* template_url = providers_.GetDefaultProviderURL();
  if (!template_url) {
    CancelLoader(&default_loader_);
    default_results_.Clear();

    std::u16string default_provider;
    const TemplateURL* default_provider_template_url =
        client()->GetTemplateURLService()->GetDefaultSearchProvider();
    if (default_provider_template_url)
      default_provider = default_provider_template_url->keyword();

    providers_.set(default_provider, providers_.keyword_provider());
  }
  template_url = providers_.GetKeywordProviderURL();
  if (!providers_.keyword_provider().empty() && !template_url) {
    CancelLoader(&keyword_loader_);
    keyword_results_.Clear();
    providers_.set(providers_.default_provider(), std::u16string());
  }
  // It's possible the template URL changed without changing associated keyword.
  // Hence, it's always necessary to update matches to use the new template
  // URL.  (One could cache the template URL and only call UpdateMatches() and
  // NotifyListeners() if a keyword was deleted/renamed or the template URL was
  // changed.  That would save extra calls to these functions.  However, this is
  // uncommon and not likely to be worth the extra work.)
  UpdateMatches();
  NotifyListeners(true);  // always pretend something changed
}

const TemplateURL* SearchProvider::GetTemplateURL(bool is_keyword) const {
  return is_keyword ? providers_.GetKeywordProviderURL()
                    : providers_.GetDefaultProviderURL();
}

const AutocompleteInput SearchProvider::GetInput(bool is_keyword) const {
  return is_keyword ? keyword_input_ : input_;
}

void SearchProvider::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body) {
  TRACE_EVENT0("omnibox", "SearchProvider::OnURLLoadComplete");
  DCHECK(!done_);
  const bool is_keyword = source == keyword_loader_.get();

  // Ensure the request succeeded and that the provider used is still available.
  // A verbatim match cannot be generated without this provider, causing errors.
  const bool request_succeeded =
      response_code == 200 && GetTemplateURL(is_keyword);

  LogLoadComplete(request_succeeded, is_keyword);

  bool results_updated = false;
  // Ignore (i.e., don't display) any suggestions for on-focus inputs.
  // SearchProvider is not intended to give suggestions on on-focus inputs;
  // that's left to ZeroSuggestProvider and friends.  Furthermore, it's not
  // clear if the suggest server will send back sensible results to the
  // request we're constructing here for on-focus inputs.
  if (!input_.IsZeroSuggest() && request_succeeded) {
    std::optional<base::Value::List> data =
        SearchSuggestionParser::DeserializeJsonData(
            SearchSuggestionParser::ExtractJsonData(source,
                                                    std::move(response_body)));
    if (data) {
      SearchSuggestionParser::Results* results =
          is_keyword ? &keyword_results_ : &default_results_;
      results_updated = SearchSuggestionParser::ParseSuggestResults(
          *data, GetInput(is_keyword), client()->GetSchemeClassifier(),
          /*default_result_relevance=*/-1, /*is_keyword_result=*/is_keyword,
          results);
      if (results_updated) {
        if (results->field_trial_triggered) {
          client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
              metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE);
        }
        SortResults(is_keyword, results);
        PrefetchImages(results);
      }
    }
  }

  // Delete the loader now that we're done with it.
  if (is_keyword)
    keyword_loader_.reset();
  else
    default_loader_.reset();

  // Update matches, done status, etc., and send alerts if necessary.
  UpdateMatches();
  if (done_ || results_updated)
    NotifyListeners(results_updated);
}

void SearchProvider::StopSuggest() {
  CancelLoader(&default_loader_);
  CancelLoader(&keyword_loader_);
  timer_.Stop();
}

void SearchProvider::ClearAllResults() {
  keyword_results_.Clear();
  default_results_.Clear();
}

void SearchProvider::UpdateMatchContentsClass(
    const std::u16string& input_text,
    SearchSuggestionParser::Results* results) {
  std::u16string trimmed_input = base::CollapseWhitespace(input_text, false);
  if (base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions)) {
    trimmed_input = base::i18n::ToLower(trimmed_input);
  }
  for (auto& suggest_result : results->suggest_results)
    suggest_result.ClassifyMatchContents(false, trimmed_input);
  for (auto& navigation_result : results->navigation_results)
    navigation_result.CalculateAndClassifyMatchContents(false, trimmed_input);
}

void SearchProvider::SortResults(bool is_keyword,
                                 SearchSuggestionParser::Results* results) {
  // Ignore suggested scores for non-keyword matches in keyword mode; if the
  // server is allowed to score these, it could interfere with the user's
  // ability to get good keyword results.
  const bool abandon_suggested_scores =
      !is_keyword && !providers_.keyword_provider().empty();
  // Apply calculated relevance scores to suggestions if valid relevances were
  // not provided or we're abandoning suggested scores entirely.
  if (!results->relevances_from_server || abandon_suggested_scores) {
    ApplyCalculatedSuggestRelevance(&results->suggest_results);
    ApplyCalculatedNavigationRelevance(&results->navigation_results);
    // If abandoning scores entirely, also abandon the verbatim score.
    if (abandon_suggested_scores)
      results->verbatim_relevance = -1;
  }

  // Keep the result lists sorted.
  const CompareScoredResults comparator = CompareScoredResults();
  std::stable_sort(results->suggest_results.begin(),
                   results->suggest_results.end(), comparator);
  std::stable_sort(results->navigation_results.begin(),
                   results->navigation_results.end(), comparator);
}

void SearchProvider::LogLoadComplete(bool success, bool is_keyword) {
  LogOmniboxSuggestRequest(RemoteRequestEvent::kResponseReceived);
  // Record response time for suggest requests sent to Google.  We care
  // only about the common case: the Google default provider used in
  // non-keyword mode.
  if (!is_keyword &&
      search::TemplateURLIsGoogle(
          providers_.GetDefaultProviderURL(),
          client()->GetTemplateURLService()->search_terms_data())) {
    const base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - time_suggest_request_sent_;
    if (success) {
      base::UmaHistogramTimes(
          "Omnibox.SuggestRequest.Success.GoogleResponseTime", elapsed_time);
    } else {
      base::UmaHistogramTimes(
          "Omnibox.SuggestRequest.Failure.GoogleResponseTime", elapsed_time);
    }
  }
}

void SearchProvider::UpdateMatches() {
  // On-focus inputs display no suggestions, so we do not need to persist the
  // previous top suggestions, add new suggestions, or revise suggestions to
  // enforce constraints about inlinability in this case.  Indeed, most of
  // these steps would be bad, as they'd add a suggestion of some form, thus
  // opening the dropdown (which we do not want to happen).
  if (!input_.IsZeroSuggest()) {
    PersistTopSuggestions(&default_results_);
    PersistTopSuggestions(&keyword_results_);
    ConvertResultsToAutocompleteMatches();
    EnforceConstraints();
    RecordTopSuggestion();
  }

  UpdateDone();
}

void SearchProvider::EnforceConstraints() {
  if (!matches_.empty() && (default_results_.HasServerProvidedScores() ||
                            keyword_results_.HasServerProvidedScores())) {
    // These blocks attempt to repair undesirable behavior by suggested
    // relevances with minimal impact, preserving other suggested relevances.
    const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
    const bool is_extension_keyword =
        (keyword_url != nullptr) &&
        (keyword_url->type() == TemplateURL::OMNIBOX_API_EXTENSION);
    if ((keyword_url != nullptr) && !is_extension_keyword &&
        (AutocompleteResult::FindTopMatch(input_, matches_) ==
         matches_.end())) {
      // In non-extension keyword mode, disregard the keyword verbatim suggested
      // relevance if necessary, so at least one match is allowed to be default.
      // (In extension keyword mode this is not necessary because the extension
      // will return a default match.)  Give keyword verbatim the lowest
      // non-zero score to best reflect what the server desired.
      DCHECK_EQ(0, keyword_results_.verbatim_relevance);
      keyword_results_.verbatim_relevance = 1;
      ConvertResultsToAutocompleteMatches();
    }
    if (IsTopMatchSearchWithURLInput()) {
      // Disregard the suggested search and verbatim relevances if the input
      // type is URL and the top match is a highly-ranked search suggestion.
      // For example, prevent a search for "foo.com" from outranking another
      // provider's navigation for "foo.com" or "foo.com/url_from_history".
      ApplyCalculatedSuggestRelevance(&keyword_results_.suggest_results);
      ApplyCalculatedSuggestRelevance(&default_results_.suggest_results);
      default_results_.verbatim_relevance = -1;
      keyword_results_.verbatim_relevance = -1;
      ConvertResultsToAutocompleteMatches();
    }
    if (!is_extension_keyword && (AutocompleteResult::FindTopMatch(
                                      input_, matches_) == matches_.end())) {
      // Guarantee that SearchProvider returns a legal default match (except
      // when in extension-based keyword mode).  The omnibox always needs at
      // least one legal default match, and it relies on SearchProvider in
      // combination with KeywordProvider (for extension-based keywords) to
      // always return one.  Give the verbatim suggestion the lowest non-zero
      // scores to best reflect what the server desired.
      DCHECK_EQ(0, default_results_.verbatim_relevance);
      default_results_.verbatim_relevance = 1;
      // We do not have to alter keyword_results_.verbatim_relevance here.
      // If the user is in keyword mode, we already reverted (earlier in this
      // function) the instructions to suppress keyword verbatim.
      ConvertResultsToAutocompleteMatches();
    }
    DCHECK(!IsTopMatchSearchWithURLInput());
    DCHECK(is_extension_keyword || (AutocompleteResult::FindTopMatch(
                                        input_, matches_) != matches_.end()));
  }
}

void SearchProvider::RecordTopSuggestion() {
  top_query_suggestion_fill_into_edit_ = std::u16string();
  top_navigation_suggestion_ = GURL();
  auto first_match = AutocompleteResult::FindTopMatch(input_, matches_);
  if (first_match != matches_.end()) {
    // Identify if this match came from a query suggestion or a navsuggestion.
    // In either case, extracts the identifying feature of the suggestion
    // (query string or navigation url).
    if (AutocompleteMatch::IsSearchType(first_match->type))
      top_query_suggestion_fill_into_edit_ = first_match->fill_into_edit;
    else
      top_navigation_suggestion_ = first_match->destination_url;
  }
}

void SearchProvider::Run(bool query_is_private) {
  // Start a new request with the current input.
  time_suggest_request_sent_ = base::TimeTicks::Now();

  if (!query_is_private) {
    default_loader_ =
        CreateSuggestLoader(providers_.GetDefaultProviderURL(), input_);
  }
  keyword_loader_ =
      CreateSuggestLoader(providers_.GetKeywordProviderURL(), keyword_input_);

  // Both the above can fail if the providers have been modified or deleted
  // since the query began.
  if (!default_loader_ && !keyword_loader_) {
    UpdateDone();
    // We only need to update the listener if we're actually done.
    if (done_)
      NotifyListeners(false);
  } else {
    // Sent at least one request.
    time_suggest_request_sent_ = base::TimeTicks::Now();
  }
}

void SearchProvider::DoHistoryQuery(bool minimal_changes) {
  // The history query results are synchronous, so if minimal_changes is true,
  // we still have the last results and don't need to do anything.
  if (minimal_changes)
    return;

  raw_keyword_history_results_.clear();
  raw_default_history_results_.clear();

  history::URLDatabase* url_db = client()->GetInMemoryDatabase();
  if (!url_db)
    return;

  // Request history for both the keyword and default provider.  We grab many
  // more matches than we'll ultimately clamp to so that if there are several
  // recent multi-word matches who scores are lowered (see
  // ScoreHistoryResults()), they won't crowd out older, higher-scoring
  // matches.  Note that this doesn't fix the problem entirely, but merely
  // limits it to cases with a very large number of such multi-word matches; for
  // now, this seems OK compared with the complexity of a real fix, which would
  // require multiple searches and tracking of "single- vs. multi-word" in the
  // database.
  size_t num_matches = provider_max_matches_ * 5;
  const base::ElapsedTimer db_query_timer;
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  if (default_url) {
    auto enumerator = url_db->CreateKeywordSearchTermVisitEnumerator(
        default_url->id(), input_.text());
    if (enumerator) {
      history::GetAutocompleteSearchTermsFromEnumerator(
          *enumerator, num_matches, history::SearchTermRankingPolicy::kRecency,
          &raw_default_history_results_);
    }
    DCHECK_LE(raw_default_history_results_.size(), num_matches);
  }
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  if (keyword_url) {
    auto enumerator = url_db->CreateKeywordSearchTermVisitEnumerator(
        keyword_url->id(), keyword_input_.text());
    if (enumerator) {
      history::GetAutocompleteSearchTermsFromEnumerator(
          *enumerator, num_matches, history::SearchTermRankingPolicy::kRecency,
          &raw_keyword_history_results_);
    }
    DCHECK_LE(raw_keyword_history_results_.size(), num_matches);
  }
  base::UmaHistogramTimes(
      "Omnibox.LocalHistoryPrefixSuggest.SearchTermsExtractionTimeV2",
      db_query_timer.Elapsed());
}

base::TimeDelta SearchProvider::GetSuggestQueryDelay() const {
  // TODO(manukh): Reuse AutocompleteProviderDebouncer which duplicates all
  //  this logic and would avoid polling field trial params repeatedly.
  bool from_last_keystroke;
  int polling_delay_ms;
  OmniboxFieldTrial::GetSuggestPollingStrategy(&from_last_keystroke,
                                               &polling_delay_ms);

  base::TimeDelta delay(base::Milliseconds(polling_delay_ms));
  if (from_last_keystroke)
    return delay;

  base::TimeDelta time_since_last_suggest_request =
      base::TimeTicks::Now() - time_suggest_request_sent_;
  return std::max(base::TimeDelta(), delay - time_since_last_suggest_request);
}

void SearchProvider::StartOrStopSuggestQuery(bool minimal_changes) {
  bool query_is_private;
  if (!IsQuerySuitableForSuggest(&query_is_private)) {
    StopSuggest();
    ClearAllResults();
    return;
  }

  // For the minimal_changes case, if we finished the previous query and still
  // have its results, or are allowed to keep running it, just do that, rather
  // than starting a new query.
  if (minimal_changes && (!default_results_.suggest_results.empty() ||
                          !default_results_.navigation_results.empty() ||
                          !keyword_results_.suggest_results.empty() ||
                          !keyword_results_.navigation_results.empty() ||
                          (!done_ && !input_.omit_asynchronous_matches())))
    return;

  // We can't keep running any previous query, so halt it.
  StopSuggest();

  UpdateAllOldResults(minimal_changes);

  // Update the content classifications of remaining results so they look good
  // against the current input.
  UpdateMatchContentsClass(input_.text(), &default_results_);
  if (!keyword_input_.text().empty())
    UpdateMatchContentsClass(keyword_input_.text(), &keyword_results_);

  // We can't start a new query if we're only allowed synchronous results.
  if (input_.omit_asynchronous_matches())
    return;

  // Kick off a timer that will start the URL fetch if it completes before
  // the user types another character.  Requests may be delayed to avoid
  // flooding the server with requests that are likely to be thrown away later
  // anyway.
  const base::TimeDelta delay = GetSuggestQueryDelay();
  if (delay <= base::TimeDelta()) {
    Run(query_is_private);
    return;
  }
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&SearchProvider::Run, base::Unretained(this),
                              query_is_private));
}

void SearchProvider::CancelLoader(
    std::unique_ptr<network::SimpleURLLoader>* loader) {
  if (*loader) {
    LogOmniboxSuggestRequest(RemoteRequestEvent::kRequestInvalidated);
    loader->reset();
  }
}

bool SearchProvider::IsQuerySuitableForSuggest(bool* query_is_private) const {
  *query_is_private = IsQueryPotentiallyPrivate();

  // Don't run Suggest in incognito mode, if the engine doesn't support it, or
  // if the user has disabled it.  Also don't send potentially private data
  // to the default search provider.  (It's always okay to send explicit
  // keyword input to a keyword suggest server, if any.)
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  return !client()->IsOffTheRecord() && client()->SearchSuggestEnabled() &&
         ((default_url && !default_url->suggestions_url().empty() &&
           !*query_is_private) ||
          (keyword_url && !keyword_url->suggestions_url().empty()));
}

bool SearchProvider::IsQueryPotentiallyPrivate() const {
  if (input_.text().empty())
    return false;

  // Check the scheme.  If this is UNKNOWN/URL with a scheme that isn't
  // http/https/ftp, we shouldn't send it.  Sending things like file: and data:
  // is both a waste of time and a disclosure of potentially private, local
  // data.  Other "schemes" may actually be usernames, and we don't want to send
  // passwords.  If the scheme is OK, we still need to check other cases below.
  // If this is QUERY, then the presence of these schemes means the user
  // explicitly typed one, and thus this is probably a URL that's being entered
  // and happens to currently be invalid -- in which case we again want to run
  // our checks below.  Other QUERY cases are less likely to be URLs and thus we
  // assume we're OK.
  if (!base::EqualsCaseInsensitiveASCII(input_.scheme(), url::kHttpScheme) &&
      !base::EqualsCaseInsensitiveASCII(input_.scheme(), url::kHttpsScheme) &&
      !base::EqualsCaseInsensitiveASCII(input_.scheme(), url::kFtpScheme))
    return (input_.type() != metrics::OmniboxInputType::QUERY);

  // Don't send URLs with usernames, queries or refs.  Some of these are
  // private, and the Suggest server is unlikely to have any useful results
  // for any of them.  Also don't send URLs with ports, as we may initially
  // think that a username + password is a host + port (and we don't want to
  // send usernames/passwords), and even if the port really is a port, the
  // server is once again unlikely to have and useful results.
  // Note that we only block based on refs if the input is URL-typed, as search
  // queries can legitimately have #s in them which the URL parser
  // overaggressively categorizes as a url with a ref.
  const url::Parsed& parts = input_.parts();
  if (parts.username.is_nonempty() || parts.port.is_nonempty() ||
      parts.query.is_nonempty() ||
      (parts.ref.is_nonempty() &&
       (input_.type() == metrics::OmniboxInputType::URL)))
    return true;

  // Don't send anything for https except the hostname.  Hostnames are OK
  // because they are visible when the TCP connection is established, but the
  // specific path may reveal private information.
  if (base::EqualsCaseInsensitiveASCII(input_.scheme(), url::kHttpsScheme) &&
      parts.path.is_nonempty())
    return true;

  return false;
}

void SearchProvider::UpdateAllOldResults(bool minimal_changes) {
  if (keyword_input_.text().empty()) {
    // User is either in keyword mode with a blank input or out of
    // keyword mode entirely.
    keyword_results_.Clear();
  }
  UpdateOldResults(minimal_changes, &default_results_);
  UpdateOldResults(minimal_changes, &keyword_results_);
}

void SearchProvider::PersistTopSuggestions(
    SearchSuggestionParser::Results* results) {
  // Mark any results matching the current top results as having been received
  // prior to the last keystroke.  That prevents asynchronous updates from
  // clobbering top results, which may be used for inline autocompletion.
  // Other results don't need similar changes, because they shouldn't be
  // displayed asynchronously anyway.
  if (!top_query_suggestion_fill_into_edit_.empty()) {
    for (auto& suggest_result : results->suggest_results) {
      if (GetFillIntoEdit(suggest_result, providers_.GetKeywordProviderURL()) ==
          top_query_suggestion_fill_into_edit_) {
        suggest_result.set_received_after_last_keystroke(false);
      }
    }
  }
  if (top_navigation_suggestion_.is_valid()) {
    for (auto& navigation_result : results->navigation_results) {
      if (navigation_result.url() == top_navigation_suggestion_)
        navigation_result.set_received_after_last_keystroke(false);
    }
  }
}

void SearchProvider::ApplyCalculatedSuggestRelevance(
    SearchSuggestionParser::SuggestResults* list) {
  for (size_t i = 0; i < list->size(); ++i) {
    SearchSuggestionParser::SuggestResult& result = (*list)[i];
    result.set_relevance(
        result.CalculateRelevance(input_, providers_.has_keyword_provider()) +
        (list->size() - i - 1));
    result.set_relevance_from_server(false);
  }
}

void SearchProvider::ApplyCalculatedNavigationRelevance(
    SearchSuggestionParser::NavigationResults* list) {
  for (size_t i = 0; i < list->size(); ++i) {
    SearchSuggestionParser::NavigationResult& result = (*list)[i];
    result.set_relevance(
        result.CalculateRelevance(input_, providers_.has_keyword_provider()) +
        (list->size() - i - 1));
    result.set_relevance_from_server(false);
  }
}

std::unique_ptr<network::SimpleURLLoader> SearchProvider::CreateSuggestLoader(
    const TemplateURL* template_url,
    const AutocompleteInput& input) {
  if (!template_url || template_url->suggestions_url().empty())
    return nullptr;

  // Setting SuggestUrl the same as SearchUrl is a typical misconfiguration.
  // It's not possible for a URL to both provide a search results page and
  // suggested queries response (at least they have different format).  Most
  // like the user set the search URL correctly; it would be obvious if they did
  // not. Thus, it's likely that the suggest URL is wrong.  Because it would not
  // give a valid query suggestion response, don't bother sending queries to it
  // (otherwise user will quickly hit rate-limit for search queries, that will
  // harm valid search queries as well).
  if (template_url->suggestions_url() == template_url->url())
    return nullptr;

  // Bail if the suggestion URL is invalid with the given replacements.
  TemplateURLRef::SearchTermsArgs search_term_args(input.text());
  search_term_args.input_type = input.type();
  search_term_args.cursor_position = input.cursor_position();
  search_term_args.page_classification = input.current_page_classification();
  search_term_args.request_source = input.request_source();
  // Session token and prefetch data required for answers.
  search_term_args.session_token =
      client()->GetTemplateURLService()->GetSessionToken();
  if (!prefetch_data_.full_query_text.empty()) {
    search_term_args.prefetch_query =
        base::UTF16ToUTF8(prefetch_data_.full_query_text);
    search_term_args.prefetch_query_type =
        base::NumberToString(prefetch_data_.query_type);
  }
  // Make sure the Lens suggest inputs are sent in the request, if
  // available.
  search_term_args.lens_overlay_suggest_inputs =
      input.lens_overlay_suggest_inputs();

  const SearchTermsData& search_terms_data =
      client()->GetTemplateURLService()->search_terms_data();

  // Make sure the current page URL is sent in the request, if it is allowed.
  if (CanSendCurrentPageURLInRequest(
          input.current_url(), input.current_page_classification(),
          template_url, search_terms_data, client())) {
    search_term_args.current_page_url = input.current_url().spec();
  }

  LogOmniboxSuggestRequest(RemoteRequestEvent::kRequestSent);

  // If the request is from omnibox focus, send empty search term args. The
  // purpose of such a request is to signal the server to warm up; no info
  // is required.
  return client()
      ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->StartSuggestionsRequest(
          input.IsZeroSuggest() ? RemoteRequestType::kSearchWarmup
                                : RemoteRequestType::kSearch,
          template_url,
          input.IsZeroSuggest() ? TemplateURLRef::SearchTermsArgs()
                                : search_term_args,
          search_terms_data,
          base::BindOnce(&SearchProvider::OnURLLoadComplete,
                         base::Unretained(this)));
}

void SearchProvider::ConvertResultsToAutocompleteMatches() {
  // Convert all the results to matches and add them to a map, so we can keep
  // the most relevant match for each result.
  MatchMap map;
  int did_not_accept_keyword_suggestion =
      keyword_results_.suggest_results.empty()
          ? TemplateURLRef::NO_SUGGESTIONS_AVAILABLE
          : TemplateURLRef::NO_SUGGESTION_CHOSEN;

  bool relevance_from_server;
  int verbatim_relevance = GetVerbatimRelevance(&relevance_from_server);
  int did_not_accept_default_suggestion =
      default_results_.suggest_results.empty()
          ? TemplateURLRef::NO_SUGGESTIONS_AVAILABLE
          : TemplateURLRef::NO_SUGGESTION_CHOSEN;
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  const bool should_curb_default_suggestions =
      providers_.has_keyword_provider();
  // Don't add what-you-typed suggestion from the default provider when the
  // user requested keyword search.
  if (!should_curb_default_suggestions && verbatim_relevance > 0) {
    const std::u16string& trimmed_verbatim =
        base::CollapseWhitespace(input_.text(), false);

    // Verbatim results don't get suggestions and hence, answers.
    // Scan previous matches if the last answer-bearing suggestion matches
    // verbatim, and if so, copy over answer contents.
    AutocompleteMatch* match_with_answer = nullptr;
    std::u16string trimmed_verbatim_lower =
        base::i18n::ToLower(trimmed_verbatim);
    for (auto it = matches_.begin(); it != matches_.end(); ++it) {
      if (it->answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED &&
          base::i18n::ToLower(it->fill_into_edit) == trimmed_verbatim_lower) {
        match_with_answer = &(*it);
        break;
      }
    }

    SearchSuggestionParser::SuggestResult verbatim(
        /*suggestion=*/trimmed_verbatim,
        AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME,
        /*subtypes=*/{}, /*from_keyword=*/false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE, verbatim_relevance,
        relevance_from_server,
        /*input_text=*/trimmed_verbatim);
    if (match_with_answer) {
      verbatim.SetAnswerType(match_with_answer->answer_type);
      if (match_with_answer->answer) {
        verbatim.SetAnswer(*match_with_answer->answer);
      }
      if (match_with_answer->answer_template) {
        verbatim.SetRichAnswerTemplate(*match_with_answer->answer_template);
      }
    }
    AddMatchToMap(verbatim, GetInput(verbatim.from_keyword()),
                  GetTemplateURL(verbatim.from_keyword()),
                  client()->GetTemplateURLService()->search_terms_data(),
                  did_not_accept_default_suggestion, false,
                  keyword_url != nullptr, &map);
  }
  if (!keyword_input_.text().empty()) {
    // We only create the verbatim search query match for a keyword
    // if it's not an extension keyword.  Extension keywords are handled
    // in KeywordProvider::Start().  (Extensions are complicated...)
    // Note: in this provider, SEARCH_OTHER_ENGINE must correspond
    // to the keyword verbatim search query.  Do not create other matches
    // of type SEARCH_OTHER_ENGINE.
    //
    // In tabs search keyword mode, navigation (switch to open tab) suggestions
    // are provided, but there's no search results landing page to navigate to,
    // so it's not possible to open a verbatim search match. Do not provide one.
    if (keyword_url &&
        (keyword_url->type() != TemplateURL::OMNIBOX_API_EXTENSION) &&
        (keyword_url->starter_pack_id() != TemplateURLStarterPackData::kTabs)) {
      bool keyword_relevance_from_server;
      const int keyword_verbatim_relevance =
          GetKeywordVerbatimRelevance(&keyword_relevance_from_server);
      if (keyword_verbatim_relevance > 0) {
        const std::u16string& trimmed_verbatim =
            base::CollapseWhitespace(keyword_input_.text(), false);
        SearchSuggestionParser::SuggestResult verbatim(
            /*suggestion=*/trimmed_verbatim,
            AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME,
            /*subtypes=*/{}, /*from_keyword=*/true,
            /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
            keyword_verbatim_relevance, keyword_relevance_from_server,
            /*input_text=*/trimmed_verbatim);
        AddMatchToMap(verbatim, GetInput(verbatim.from_keyword()),
                      GetTemplateURL(verbatim.from_keyword()),
                      client()->GetTemplateURLService()->search_terms_data(),
                      did_not_accept_keyword_suggestion, false, true, &map);
      }
    }
  }
  AddRawHistoryResultsToMap(true, did_not_accept_keyword_suggestion, &map);
  if (!should_curb_default_suggestions)
    AddRawHistoryResultsToMap(false, did_not_accept_default_suggestion, &map);
  AddSuggestResultsToMap(keyword_results_.suggest_results, &map);
  if (!should_curb_default_suggestions) {
    AddSuggestResultsToMap(default_results_.suggest_results, &map);
  }
  ACMatches matches;
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches.push_back(i->second);

  AddNavigationResultsToMatches(keyword_results_.navigation_results, &matches);
  if (!should_curb_default_suggestions) {
    AddNavigationResultsToMatches(default_results_.navigation_results,
                                  &matches);
  }

  if (OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.Get()) {
    DuplicateCardAnswer(&matches);
  }
  // Now add the most relevant matches to |matches_|.  We take up to
  // provider_max_matches_ suggest/navsuggest matches, regardless of origin.  We
  // always include in that set a legal default match if possible. If we have
  // server-provided (and thus hopefully more accurate) scores for some
  // suggestions, we allow more of those, until we reach
  // AutocompleteResult::GetDynamicMaxMatches() total matches (that is, enough
  // to fill the whole popup).
  //
  // We will always return any verbatim matches, no matter how we obtained their
  // scores, unless we have already accepted
  // AutocompleteResult::GetDynamicMaxMatches() higher-scoring matches under
  // the conditions above.
  std::sort(matches.begin(), matches.end(), &AutocompleteMatch::MoreRelevant);

  // Guarantee that if there's a legal default match anywhere in the result
  // set that it'll get returned.  The rotate() call does this by moving the
  // default match to the front of the list.
  auto default_match = AutocompleteResult::FindTopMatch(input_, &matches);
  if (default_match != matches.end())
    std::rotate(matches.begin(), default_match, default_match + 1);

  // It's possible to get a copy of an answer from previous matches and get the
  // same or a different answer to another server-provided suggestion.  In the
  // future we may decide that we want to have answers attached to multiple
  // suggestions, but the current assumption is that there should only ever be
  // one suggestion with an answer.  To maintain this assumption, remove any
  // answers after the first.
  RemoveExtraAnswers(&matches);

  matches_.clear();
  size_t num_suggestions = 0;
  for (ACMatches::const_iterator i(matches.begin());
       (i != matches.end()) &&
       (matches_.size() < AutocompleteResult::GetDynamicMaxMatches());
       ++i) {
    // SEARCH_OTHER_ENGINE is only used in the SearchProvider for the keyword
    // verbatim result, so this condition basically means "if this match is a
    // suggestion of some sort".
    if ((i->type != AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED) &&
        (i->type != AutocompleteMatchType::SEARCH_OTHER_ENGINE)) {
      // If we've already hit the limit on non-server-scored suggestions, and
      // this isn't a server-scored suggestion we can add, skip it.
      // TODO (manukh): `GetAdditionalInfoForDebugging()` shouldn't be used for
      //   non-debugging purposes.
      if ((num_suggestions >= provider_max_matches_) &&
          (i->GetAdditionalInfoForDebugging(kRelevanceFromServerKey) !=
           kTrue)) {
        continue;
      }

      ++num_suggestions;
    }

    matches_.push_back(std::move(*i));
  }
}

void SearchProvider::RemoveExtraAnswers(ACMatches* matches) {
  bool answer_seen = false;
  for (auto it = matches->begin(); it != matches->end(); ++it) {
    if (it->answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
      if (!answer_seen) {
        answer_seen = true;
      } else {
        it->answer_type = omnibox::ANSWER_TYPE_UNSPECIFIED;
        it->answer.reset();
        it->answer_template.reset();
      }
    }
  }
}

void SearchProvider::DuplicateCardAnswer(ACMatches* matches) {
  auto iter = base::ranges::find_if(*matches, [](const auto& match) {
    return match.answer_template.has_value();
  });

  if (iter == matches->end()) {
    return;
  }

  bool orig_allowed_to_be_default_match = iter->allowed_to_be_default_match;
  iter->allowed_to_be_default_match = false;

  auto& copy = matches->emplace_back(*iter);
  copy.answer_template.reset();
  copy.actions.clear();
  copy.allowed_to_be_default_match = orig_allowed_to_be_default_match;
  copy.suggestion_group_id = omnibox::GROUP_SEARCH;
}

bool SearchProvider::IsTopMatchSearchWithURLInput() const {
  auto first_match = AutocompleteResult::FindTopMatch(input_, matches_);
  return (input_.type() == metrics::OmniboxInputType::URL) &&
         (first_match != matches_.end()) &&
         (first_match->relevance > CalculateRelevanceForVerbatim()) &&
         (first_match->type != AutocompleteMatchType::NAVSUGGEST) &&
         (first_match->type != AutocompleteMatchType::NAVSUGGEST_PERSONALIZED);
}

void SearchProvider::AddNavigationResultsToMatches(
    const SearchSuggestionParser::NavigationResults& navigation_results,
    ACMatches* matches) {
  for (auto it = navigation_results.begin(); it != navigation_results.end();
       ++it) {
    matches->push_back(NavigationToMatch(*it));
    // In the absence of suggested relevance scores, use only the single
    // highest-scoring result.  (The results are already sorted by relevance.)
    if (!it->relevance_from_server())
      return;
  }
}

void SearchProvider::AddRawHistoryResultsToMap(bool is_keyword,
                                               int did_not_accept_suggestion,
                                               MatchMap* map) {
  const SearchSuggestionParser::SuggestResults* transformed_results =
      is_keyword ? &transformed_keyword_history_results_
                 : &transformed_default_history_results_;
  DCHECK(transformed_results);
  AddTransformedHistoryResultsToMap(*transformed_results,
                                    did_not_accept_suggestion, map);
}

void SearchProvider::AddTransformedHistoryResultsToMap(
    const SearchSuggestionParser::SuggestResults& transformed_results,
    int did_not_accept_suggestion,
    MatchMap* map) {
  for (const auto& result : transformed_results) {
    AddMatchToMap(result, GetInput(result.from_keyword()),
                  GetTemplateURL(result.from_keyword()),
                  client()->GetTemplateURLService()->search_terms_data(),
                  did_not_accept_suggestion, true,
                  providers_.GetKeywordProviderURL() != nullptr, map);
  }
}

SearchSuggestionParser::SuggestResults
SearchProvider::ScoreHistoryResultsHelper(const HistoryResults& results,
                                          bool base_prevent_inline_autocomplete,
                                          bool input_multiple_words,
                                          const std::u16string& input_text,
                                          bool is_keyword) {
  SearchSuggestionParser::SuggestResults scored_results;
  // True if the user has asked this exact query previously.
  bool found_what_you_typed_match = false;
  std::u16string trimmed_input = base::CollapseWhitespace(input_text, false);
  if (base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions)) {
    trimmed_input = base::i18n::ToLower(trimmed_input);
  }
  for (const auto& result : results) {
    const std::u16string& trimmed_suggestion =
        base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions)
            ? result->normalized_term
            : base::CollapseWhitespace(result->term, false);

    // Don't autocomplete multi-word queries that have only been seen once
    // unless the user has typed more than one word.
    bool prevent_inline_autocomplete =
        base_prevent_inline_autocomplete ||
        (!input_multiple_words && (result->visit_count < 2) &&
         HasMultipleWords(trimmed_suggestion));

    int relevance = CalculateRelevanceForHistory(
        result->last_visit_time, is_keyword, !prevent_inline_autocomplete);
    // Add the match to |scored_results| by putting the what-you-typed match
    // on the front and appending all other matches.  We want the what-you-
    // typed match to always be first.
    auto insertion_position = scored_results.end();
    if (trimmed_suggestion == trimmed_input) {
      found_what_you_typed_match = true;
      insertion_position = scored_results.begin();
    }
    SearchSuggestionParser::SuggestResult history_suggestion(
        /*suggestion=*/trimmed_suggestion,
        AutocompleteMatchType::SEARCH_HISTORY,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
        is_keyword, /*navigational_intent=*/omnibox::NAV_INTENT_NONE, relevance,
        /*relevance_from_server=*/false, /*input_text=*/trimmed_input);
    // History results are synchronous; they are received on the last keystroke.
    history_suggestion.set_received_after_last_keystroke(false);
    scored_results.insert(insertion_position, history_suggestion);
  }

  // History returns results sorted for us.  However, we may have docked some
  // results' scores, so things are no longer in order.  While keeping the
  // what-you-typed match at the front (if it exists), do a stable sort to get
  // things back in order without otherwise disturbing results with equal
  // scores, then force the scores to be unique, so that the order in which
  // they're shown is deterministic.
  std::stable_sort(
      scored_results.begin() + (found_what_you_typed_match ? 1 : 0),
      scored_results.end(), CompareScoredResults());

  // Don't autocomplete to search terms that would normally be treated as URLs
  // when typed. For example, if the user searched for "google.com" and types
  // "goog", don't autocomplete to the search term "google.com". Otherwise,
  // the input will look like a URL but act like a search, which is confusing.
  // The 1200 relevance score threshold in the test below is the lowest
  // possible score in CalculateRelevanceForHistory()'s aggressive-scoring
  // curve.  This is an appropriate threshold to use to decide if we're overly
  // aggressively inlining because, if we decide the answer is yes, the
  // way we resolve it it to not use the aggressive-scoring curve.
  // NOTE: We don't check for autocompleting to URLs in the following cases:
  //  * When inline autocomplete is disabled, we won't be inline autocompleting
  //    this term, so we don't need to worry about confusion as much.  This
  //    also prevents calling Classify() again from inside the classifier
  //    (which will corrupt state and likely crash), since the classifier
  //    always disables inline autocomplete.
  //  * When the user has typed the whole string before as a query, then it's
  //    likely the user has no expectation that term should be interpreted as
  //    as a URL, so we need not do anything special to preserve user
  //    expectation.
  int last_relevance = 0;
  if (!base_prevent_inline_autocomplete && !found_what_you_typed_match &&
      scored_results.front().relevance() >= 1200) {
    AutocompleteMatch match;
    client()->Classify(scored_results.front().suggestion(), false, false,
                       input_.current_page_classification(), &match, nullptr);
    // Demote this match that would normally be interpreted as a URL to have
    // the highest score a previously-issued search query could have when
    // scoring with the non-aggressive method.  A consequence of demoting
    // by revising |last_relevance| is that this match and all following
    // matches get demoted; the relative order of matches is preserved.
    // One could imagine demoting only those matches that might cause
    // confusion (which, by the way, might change the relative order of
    // matches.  We have decided to go with the simple demote-all approach
    // because selective demotion requires multiple Classify() calls and
    // such calls can be expensive (as expensive as running the whole
    // autocomplete system).
    if (!AutocompleteMatch::IsSearchType(match.type)) {
      last_relevance =
          CalculateRelevanceForHistory(base::Time::Now(), is_keyword, false);
    }
  }

  for (auto i(scored_results.begin()); i != scored_results.end(); ++i) {
    if ((last_relevance != 0) && (i->relevance() >= last_relevance))
      i->set_relevance(last_relevance - 1);
    last_relevance = i->relevance();
  }

  return scored_results;
}

void SearchProvider::ScoreHistoryResults(
    const HistoryResults& results,
    bool is_keyword,
    SearchSuggestionParser::SuggestResults* scored_results) {
  DCHECK(scored_results);
  scored_results->clear();

  if (results.empty()) {
    return;
  }

  bool prevent_inline_autocomplete =
      input_.prevent_inline_autocomplete() ||
      (input_.type() == metrics::OmniboxInputType::URL);
  const std::u16string input_text = GetInput(is_keyword).text();
  bool input_multiple_words = HasMultipleWords(input_text);

  if (!prevent_inline_autocomplete && input_multiple_words) {
    // ScoreHistoryResultsHelper() allows autocompletion of multi-word, 1-visit
    // queries if the input also has multiple words.  But if we were already
    // scoring a multi-word, multi-visit query aggressively, and the current
    // input is still a prefix of it, then changing the suggestion suddenly
    // feels wrong.  To detect this case, first score as if only one word has
    // been typed, then check if the best result came from aggressive search
    // history scoring.  If it did, then just keep that score set.  This
    // 1200 the lowest possible score in CalculateRelevanceForHistory()'s
    // aggressive-scoring curve.
    *scored_results = ScoreHistoryResultsHelper(
        results, prevent_inline_autocomplete, false, input_text, is_keyword);
    if ((scored_results->front().relevance() < 1200) ||
        !HasMultipleWords(scored_results->front().suggestion()))
      scored_results->clear();  // Didn't detect the case above, score normally.
  }
  if (scored_results->empty()) {
    *scored_results =
        ScoreHistoryResultsHelper(results, prevent_inline_autocomplete,
                                  input_multiple_words, input_text, is_keyword);
  }
}

void SearchProvider::AddSuggestResultsToMap(
    const SearchSuggestionParser::SuggestResults& results,
    MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i) {
    AddMatchToMap(results[i], GetInput(results[i].from_keyword()),
                  GetTemplateURL(results[i].from_keyword()),
                  client()->GetTemplateURLService()->search_terms_data(), i,
                  false, providers_.GetKeywordProviderURL() != nullptr, map);
  }
}

int SearchProvider::GetVerbatimRelevance(bool* relevance_from_server) const {
  // Use the suggested verbatim relevance score if it is non-negative (valid),
  // if inline autocomplete isn't prevented (always show verbatim on backspace),
  // and if it won't suppress verbatim, leaving no default provider matches.
  // Otherwise, if the default provider returned no matches and was still able
  // to suppress verbatim, the user would have no search/nav matches and may be
  // left unable to search using their default provider from the omnibox.
  // Check for results on each verbatim calculation, as results from older
  // queries (on previous input) may be trimmed for failing to inline new input.
  bool use_server_relevance = (default_results_.verbatim_relevance >= 0) &&
                              !input_.prevent_inline_autocomplete() &&
                              ((default_results_.verbatim_relevance > 0) ||
                               !default_results_.suggest_results.empty() ||
                               !default_results_.navigation_results.empty());
  if (relevance_from_server)
    *relevance_from_server = use_server_relevance;
  return use_server_relevance ? default_results_.verbatim_relevance
                              : CalculateRelevanceForVerbatim();
}

int SearchProvider::CalculateRelevanceForVerbatim() const {
  if (!providers_.keyword_provider().empty())
    return 250;
  return CalculateRelevanceForVerbatimIgnoringKeywordModeState();
}

int SearchProvider::CalculateRelevanceForVerbatimIgnoringKeywordModeState()
    const {
  switch (input_.type()) {
    case metrics::OmniboxInputType::UNKNOWN:
    case metrics::OmniboxInputType::QUERY:
      return kNonURLVerbatimRelevance;

    case metrics::OmniboxInputType::URL:
      return 850;

    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

int SearchProvider::GetKeywordVerbatimRelevance(
    bool* relevance_from_server) const {
  // Use the suggested verbatim relevance score if it is non-negative (valid),
  // if inline autocomplete isn't prevented (always show verbatim on backspace),
  // and if it won't suppress verbatim, leaving no keyword provider matches.
  // Otherwise, if the keyword provider returned no matches and was still able
  // to suppress verbatim, the user would have no search/nav matches and may be
  // left unable to search using their keyword provider from the omnibox.
  // Check for results on each verbatim calculation, as results from older
  // queries (on previous input) may be trimmed for failing to inline new input.
  bool use_server_relevance = (keyword_results_.verbatim_relevance >= 0) &&
                              !input_.prevent_inline_autocomplete() &&
                              ((keyword_results_.verbatim_relevance > 0) ||
                               !keyword_results_.suggest_results.empty() ||
                               !keyword_results_.navigation_results.empty());
  if (relevance_from_server)
    *relevance_from_server = use_server_relevance;
  return use_server_relevance ? keyword_results_.verbatim_relevance
                              : CalculateRelevanceForKeywordVerbatim(
                                    keyword_input_.type(), true,
                                    keyword_input_.prefer_keyword());
}

int SearchProvider::CalculateRelevanceForHistory(
    const base::Time& time,
    bool is_keyword,
    bool use_aggressive_method) const {
  // The relevance of past searches falls off over time. There are two distinct
  // equations used. If the first equation is used (searches to the primary
  // provider that we want to score aggressively), the score is in the range
  // 1300-1599. If the second equation is used the relevance of a search 15
  // minutes ago is discounted 50 points, while the relevance of a search two
  // weeks ago is discounted 450 points.
  double elapsed_time = std::max((base::Time::Now() - time).InSecondsF(), 0.0);
  bool is_primary_provider = is_keyword || !providers_.has_keyword_provider();
  if (is_primary_provider && use_aggressive_method) {
    // Searches with the past two days get a different curve.
    const double autocomplete_time = 2 * 24 * 60 * 60;
    if (elapsed_time < autocomplete_time) {
      int max_score = is_keyword ? 1599 : 1399;
      return max_score -
             static_cast<int>(99 *
                              std::pow(elapsed_time / autocomplete_time, 2.5));
    }
    elapsed_time -= autocomplete_time;
  }

  const int score_discount =
      static_cast<int>(6.5 * std::pow(elapsed_time, 0.3));

  // Don't let scores go below 0.  Negative relevance scores are meaningful in
  // a different way.
  int base_score;
  if (is_primary_provider)
    base_score = (input_.type() == metrics::OmniboxInputType::URL) ? 750 : 1050;
  else
    base_score = 200;
  return std::max(0, base_score - score_discount);
}

AutocompleteMatch SearchProvider::NavigationToMatch(
    const SearchSuggestionParser::NavigationResult& navigation) {
  std::u16string input;
  const bool trimmed_whitespace =
      base::TrimWhitespace(
          navigation.from_keyword() ? keyword_input_.text() : input_.text(),
          base::TRIM_TRAILING, &input) != base::TRIM_NONE;
  AutocompleteMatch match(this, navigation.relevance(), false,
                          navigation.type());
  match.destination_url = navigation.url();
  match.suggest_type = navigation.suggest_type();
  for (const int subtype : navigation.subtypes()) {
    match.subtypes.insert(SuggestSubtypeForNumber(subtype));
  }
  BaseSearchProvider::SetDeletionURL(navigation.deletion_url(), &match);
  // First look for the user's input inside the formatted url as it would be
  // without trimming the scheme, so we can find matches at the beginning of the
  // scheme.
  const URLPrefix* prefix =
      URLPrefix::BestURLPrefix(navigation.formatted_url(), input);
  size_t match_start = (prefix == nullptr)
                           ? navigation.formatted_url().find(input)
                           : prefix->prefix.length();
  bool trim_http =
      !AutocompleteInput::HasHTTPScheme(input) && (!prefix || match_start != 0);
  const url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults &
      ~(trim_http ? 0 : url_formatter::kFormatUrlOmitHTTP);

  size_t inline_autocomplete_offset = (prefix == nullptr)
                                          ? std::u16string::npos
                                          : (match_start + input.length());
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          navigation.url(),
          url_formatter::FormatUrl(navigation.url(), format_types,
                                   base::UnescapeRule::SPACES, nullptr, nullptr,
                                   &inline_autocomplete_offset),
          client()->GetSchemeClassifier(), &inline_autocomplete_offset);
  if (inline_autocomplete_offset != std::u16string::npos) {
    DCHECK(inline_autocomplete_offset <= match.fill_into_edit.length());
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
  }
  // An inlinable navsuggestion can only be the default match when there
  // is no keyword provider active, lest it appear first and break the user
  // out of keyword mode.  We also must have received the navsuggestion before
  // the last keystroke, to prevent asynchronous inline autocompletions changes.
  // The navsuggestion can also only be default if either the inline
  // autocompletion is empty or we're not preventing inline autocompletion.
  // Finally, if we have an inlinable navsuggestion with an inline completion
  // that we're not preventing, make sure we didn't trim any whitespace.
  // We don't want to claim http://foo.com/bar is inlinable against the
  // input "foo.com/b ".
  match.allowed_to_be_default_match =
      (prefix != nullptr) && (providers_.GetKeywordProviderURL() == nullptr) &&
      !navigation.received_after_last_keystroke() &&
      (match.inline_autocompletion.empty() ||
       (!input_.prevent_inline_autocomplete() && !trimmed_whitespace));

  match.contents = navigation.match_contents();
  match.contents_class = navigation.match_contents_class();
  match.description = navigation.description();
  match.description_class = navigation.description_class();

  match.RecordAdditionalInfo(
      kRelevanceFromServerKey,
      navigation.relevance_from_server() ? kTrue : kFalse);
  match.RecordAdditionalInfo(kShouldPrefetchKey, kFalse);

  match.from_keyword = navigation.from_keyword();

  // Only set scoring signals for eligible matches.
  if (match.IsMlSignalLoggingEligible()) {
    // Initialize the ML scoring signals for this suggestion if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    if (navigation.relevance_from_server()) {
      match.scoring_signals->set_search_suggest_relevance(
          navigation.relevance());
    }
    SearchScoringSignalsAnnotator::UpdateMatchTypeScoringSignals(match, input);
  }

  return match;
}

void SearchProvider::UpdateDone() {
  // We're done when the timer isn't running and there are no suggest queries
  // pending.
  done_ = !timer_.IsRunning() && !default_loader_ && !keyword_loader_;
}

AnswersQueryData SearchProvider::FindAnswersPrefetchData() {
  // Retrieve the top entry from scored history results.
  MatchMap map;
  AddTransformedHistoryResultsToMap(transformed_keyword_history_results_,
                                    TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
                                    &map);
  AddTransformedHistoryResultsToMap(transformed_default_history_results_,
                                    TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
                                    &map);

  ACMatches matches;
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches.push_back(i->second);
  std::sort(matches.begin(), matches.end(), &AutocompleteMatch::MoreRelevant);

  // If there is a top scoring entry, find the corresponding answer.
  if (!matches.empty())
    return answers_cache_.GetTopAnswerEntry(matches[0].contents);

  return AnswersQueryData();
}

void SearchProvider::PrefetchImages(SearchSuggestionParser::Results* results) {
  // The server sends back as many as 20 suggestions that may have
  // images but only a few of these will end up getting shown.  Limit the images
  // prefetched to those for most relevant results that will get shown.  This
  // will prevent blasting the cache, causing reloads & flicker.  The results
  // are processed in descending order of relevance so the first suggestions are
  // the ones to be shown; prefetching images for the rest would be wasteful.
  std::vector<GURL> prefetch_image_urls;
  size_t prefetch_limit = AutocompleteResult::GetDynamicMaxMatches();
  for (size_t i = 0; i < prefetch_limit && i < results->suggest_results.size();
       ++i) {
    auto suggestion = results->suggest_results[i];

    GURL entity_image_url = GURL(suggestion.entity_info().image_url());
    if (entity_image_url.is_valid()) {
      prefetch_image_urls.push_back(std::move(entity_image_url));
    }

    GURL answer_image_url =
        omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
                suggestion.answer_template()
            ? GURL(suggestion.answer_template()->answers(0).image().url())
            : ((suggestion.answer() ? suggestion.answer()->image_url()
                                    : GURL()));
    if (answer_image_url.is_valid()) {
      prefetch_image_urls.push_back(std::move(answer_image_url));
    }
  }

  for (const GURL& url : prefetch_image_urls)
    client()->PrefetchImage(url);
}
