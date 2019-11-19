// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/bookmark_provider.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/snippet.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/url_constants.h"

using bookmarks::BookmarkNode;
using bookmarks::TitledUrlMatch;
using TitledUrlMatches = std::vector<TitledUrlMatch>;

// BookmarkProvider ------------------------------------------------------------

BookmarkProvider::BookmarkProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BOOKMARK),
      client_(client),
      bookmark_model_(client ? client_->GetBookmarkModel() : nullptr) {}

void BookmarkProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  TRACE_EVENT0("omnibox", "BookmarkProvider::Start");
  matches_.clear();

  if (input.from_omnibox_focus() || input.text().empty())
    return;

  DoAutocomplete(input);
}

BookmarkProvider::~BookmarkProvider() {}

void BookmarkProvider::DoAutocomplete(const AutocompleteInput& input) {
  // We may not have a bookmark model for some unit tests.
  if (!bookmark_model_)
    return;

  TitledUrlMatches matches;
  // Retrieve enough bookmarks so that we have a reasonable probability of
  // suggesting the one that the user desires.
  const size_t kMaxBookmarkMatches = 50;

  // GetBookmarksMatching returns bookmarks matching the user's
  // search terms using the following rules:
  //  - The search text is broken up into search terms. Each term is searched
  //    for separately.
  //  - Term matches are always performed against the start of a word. 'def'
  //    will match against 'define' but not against 'indefinite'.
  //  - Terms must be at least three characters in length in order to perform
  //    partial word matches. Any term of lesser length will only be used as an
  //    exact match. 'def' will match against 'define' but 'de' will not match.
  //    (Unless IsShortBookmarkSuggestionsEnabled is true.)
  //  - A search containing multiple terms will return results with those words
  //    occurring in any order.
  //  - Terms enclosed in quotes comprises a phrase that must match exactly.
  //  - Multiple terms enclosed in quotes will require those exact words in that
  //    exact order to match.
  //
  // Please refer to the code for TitledUrlIndex::GetResultsMatching for
  // complete details of how searches are performed against the user's
  // bookmarks.
  bookmark_model_->GetBookmarksMatching(
      input.text(), kMaxBookmarkMatches,
      OmniboxFieldTrial::IsShortBookmarkSuggestionsEnabled()
          ? query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH
          : query_parser::MatchingAlgorithm::DEFAULT,
      &matches);
  if (matches.empty())
    return;  // There were no matches.
  const base::string16 fixed_up_input(FixupUserInput(input).second);
  for (const auto& bookmark_match : matches) {
    // Score the TitledUrlMatch. If its score is greater than 0 then the
    // AutocompleteMatch is created and added to matches_.
    int relevance = CalculateBookmarkMatchRelevance(bookmark_match);
    if (relevance > 0) {
      matches_.push_back(TitledUrlMatchToAutocompleteMatch(
          bookmark_match, AutocompleteMatchType::BOOKMARK_TITLE, relevance,
          this, client_->GetSchemeClassifier(), input, fixed_up_input));
    }
  }

  // Sort and clip the resulting matches.
  size_t num_matches = std::min(matches_.size(), provider_max_matches_);
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), AutocompleteMatch::MoreRelevant);
  matches_.resize(num_matches);
}

namespace {

// for_each helper functor that calculates a match factor for each query term
// when calculating the final score.
//
// Calculate a 'factor' from 0 to the bookmark's title length for a match
// based on 1) how many characters match and 2) where the match is positioned.
class ScoringFunctor {
 public:
  // |title_length| is the length of the bookmark title against which this
  // match will be scored.
  explicit ScoringFunctor(size_t title_length)
      : title_length_(static_cast<double>(title_length)),
        scoring_factor_(0.0) {
  }

  void operator()(const query_parser::Snippet::MatchPosition& match) {
    double term_length = static_cast<double>(match.second - match.first);
    scoring_factor_ += term_length *
        (title_length_ - match.first) / title_length_;
  }

  double ScoringFactor() { return scoring_factor_; }

 private:
  double title_length_;
  double scoring_factor_;
};

}  // namespace

