// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/history_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "omnibox_event.pb.h"

class ScoredHistoryMatchTest;

// An HistoryMatch that has a score as well as metrics defining where in the
// history item's URL and/or page title matches have occurred.
struct ScoredHistoryMatch : public history::HistoryMatch {
  // ScoreMaxRelevance maps from an intermediate-score to the maximum
  // final-relevance score given to a URL for this intermediate score.
  // This is used to store the score ranges of relevance buckets.
  // Please see GetFinalRelevancyScore() for details.
  using ScoreMaxRelevance = std::pair<double, int>;

  // A sorted vector of ScoreMaxRelevance entries, used by taking a score and
  // interpolating between consecutive buckets.  See GetFinalRelevancyScore()
  // for details.
  using ScoreMaxRelevances = std::vector<ScoreMaxRelevance>;

  // Required for STL, we don't use this directly.
  ScoredHistoryMatch();
  ScoredHistoryMatch(const ScoredHistoryMatch& other);
  ScoredHistoryMatch(ScoredHistoryMatch&& other);
  ScoredHistoryMatch& operator=(const ScoredHistoryMatch& other);
  ScoredHistoryMatch& operator=(ScoredHistoryMatch&& other);

  // Initializes the ScoredHistoryMatch with a raw score calculated for the
  // history item given in |row| with recent visits as indicated in |visits|. It
  // first determines if the row qualifies by seeing if all of the terms in
  // |terms_vector| occur in |row|.  If so, calculates a raw score.  This raw
  // score is in part determined by whether the matches occur at word
  // boundaries, the locations of which are stored in |word_starts|.  For some
  // terms, it's appropriate to look for the word boundary within the term. For
  // instance, the term ".net" should look for a word boundary at the "n".
  // These offsets (".net" should have an offset of 1) come from
  // |terms_to_word_starts_offsets|. |is_url_bookmarked| indicates whether the
  // match's URL is referenced by any bookmarks, which can also affect the raw
  // score.  |num_matching_pages| indicates how many URLs in the eligible URL
  // database match the user's input; it can also affect the raw score.  The raw
  // score allows the matches to be ordered and can be used to influence the
  // final score calculated by the client of this index.  If the row does not
  // qualify the raw score will be 0.
  ScoredHistoryMatch(const history::URLRow& row,
                     const VisitInfoVector& visits,
                     const std::u16string& lower_string,
                     const String16Vector& terms_vector,
                     const WordStarts& terms_to_word_starts_offsets,
                     const RowWordStarts& word_starts,
                     bool is_url_bookmarked,
                     size_t num_matching_pages,
                     bool is_highly_visited_host,
                     base::Time now);

  ~ScoredHistoryMatch();

  // Compares two matches by score.  Functor supporting URLIndexPrivateData's
  // HistoryItemsForTerms function.  Looks at particular fields within
  // with url_info to make tie-breaking a bit smarter.
  static bool MatchScoreGreater(const ScoredHistoryMatch& m1,
                                const ScoredHistoryMatch& m2);

  // Returns |term_matches| after removing all matches that are not at a
  // word break that are in the range [|start_pos|, |end_pos|).
  // start_pos == string::npos is treated as start_pos = length of string.
  // (In other words, no matches will be filtered.)
  // end_pos == string::npos is treated as end_pos = length of string. If
  // |allow_midword_continuations| is true, matches not at a word break are not
  // filtered if they continue where the previous match ended.
  static TermMatches FilterTermMatchesByWordStarts(
      const TermMatches& term_matches,
      const WordStarts& terms_to_word_starts_offsets,
      const WordStarts& word_starts,
      size_t start_pos,
      size_t end_pos,
      bool allow_midword_continuations = false);

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score;

  // Both these TermMatches contain the set of matches that are considered
  // important.  At this time, that means they exclude mid-word matches
  // except in the hostname of the URL.  (Technically, during early
  // construction of ScoredHistoryMatch, they may contain all matches, but
  // unimportant matches are eliminated by GetTopicalityScore(), called
  // during construction.)

  // Term matches within the URL.
  TermMatches url_matches;
  // Term matches within the page title.
  TermMatches title_matches;

  // Signals used to score matches. These are propagated to the ACController
  // via ACMatch, and used by the ML Scorer as well as logged to
  // OmniboxEventProto in order to provide ML training data.
  metrics::OmniboxEventProto::Suggestion::ScoringSignals scoring_signals;

