// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_quick_provider.h"

#include <stddef.h>

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

bool HistoryQuickProvider::disabled_ = false;

HistoryQuickProvider::HistoryQuickProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_QUICK, client),
      in_memory_url_index_(client->GetInMemoryURLIndex()) {}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryQuickProvider::Start");
  matches_.clear();
  if (disabled_ || input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client()->GetTemplateURLService());
  autocomplete_input_ = std::move(adjusted_input);
  starter_pack_engine_ = starter_pack_engine;

  if (in_memory_url_index_) {
    DoAutocomplete();
  }
}

size_t HistoryQuickProvider::EstimateMemoryUsage() const {
  size_t res = HistoryProvider::EstimateMemoryUsage();

  res += base::trace_event::EstimateMemoryUsage(autocomplete_input_);

  return res;
}

HistoryQuickProvider::~HistoryQuickProvider() = default;

void HistoryQuickProvider::DoAutocomplete() {
  // In keyword mode, it's possible we only provide results from one or two
  // autocomplete provider(s), so it's sometimes necessary to show more results
  // than provider_max_matches_.
  size_t max_matches = autocomplete_input_.InKeywordMode()
                           ? provider_max_matches_in_keyword_mode_
                           : provider_max_matches_;

  // Get the matching URLs from the DB.
  ScoredHistoryMatches matches = in_memory_url_index_->HistoryItemsForTerms(
      autocomplete_input_.text(), autocomplete_input_.cursor_position(), "",
      max_matches, client()->GetOmniboxTriggeredFeatureService());
  if (matches.empty())
    return;

  // `original_max_match_score` keeps track of the potential URL-what-you-typed
  // suggestion's score; all HQP suggestions should be scored strictly lower.
  const auto original_max_match_score = MaxMatchScore();
  const auto add_matches = [&](const ScoredHistoryMatches& matches) {
    // `max_match_score` keeps track of the scores within `matches` to guarantee
    // scores are decreasing within each batch. Scores from subsequent batches
    // may be higher.
    int max_match_score =
        original_max_match_score.value_or(matches[0].raw_score);
    for (const auto& history_match : matches) {
      // Set max_match_score to the score we'll assign this result.
      max_match_score = std::min(max_match_score, history_match.raw_score);
      matches_.push_back(QuickMatchToACMatch(history_match, max_match_score));
      // Mark this max_match_score as being used.
      max_match_score--;
    }
  };

  add_matches(matches);

  // If ML scoring is enabled, mark all "extra" matches as `culled_by_provider`.
  // If ML scoring is disabled, this is effectively a no-op as the matches will
  // already be resized in the above call to `HistoryItemsForTerms()`.
  ResizeMatches(
      max_matches,
      OmniboxFieldTrial::IsMlUrlScoringUnlimitedNumCandidatesEnabled());

  // Add suggestions from the user's highly visited domains bypassing
  // `provider_max_matches_`.

  // In keyword mode, already have enough matches.
  if (autocomplete_input_.InKeywordMode()) {
    return;
  }

  static const size_t domain_suggestions_min_char =
      OmniboxFieldTrial::kDomainSuggestionsMinInputLength.Get();
  static const int max_host_matches =
      OmniboxFieldTrial::kDomainSuggestionsMaxMatchesPerDomain.Get();
  if (autocomplete_input_.text().length() < domain_suggestions_min_char ||
      max_host_matches == 0) {
    return;
  }

  // Append suggestions for each of the user's highly visited domains. To
  // determine these domains, the user's visits are aggregated by URL host and
  // their aggregate info (e.g. sum typed count) are considered. Each highly
  // visited domain gets its own `max_matches` allowance.
  for (const auto& host : in_memory_url_index_->HighlyVisitedHosts()) {
    // TODO(manukh): Calling `HistoryItemsForTerms()` is somewhat wasteful. URLs
    //  have 1 host, so they'll be re-processed in at most 1 iteration. A
    //  typical input that triggered this feature will match about 100 history
    //  items, which are all scored. If the suggestions are from highly visited,
    //  the number of history items scored will at most double, so about an
    //  extra 100 items scored. Sorting, deduping, and converting to
    //  `AutocompleteMatch`es are only done on 6 (or less) history items, so
    //  those are not as big of a concern. If performance metrics regress, we
    //  should extract matching and scoring history items from
    //  `HistoryItemsForTerms()` so it can be done just once.
    ScoredHistoryMatches host_matches =
        in_memory_url_index_->HistoryItemsForTerms(
            autocomplete_input_.text(), autocomplete_input_.cursor_position(),
            host, max_host_matches,
            client()->GetOmniboxTriggeredFeatureService());
    // TODO(manukh): Consider using a new `AutocompleteMatchType` for domain
    //  suggestions to distinguish them in metrics.
    if (!host_matches.empty()) {
      client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_DOMAIN_SUGGESTIONS);
      static const bool counterfactual =
          OmniboxFieldTrial::kDomainSuggestionsCounterfactual.Get();
      if (!counterfactual)
        add_matches(host_matches);
    }
  }
}