int BookmarkProvider::CalculateBookmarkMatchRelevance(
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
  //     This is capped at the length of the bookmark's title
  //     to prevent terms that match in both the title and the URL from
  //     scoring too strongly.
  //
  //  2) where the match occurs within the bookmark's title or URL,
  //     giving more points for matches that appear earlier in the string:
  //       ((string_length - position of match start) / string_length).
  //
  //  Example: Given a bookmark title of 'abcde fghijklm', with a title length
  //     of 14, and two different search terms, 'abcde' and 'fghij', with
  //     start positions of 0 and 6, respectively, 'abcde' will score higher
  //     (with a a partial factor of (14-0)/14 = 1.000 ) than 'fghij' (with
  //     a partial factor of (14-6)/14 = 0.571 ).  (In this example neither
  //     term matches in the URL.)
  //
  // Once all match factors have been calculated they are summed.  If there
  // are no URL matches, the resulting sum will never be greater than the
  // length of the bookmark title because of the way the bookmark model matches
  // and removes overlaps.  (In particular, the bookmark model only
  // matches terms to the beginning of words and it removes all overlapping
  // matches, keeping only the longest.  Together these mean that each
  // character is included in at most one match.)  If there are matches in the
  // URL, the sum can be greater.
  //
  // This sum is then normalized by the length of the bookmark title + 10
  // and capped at 1.0.  The +10 is to expand the scoring range so fewer
  // bookmarks will hit the 1.0 cap and hence lose all ability to distinguish
  // between these high-quality bookmarks.
  //
  // The normalized value is multiplied against the scoring range available,
  // which is the difference between the minimum possible score and the maximum
  // possible score.  This product is added to the minimum possible score to
  // give the preliminary score.
  //
  // If the preliminary score is less than the maximum possible score, 1199,
  // it can be boosted up to that maximum possible score if the URL referenced
  // by the bookmark is also referenced by any of the user's other bookmarks.
  // A count of how many times the bookmark's URL is referenced is determined
  // and, for each additional reference beyond the one for the bookmark being
  // scored up to a maximum of three, the score is boosted by a fixed amount
  // given by |kURLCountBoost|, below.

  base::string16 title(bookmark_match.node->GetTitledUrlNodeTitle());
  const GURL& url(bookmark_match.node->GetTitledUrlNodeUrl());

  // Pretend empty titles are identical to the URL.
  if (title.empty())
    title = base::ASCIIToUTF16(url.spec());
  ScoringFunctor title_position_functor =
      for_each(bookmark_match.title_match_positions.begin(),
               bookmark_match.title_match_positions.end(),
               ScoringFunctor(title.size()));
  ScoringFunctor url_position_functor =
      for_each(bookmark_match.url_match_positions.begin(),
               bookmark_match.url_match_positions.end(),
               ScoringFunctor(
                   bookmark_match.node->GetTitledUrlNodeUrl().spec().length()));
  const double title_match_strength = title_position_functor.ScoringFactor();
  const double summed_factors = title_match_strength +
      url_position_functor.ScoringFactor();
  const double normalized_sum =
      std::min(summed_factors / (title.size() + 10), 1.0);
  // Bookmarks with javascript scheme ("bookmarklets") that do not have title
  // matches get a lower base and lower maximum score because returning them
  // for matches in their (often very long) URL looks stupid and is often not
  // intended by the user.
  const bool bookmarklet_without_title_match =
      url.SchemeIs(url::kJavaScriptScheme) && (title_match_strength == 0.0);
  const int kBaseBookmarkScore = bookmarklet_without_title_match ? 400 : 900;
  const int kMaxBookmarkScore = bookmarklet_without_title_match ? 799 : 1199;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkScore);
  int relevance = static_cast<int>(normalized_sum * kBookmarkScoreRange) +
                  kBaseBookmarkScore;
  // Don't waste any time searching for additional referenced URLs if we
  // already have a perfect title match.
  if (relevance >= kMaxBookmarkScore)
    return relevance;
  // Boost the score if the bookmark's URL is referenced by other bookmarks.
  const int kURLCountBoost[4] = { 0, 75, 125, 150 };
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(url, &nodes);
  DCHECK_GE(std::min(base::size(kURLCountBoost), nodes.size()), 1U);
  relevance +=
      kURLCountBoost[std::min(base::size(kURLCountBoost), nodes.size()) - 1];
  relevance = std::min(kMaxBookmarkScore, relevance);
  return relevance;
}