 private:
  friend class ScoredHistoryMatchTest;
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetDocumentSpecificityScore);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetFinalRelevancyScore);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetFrequency);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetHQPBucketsFromString);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringBookmarks);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringScheme);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringTLD);

  // Initialize ScoredHistoryMatch statics. Must be called before any other
  // method of ScoredHistoryMatch and before creating any instances.
  static void Init();

  // Return a topicality score based on how many matches appear in the url and
  // the page's title and where they are (e.g., at word boundaries).  Revises
  // url_matches and title_matches in the process so they only reflect matches
  // used for scoring.  (For instance, some mid-word matches are not given
  // credit in scoring.)  Requires that `url_matches` and `title_matches` are
  // sorted. `adjustments` must contain any adjustments used to format `url`.
  // Signals used for scoring that are calculated here are also populated in
  // `scoring_signals` in order to provide training data for the ML Scoring
  // model.
  float GetTopicalityScore(const int num_terms,
                           const GURL& url,
                           const base::OffsetAdjuster::Adjustments& adjustments,
                           const WordStarts& terms_to_word_starts_offsets,
                           const RowWordStarts& word_starts);

  // Returns a recency score based on |last_visit_days_ago|, which is
  // how many days ago the page was last visited.
  float GetRecencyScore(int last_visit_days_ago) const;

  // Examines the first |max_visits_to_score_| and returns a score (higher is
  // better) based the rate of visits, whether the page is bookmarked, and
  // how often those visits are typed navigations (i.e., explicitly
  // invoked by the user).  |now| is passed in to avoid unnecessarily
  // recomputing it frequently.
  float GetFrequency(const base::Time& now,
                     const bool bookmarked,
                     const VisitInfoVector& visits) const;

  // Returns a document specificity score based on how many pages matched the
  // user's input.
  float GetDocumentSpecificityScore(size_t num_matching_pages) const;

  // Combines the four component scores into a final score that's an appropriate
  // value to use as a relevancy score.
  static float GetFinalRelevancyScore(float topicality_score,
                                      float frequency_score,
                                      float specificity_score,
                                      float domain_score);

  // Helper function that returns the string containing the scoring buckets
  // (either the default ones or ones specified in an experiment).
  static ScoreMaxRelevances GetHQPBuckets();

  // Helper function to parse the string containing the scoring buckets and
  // return the results.  For example, with |buckets_str| as
  // "0.0:400,1.5:600,12.0:1300,20.0:1399", it returns [(0.0, 400), (1.5, 600),
  // (12.0, 1300), (20.0, 1399)]. It returns an empty vector in the case of a
  // malformed |buckets_str|.
  static ScoreMaxRelevances GetHQPBucketsFromString(
      const std::string& buckets_str);

  // If true, assign raw scores to be max(whatever it normally would be, a
  // score that's similar to the score HistoryURL provider would assign).
  static bool also_do_hup_like_scoring_;

  // Untyped visits to bookmarked pages score this, compared to 1 for
  // untyped visits to non-bookmarked pages and |typed_value_| for typed visits.
  static float bookmark_value_;

  // Typed visits to page score this, compared to 1 for untyped visits.
  static float typed_value_;

  // The maximum number of recent visits to examine in GetFrequency().
  static size_t max_visits_to_score_;

  // If true, we allow input terms to match in the TLD (e.g., ".com").
  static bool allow_tld_matches_;

  // If true, we allow input terms to match in the scheme (e.g., "http://").
  static bool allow_scheme_matches_;

  // The number of title words examined when computing topicality scores.
  // Words beyond this number are ignored.
  static size_t num_title_words_to_allow_;

  // |topicality_threshold_| is used to control the topicality scoring.
  // If |topicality_threshold_| > 0, then URLs with topicality-score less than
  // the threshold are given topicality score of 0.
  static float topicality_threshold_;

  // Used for testing.  A possibly null pointer to a vector.  If set,
  // overrides the static local variable |relevance_buckets| declared in
  // GetFinalRelevancyScore().
  static ScoreMaxRelevances* relevance_buckets_override_;

  // Used for testing.  If this pointer is not null, it overrides the static
  // local variable |default_matches_to_specificity| declared in
  // GetDocumentSpecificityScore().
  static OmniboxFieldTrial::NumMatchesScores* matches_to_specificity_override_;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

#endif  // COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_
