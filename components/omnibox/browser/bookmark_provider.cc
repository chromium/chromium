// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/bookmark_provider.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

using bookmarks::BookmarkNode;
using bookmarks::TitledUrlMatch;

BookmarkProvider::BookmarkProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BOOKMARK),
      client_(client),
      bookmark_model_(client ? client_->GetBookmarkModel() : nullptr) {}

void BookmarkProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  TRACE_EVENT0("omnibox", "BookmarkProvider::Start");
  matches_.clear();

  if (input.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT ||
      input.text().empty())
    return;

  DoAutocomplete(input);
}

BookmarkProvider::~BookmarkProvider() = default;

void BookmarkProvider::DoAutocomplete(const AutocompleteInput& input) {
  // We may not have a bookmark model for some unit tests.
  if (!bookmark_model_)
    return;

  // Retrieve enough bookmarks so that we have a reasonable probability of
  // suggesting the one that the user desires.
  const size_t kMaxBookmarkMatches = 50;

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client_->GetTemplateURLService());

  // GetBookmarksMatching returns bookmarks matching the user's
  // search terms using the following rules:
  //  - The search text is broken up into search terms. Each term is searched
  //    for separately.
  //  - Term matches are always performed against the start of a word. 'def'
  //    will match against 'define' but not against 'indefinite'.
  //  - Terms must be at least three characters in length in order to perform
  //    partial word matches. Any term of lesser length will only be used as an
  //    exact match. 'def' will match against 'define' but 'de' will not match.
  //    (Unless either |IsShortBookmarkSuggestionsEnabled()| or
  //    |IsShortBookmarkSuggestionsByTotalInputLengthEnabled()| is true.)
  //  - A search containing multiple terms will return results with those words
  //    occurring in any order.
  //  - Terms enclosed in quotes comprises a phrase that must match exactly.
  //  - Multiple terms enclosed in quotes will require those exact words in that
  //    exact order to match.
  //
  // Please refer to the code for TitledUrlIndex::GetResultsMatching for
  // complete details of how searches are performed against the user's
  // bookmarks.
  std::vector<TitledUrlMatch> matches =
      GetMatchesWithBookmarkPaths(adjusted_input, kMaxBookmarkMatches);
  if (matches.empty())
    return;  // There were no matches.
  const std::u16string fixed_up_input(FixupUserInput(adjusted_input).second);
  for (auto& bookmark_match : matches) {
    if (OmniboxFieldTrial::ShouldDisableCGIParamMatching()) {
      RemoveQueryParamKeyMatches(bookmark_match);
      if (bookmark_match.title_match_positions.empty() &&
          bookmark_match.url_match_positions.empty())
        continue;
    }
    // Score the TitledUrlMatch. If its score is greater than 0 then the
    // AutocompleteMatch is created and added to matches_.
    auto [relevance, bookmark_count] =
        CalculateBookmarkMatchRelevance(bookmark_match);
    if (relevance > 0) {
      AutocompleteMatch match = TitledUrlMatchToAutocompleteMatch(
          bookmark_match, AutocompleteMatchType::BOOKMARK_TITLE, relevance,
          bookmark_count, this, client_->GetSchemeClassifier(), adjusted_input,
          fixed_up_input);
      // If the input was in a starter pack keyword scope, set the `keyword` and
      // `transition` appropriately to avoid popping the user out of keyword
      // mode.
      if (starter_pack_engine) {
        match.keyword = starter_pack_engine->keyword();
        match.transition = ui::PAGE_TRANSITION_KEYWORD;
      }

      matches_.push_back(match);
    }
  }

  // In keyword mode, it's possible we only provide results from one or two
  // autocomplete provider(s), so it's sometimes necessary to show more results
  // than provider_max_matches_.
  size_t max_matches = InKeywordMode(adjusted_input)
                           ? provider_max_matches_in_keyword_mode_
                           : provider_max_matches_;

  // Sort and clip the resulting matches.
  size_t num_matches = std::min(matches_.size(), max_matches);
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), AutocompleteMatch::MoreRelevant);
  matches_.resize(num_matches);
}