std::optional<int> HistoryQuickProvider::MaxMatchScore() {
  // Figure out if HistoryURL provider has a URL-what-you-typed match
  // that ought to go first and what its score will be.
  bool will_have_url_what_you_typed_match_first = false;
  int url_what_you_typed_match_score = -1;  // undefined
  // These are necessary (but not sufficient) conditions for the omnibox
  // input to be a URL-what-you-typed match.  The username test checks that
  // either the username does not exist (a regular URL such as http://site/)
  // or, if the username exists (http://user@site/), there must be either
  // a password or a port.  Together these exclude pure username@site
  // inputs because these are likely to be an e-mail address.  HistoryURL
  // provider won't promote the URL-what-you-typed match to first
  // for these inputs.
  const bool can_have_url_what_you_typed_match_first =
      (autocomplete_input_.type() != metrics::OmniboxInputType::QUERY) &&
      (autocomplete_input_.parts().username.is_empty() ||
       autocomplete_input_.parts().password.is_nonempty() ||
       autocomplete_input_.parts().path.is_nonempty());
  if (can_have_url_what_you_typed_match_first) {
    history::HistoryService* const history_service =
        client()->GetHistoryService();
    // We expect HistoryService to be available.  In case it's not,
    // (e.g., due to Profile corruption) we let HistoryQuick provider
    // completions (which may be available because it's a different
    // data structure) compete with the URL-what-you-typed match as
    // normal.
    if (history_service) {
      history::URLDatabase* url_db = history_service->InMemoryDatabase();
      // `url_db` can be null if it hasn't finished initializing (or failed to
      // initialize). In this case, let `HistoryQuickProvider` completions
      // compete with the URL-what-you-typed match as normal.
      if (url_db) {
        const std::string host(
            base::UTF16ToUTF8(autocomplete_input_.text().substr(
                autocomplete_input_.parts().host.begin,
                autocomplete_input_.parts().host.len)));
        // We want to put the URL-what-you-typed match first if either
        // * the user visited the URL before (intranet or internet).
        // * it's a URL on a host that user visited before and this
        //   is the root path of the host.  (If the user types some
        //   of a path--more than a simple "/"--we let autocomplete compete
        //   normally with the URL-what-you-typed match.)
        // TODO(mpearson): Remove this hacky code and simply score URL-what-
        // you-typed in some sane way relative to possible completions:
        // URL-what-you-typed should get some sort of a boost relative
        // to completions, but completions should naturally win if
        // they're a lot more popular.  In this process, if the input
        // is a bare intranet hostname that has been visited before, we
        // may want to enforce that the only completions that can outscore
        // the URL-what-you-typed match are on the same host (i.e., aren't
        // from a longer internet hostname for which the omnibox input is
        // a prefix).
        if (url_db->GetRowForURL(autocomplete_input_.canonicalized_url(),
                                 nullptr) != 0) {
          // We visited this URL before.
          will_have_url_what_you_typed_match_first = true;
          // HistoryURLProvider gives visited what-you-typed URLs a high score.
          url_what_you_typed_match_score =
              HistoryURLProvider::kScoreForBestInlineableResult;
        } else if (url_db->IsTypedHost(host, /*scheme=*/nullptr) &&
                   (autocomplete_input_.parts().path.is_empty() ||
                    ((autocomplete_input_.parts().path.len == 1) &&
                     (autocomplete_input_
                          .text()[autocomplete_input_.parts().path.begin] ==
                      '/'))) &&
                   autocomplete_input_.parts().query.is_empty() &&
                   autocomplete_input_.parts().ref.is_empty()) {
          // Not visited, but we've seen the host before.
          will_have_url_what_you_typed_match_first = true;
          if (net::registry_controlled_domains::HostHasRegistryControlledDomain(
                  host,
                  net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
                  net::registry_controlled_domains::
                      EXCLUDE_PRIVATE_REGISTRIES)) {
            // Known internet host.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForWhatYouTypedResult;
          } else {
            // An intranet host.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForUnvisitedIntranetResult;
          }
        }
      }
    }
  }
  return will_have_url_what_you_typed_match_first
             ? std::optional<int>{url_what_you_typed_match_score - 1}
             : std::nullopt;
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    int score) {
  const history::URLRow& info = history_match.url_info;
  bool deletable =
      !!info.visit_count() && client()->AllowDeletingBrowserHistory();
  AutocompleteMatch match(this, score, deletable,
                          history_match.url_matches.empty()
                              ? AutocompleteMatchType::HISTORY_TITLE
                              : AutocompleteMatchType::HISTORY_URL);
  match.typed_count = info.typed_count();
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());

  // The inline_autocomplete_offset should be adjusted based on the formatting
  // applied to |fill_into_edit|.
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      autocomplete_input_.text(), FixupUserInput(autocomplete_input_).second,
      false, base::UTF8ToUTF16(info.url().spec()));
  auto fill_into_edit_format_types = url_formatter::kFormatUrlOmitDefaults;
  if (history_match.match_in_scheme)
    fill_into_edit_format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          info.url(),
          url_formatter::FormatUrl(info.url(), fill_into_edit_format_types,
                                   base::UnescapeRule::SPACES, nullptr, nullptr,
                                   &inline_autocomplete_offset),
          client()->GetSchemeClassifier(), &inline_autocomplete_offset);

  // HistoryQuick classification diverges from relevance scoring. Specifically,
  // 1) All occurrences of the input contribute to relevance; e.g. for the input
  // 'pre', the suggestion 'pre prefix' will be scored higher than 'pre suffix'.
  // For classification though, if the input is a prefix of the suggestion text,
  // only the prefix will be bolded; e.g. the 1st suggestion will display '[pre]
  // prefix' as opposed to '[pre] [pre]fix'. This divergence allows consistency
  // with other providers' and google.com's bolding.
  // 2) Mid-word occurrences of the input within the suggestion URL contribute
  // to relevance; e.g. for the input 'mail', the suggestion 'mail - gmail.com'
  // will be scored higher than 'mail - outlook.live.com'. Mid-word matches only
  // in the domain affect scoring. For classification though, mid-word matches
  // are not bolded; e.g. the 1st suggestion will display '[mail] - gmail.com'.
  // 3) User input is not broken on symbols for relevance calculations; e.g. for
  // the input '#yolo', the suggestion 'how-to-yolo - yolo.com/#yolo' would be
  // scored the same as 'how-to-tie-a-tie - yolo.com/#yolo/tie'. For
  // classification though, user input is broken on symbols; e.g. the 1st
  // suggestion will display 'how-to-[yolo] - [yolo].com/#[yolo]'.

  // If this is a document suggestion, hide its URL for (a) consistency with the
  // document provider and (b) ease of reading.
  // TODO(manukh): For doc suggestions, the description will be
  //  'Doc Title - Google [Docs|Sheets...]'. For additional consistency with
  //  the document provider, the description could be split to 'Doc Title' and
  //  'Google [Docs|Sheets...]', moving the latter to contents. But for
  //  now, do the simpler thing of just clearing the URL.
  if (!match.IsDocumentSuggestion()) {
    match.contents = url_formatter::FormatUrl(
        info.url(),
        AutocompleteMatch::GetFormatTypes(
            autocomplete_input_.parts().scheme.is_nonempty() ||
                history_match.match_in_scheme,
            history_match.match_in_subdomain),
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
    auto contents_terms =
        FindTermMatches(autocomplete_input_.text(), match.contents);
    match.contents_class = ClassifyTermMatches(
        contents_terms, match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
  }

  match.description = AutocompleteMatch::SanitizeString(info.title());
  auto description_terms =
      FindTermMatches(autocomplete_input_.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  // Set |inline_autocompletion| and |allowed_to_be_default_match| if possible.
  if (match.TryRichAutocompletion(match.contents, match.description,
                                  autocomplete_input_)) {
    // If rich autocompletion applies, we skip trying the alternatives below.
  } else if (inline_autocomplete_offset != std::u16string::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.SetAllowedToBeDefault(autocomplete_input_);
  }

  // If the input was in a starter pack keyword scope, set the `keyword` and
  // `transition` appropriately to avoid popping the user out of keyword mode.
  if (starter_pack_engine_) {
    match.keyword = starter_pack_engine_->keyword();
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
  }

  if (autocomplete_input_.InKeywordMode()) {
    match.from_keyword = true;
  }

  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled() &&
      match.IsMlSignalLoggingEligible()) {
    // Propagate scoring signals to AC Match for ML Model training data.
    // `allowed_to_be_default_match` is set in this function, after the ACMatch
    // is constructed, rather than in ScoredHistoryMatch. We have to propagate
    // that signal to `scoring_signals` in addition to all signals calculated in
    // the ScoredHistoryMatch.
    DCHECK(history_match.scoring_signals.has_value());
    match.scoring_signals = history_match.scoring_signals;
    match.scoring_signals->set_allowed_to_be_default_match(
        match.allowed_to_be_default_match);
  }
  match.RecordAdditionalInfo("typed count", info.typed_count());
  match.RecordAdditionalInfo("visit count", info.visit_count());
  match.RecordAdditionalInfo("last visit", info.last_visit());
  match.RecordAdditionalInfo("raw score before domain boosting",
                             history_match.raw_score_before_domain_boosting);
  match.RecordAdditionalInfo("raw score after domain boosting",
                             history_match.raw_score_after_domain_boosting);
  return match;
}