std::vector<TitledUrlMatch> BookmarkProvider::GetMatchesWithBookmarkPaths(
    const AutocompleteInput& input,
    size_t kMaxBookmarkMatches) {
  // Determining whether the |kBookmarkPaths| feature had, or would have had, an
  // impact for counterfactual logging is expensive as it requires invoking
  // |BookmarkModel::GetBookmarksMatching()| twice. Therefore, the param
  // |kBookmarkPathsCounterfactual| determines whether to counterfactual log:
  // - When empty, counterfactual logging won't occur.
  // - When set to "control" counterfactual logging will occur like usual; i.e.
  //   path matched bookmarks won't be returned but will be compared to
  //   determine if the feature triggered.
  // - When set to "enabled", counterfactual logging will occur and path matched
  //   bookmarks will be returned.
  std::string counterfactual =
      OmniboxFieldTrial::kBookmarkPathsCounterfactual.Get();

  bool match_paths = base::FeatureList::IsEnabled(omnibox::kBookmarkPaths) &&
                     counterfactual != "control";

  query_parser::MatchingAlgorithm matching_algorithm =
      GetMatchingAlgorithm(input);

  if (counterfactual.empty()) {
    return bookmark_model_->GetBookmarksMatching(
        input.text(), kMaxBookmarkMatches, matching_algorithm, match_paths);
  }

  std::vector<TitledUrlMatch> matches_without_paths =
      bookmark_model_->GetBookmarksMatching(input.text(), kMaxBookmarkMatches,
                                            matching_algorithm);
  std::vector<TitledUrlMatch> matches_with_paths =
      bookmark_model_->GetBookmarksMatching(input.text(), kMaxBookmarkMatches,
                                            matching_algorithm, true);
  DCHECK_LE(matches_without_paths.size(), matches_with_paths.size());

  // It's unnecessary to compare the matches themselves because all
  // |matches_without_paths| should be contained in |matches_with_paths|.
  if (matches_without_paths.size() != matches_with_paths.size()) {
    client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
        OmniboxTriggeredFeatureService::Feature::kBookmarkPaths);
  }

  return match_paths ? matches_with_paths : matches_without_paths;
}

query_parser::MatchingAlgorithm BookmarkProvider::GetMatchingAlgorithm(
    AutocompleteInput input) {
  // TODO(yoangela): This might have to check whether we're in @bookmarks mode
  //  specifically, since we might still get bookmarks suggestions in
  //  non-bookmarks keyword mode. This is enough of an edge case it makes sense
  //  to just stick with simplicity for now.
  if (OmniboxFieldTrial::IsShortBookmarkSuggestionsEnabled() ||
      (OmniboxFieldTrial::IsSiteSearchStarterPackEnabled() &&
       InKeywordMode(input))) {
    return query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH;
  }

  if (OmniboxFieldTrial::
          IsShortBookmarkSuggestionsByTotalInputLengthEnabled() &&
      input.text().length() >=
          OmniboxFieldTrial::
              ShortBookmarkSuggestionsByTotalInputLengthThreshold()) {
    client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
        OmniboxTriggeredFeatureService::Feature::
            kShortBookmarkSuggestionsByTotalInputLength);
    return OmniboxFieldTrial::
                   kShortBookmarkSuggestionsByTotalInputLengthCounterfactual
                       .Get()
               ? query_parser::MatchingAlgorithm::DEFAULT
               : query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH;
  }

  return query_parser::MatchingAlgorithm::DEFAULT;
}

std::pair<int, int> BookmarkProvider::CalculateBookmarkMatchRelevance(
    const TitledUrlMatch& bookmark_match) const {
  // Summary on how a relevance score is determined for the match:
  //
  // For each match within the bookmark's title or URL (or both), calculate a
  // 'factor', sum up those factors, then use the sum to figure out a value
  // between the base score and the maximum score.
  //
  // The factor for each match is the product of:
  //
  //  1) how many characters in the bookmark's title/URL are part of this match.
  //     This is capped at the length of the bookmark's title to prevent terms
  //     that match in both the title and the URL from scoring too strongly.
  //
  //  2) where the match occurs within the bookmark's title or URL, giving more
  //     points for matches that appear earlier in the string:
  //     ((string_length - position of match start) / string_length).
  //
  //  Example: Given a bookmark title of 'abcde fghijklm', with a title length
  //  of 14, and two different search terms, 'abcde' and 'fghij', with start
  //  positions of 0 and 6, respectively, 'abcde' will score higher (with a
  //  partial factor of (14-0)/14 = 1.000 ) than 'fghij' (with a partial factor
  //  of (14-6)/14 = 0.571 ). (In this example neither term matches in the URL.)
  //
  // Once all match factors have been calculated they are summed. If there are
  // no URL matches, the resulting sum will never be greater than the length of
  // the bookmark title because of the way the bookmark model matches and
  // removes overlaps. (In particular, the bookmark model only matches terms to
  // the beginning of words, and it removes all overlapping matches, keeping
  // only the longest. Together these mean that each character is included in at
  // most one match.)  If there are matches in the URL, the sum can be greater.
  //
  // This sum is then normalized by the length of the bookmark title + 10 and
  // capped at 1.0. The +10 is to expand the scoring range so fewer bookmarks
  // will hit the 1.0 cap and hence lose all ability to distinguish between
  // these high-quality bookmarks.
  //
  // The normalized value is multiplied against the scoring range available,
  // which is the difference between the minimum possible score and the maximum
  // possible score. This product is added to the minimum possible score to give
  // the preliminary score.
  //
  // If the preliminary score is less than the maximum possible score, 1199, it
  // can be boosted up to that maximum possible score if the URL referenced by
  // the bookmark is also referenced by any of the user's other bookmarks. A
  // count of how many times the bookmark's URL is referenced is determined and,
  // for each additional reference beyond the one for the bookmark being scored
  // up to a maximum of three, the score is boosted by a fixed amount given by
  // `kURLCountBoost`, below.

  size_t title_length = bookmark_match.node->GetTitledUrlNodeTitle().length();
  size_t url_length =
      bookmark_match.node->GetTitledUrlNodeUrl().spec().length();
  size_t length = title_length > 0 ? title_length : url_length;

  ScoringFunctor title_position_functor = for_each(
      bookmark_match.title_match_positions.begin(),
      bookmark_match.title_match_positions.end(), ScoringFunctor(title_length));
  ScoringFunctor url_position_functor = for_each(
      bookmark_match.url_match_positions.begin(),
      bookmark_match.url_match_positions.end(), ScoringFunctor(url_length));
  const double title_match_strength = title_position_functor.ScoringFactor();
  const double summed_factors =
      title_match_strength + url_position_functor.ScoringFactor();
  const double normalized_sum = std::min(summed_factors / (length + 10), 1.0);

  // Bookmarks with javascript scheme ("bookmarklets") that do not have title
  // matches get a lower base and lower maximum score because returning them
  // for matches in their (often very long) URL looks stupid and is often not
  // intended by the user.
  const GURL& url(bookmark_match.node->GetTitledUrlNodeUrl());
  const bool bookmarklet_without_title_match =
      url.SchemeIs(url::kJavaScriptScheme) && title_match_strength == 0.0;
  const int kBaseBookmarkScore = bookmarklet_without_title_match ? 400 : 900;
  const int kMaxBookmarkScore = bookmarklet_without_title_match ? 799 : 1199;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkScore);
  int relevance = static_cast<int>(normalized_sum * kBookmarkScoreRange) +
                  kBaseBookmarkScore;

  // If scoring signal logging is disabled, skip counting bookmarks if relevance
  // is above max score. Don't waste any time searching for additional
  // referenced URLs if we already have a perfect title match. Returns a pair of
  // the relevance score and -1 as a dummy bookmark count.
  if (!OmniboxFieldTrial::IsLogUrlScoringSignalsEnabled() &&
      relevance >= kMaxBookmarkScore) {
    return {relevance, /*bookmark_count=*/-1};
  }

  // Boost the score if the bookmark's URL is referenced by other bookmarks.
  const int kURLCountBoost[4] = {0, 75, 125, 150};
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(url, &nodes);
  DCHECK_GE(std::min(std::size(kURLCountBoost), nodes.size()), 1U);
  relevance +=
      kURLCountBoost[std::min(std::size(kURLCountBoost), nodes.size()) - 1];
  relevance = std::min(kMaxBookmarkScore, relevance);
  return {relevance, nodes.size()};
}

void BookmarkProvider::RemoveQueryParamKeyMatches(TitledUrlMatch& match) {
  const GURL& url = match.node->GetTitledUrlNodeUrl();
  if (!url.has_query())
    return;

  // Remove any matches that are for query param keys. Since bookmark provider
  // match positions are always at the beginning of words, we can just look at
  // the preceding character for a '?' or '&' character.
  base::EraseIf(match.url_match_positions,
                [url](TitledUrlMatch::MatchPosition& position) {
                  size_t query_begin =
                      url.parsed_for_possibly_invalid_spec().query.begin;
                  return ((query_begin <= position.first) &&
                          (url.spec().at(position.first - 1) == '?' ||
                           url.spec().at(position.first - 1) == '&'));
                });
}
