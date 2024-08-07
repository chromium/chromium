// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/scored_history_match.h"

#include <math.h>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

// The number of days of recency scores to precompute.
const int kDaysToPrecomputeRecencyScoresFor = 366;

// The number of raw term score buckets use; raw term scores greater this are
// capped at the score of the largest bucket.
const int kMaxRawTermScore = 30;

// Pre-computed information to speed up calculating recency scores.
// |days_ago_to_recency_score| is a simple array mapping how long ago a page was
// visited (in days) to the recency score we should assign it.  This allows easy
// lookups of scores without requiring math. This is initialized by
// InitDaysAgoToRecencyScoreArray called by
// ScoredHistoryMatch::Init().
std::array<float, kDaysToPrecomputeRecencyScoresFor> days_ago_to_recency_score;

// Pre-computed information to speed up calculating topicality scores.
// |raw_term_score_to_topicality_score| is a simple array mapping how raw terms
// scores (a weighted sum of the number of hits for the term, weighted by how
// important the hit is: hostname, path, etc.) to the topicality score we should
// assign it.  This allows easy lookups of scores without requiring math. This
// is initialized by InitRawTermScoreToTopicalityScoreArray() called from
// ScoredHistoryMatch::Init().
std::array<float, kMaxRawTermScore> raw_term_score_to_topicality_score;

// Precalculates raw_term_score_to_topicality_score, used in
// GetTopicalityScore().
void InitRawTermScoreToTopicalityScoreArray() {
  for (int term_score = 0; term_score < kMaxRawTermScore; ++term_score) {
    float topicality_score;
    if (term_score < 10) {
      // If the term scores less than 10 points (no full-credit hit, or
      // no combination of hits that score that well), then the topicality
      // score is linear in the term score.
      topicality_score = 0.1 * term_score;
    } else {
      // For term scores of at least ten points, pass them through a log
      // function so a score of 10 points gets a 1.0 (to meet up exactly
      // with the linear component) and increases logarithmically until
      // maxing out at 30 points, with computes to a score around 2.1.
      topicality_score = (1.0 + 2.25 * log10(0.1 * term_score));
    }
    raw_term_score_to_topicality_score[term_score] = topicality_score;
  }
}

// Pre-calculates days_ago_to_recency_score, used in GetRecencyScore().
void InitDaysAgoToRecencyScoreArray() {
  for (int days_ago = 0; days_ago < kDaysToPrecomputeRecencyScoresFor;
       days_ago++) {
    int unnormalized_recency_score;
    if (days_ago <= 4) {
      unnormalized_recency_score = 100;
    } else if (days_ago <= 14) {
      // Linearly extrapolate between 4 and 14 days so 14 days has a score
      // of 70.
      unnormalized_recency_score = 70 + (14 - days_ago) * (100 - 70) / (14 - 4);
    } else if (days_ago <= 31) {
      // Linearly extrapolate between 14 and 31 days so 31 days has a score
      // of 50.
      unnormalized_recency_score = 50 + (31 - days_ago) * (70 - 50) / (31 - 14);
    } else if (days_ago <= 90) {
      // Linearly extrapolate between 30 and 90 days so 90 days has a score
      // of 30.
      unnormalized_recency_score = 30 + (90 - days_ago) * (50 - 30) / (90 - 30);
    } else {
      // Linearly extrapolate between 90 and 365 days so 365 days has a score
      // of 10.
      unnormalized_recency_score =
          10 + (365 - days_ago) * (20 - 10) / (365 - 90);
    }
    days_ago_to_recency_score[days_ago] = unnormalized_recency_score / 100.0;
    if (days_ago > 0) {
      DCHECK_LE(days_ago_to_recency_score[days_ago],
                days_ago_to_recency_score[days_ago - 1]);
    }
  }
}

}  // namespace

// static
bool ScoredHistoryMatch::also_do_hup_like_scoring_;
float ScoredHistoryMatch::bookmark_value_;
float ScoredHistoryMatch::typed_value_;
size_t ScoredHistoryMatch::max_visits_to_score_;
bool ScoredHistoryMatch::allow_tld_matches_;
bool ScoredHistoryMatch::allow_scheme_matches_;
size_t ScoredHistoryMatch::num_title_words_to_allow_;
float ScoredHistoryMatch::topicality_threshold_;
ScoredHistoryMatch::ScoreMaxRelevances*
    ScoredHistoryMatch::relevance_buckets_override_ = nullptr;
OmniboxFieldTrial::NumMatchesScores*
    ScoredHistoryMatch::matches_to_specificity_override_ = nullptr;

ScoredHistoryMatch::ScoredHistoryMatch()
    : ScoredHistoryMatch(history::URLRow(),
                         VisitInfoVector(),
                         std::u16string(),
                         String16Vector(),
                         WordStarts(),
                         RowWordStarts(),
                         false,
                         1,
                         false,
                         base::Time::Max()) {}

ScoredHistoryMatch::ScoredHistoryMatch(
    const history::URLRow& row,
    const VisitInfoVector& visits,
    const std::u16string& lower_string,
    const String16Vector& terms_vector,
    const WordStarts& terms_to_word_starts_offsets,
    const RowWordStarts& word_starts,
    bool is_url_bookmarked,
    size_t num_matching_pages,
    bool is_highly_visited_host,
    base::Time now) {
  // Initialize HistoryMatch fields. TODO(tommycli): Merge these two classes.
  url_info = row;
  input_location = 0;
  match_in_scheme = false;
  match_in_subdomain = false;
  innermost_match = false;

  // NOTE: Call Init() before doing any validity checking to ensure that the
  // class is always initialized after an instance has been constructed. In
  // particular, this ensures that the class is initialized after an instance
  // has been constructed via the no-args constructor.
  ScoredHistoryMatch::Init();

  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled()) {
    // Populate the scoring signals available in the URL Row.
    scoring_signals = std::make_optional<ScoringSignals>();
    scoring_signals->set_typed_count(row.typed_count());
    scoring_signals->set_visit_count(row.visit_count());
    base::TimeDelta elapsed_time = now - row.last_visit();
    scoring_signals->set_elapsed_time_last_visit_secs(elapsed_time.InSeconds());
    scoring_signals->set_is_host_only(IsHostOnly());
    scoring_signals->set_length_of_url(row.url().spec().length());
  }

  // Figure out where each search term appears in the URL and/or page title
  // so that we can score as well as provide autocomplete highlighting.
  base::OffsetAdjuster::Adjustments adjustments;
  GURL gurl = row.url();
  std::u16string cleaned_up_url_for_matching =
      bookmarks::CleanUpUrlForMatching(gurl, &adjustments);
  std::u16string title = bookmarks::CleanUpTitleForMatching(row.title());
  int term_num = 0;
  for (const auto& term : terms_vector) {
    TermMatches url_term_matches =
        MatchTermInString(term, cleaned_up_url_for_matching, term_num);
    TermMatches title_term_matches = MatchTermInString(term, title, term_num);
    if (url_term_matches.empty() && title_term_matches.empty()) {
      // A term was not found in either URL or title - reject.
      return;
    }
    url_matches.insert(url_matches.end(), url_term_matches.begin(),
                       url_term_matches.end());
    title_matches.insert(title_matches.end(), title_term_matches.begin(),
                         title_term_matches.end());
    ++term_num;
  }

  // Sort matches by offset, which is needed for GetTopicalityScore() to
  // function correctly.
  url_matches = SortMatches(url_matches);
  title_matches = SortMatches(title_matches);

  // We can likely inline autocomplete a match if:
  //  1) there is only one search term
  //  2) AND the match begins immediately after one of the prefixes in
  //     URLPrefix such as http://www and https:// (note that one of these
  //     is the empty prefix, for cases where the user has typed the scheme)
  //  3) AND the search string does not end in whitespace (making it look to
  //     the IMUI as though there is a single search term when actually there
  //     is a second, empty term).
  // |best_inlineable_prefix| stores the inlineable prefix computed in
  // clause (2) or NULL if no such prefix exists.  (The URL is not inlineable.)
  // Note that using the best prefix here means that when multiple
  // prefixes match, we'll choose to inline following the longest one.
  // For a URL like "http://www.washingtonmutual.com", this means
  // typing "w" will inline "ashington..." instead of "ww.washington...".
  // We cannot be sure about inlining at this stage because this test depends
  // on the cleaned up URL, which is not necessarily the same as the URL string
  // used in HistoryQuick provider to construct the match.  For instance, the
  // cleaned up URL has special characters unescaped so as to allow matches
  // with them.  This aren't unescaped when HistoryQuick displays the URL;
  // hence a match in the URL that involves matching the unescaped special
  // characters may not be able to be inlined given how HistoryQuick displays
  // it.  This happens in HistoryQuickProvider::QuickMatchToACMatch().
  bool likely_can_inline = false;
  if (!url_matches.empty() && (terms_vector.size() == 1) &&
      !base::IsUnicodeWhitespace(*lower_string.rbegin())) {
    const std::u16string gurl_spec = base::UTF8ToUTF16(gurl.spec());
    const URLPrefix* best_inlineable_prefix =
        URLPrefix::BestURLPrefix(gurl_spec, terms_vector[0]);
    if (best_inlineable_prefix) {
      // When inline autocompleting this match, we're going to use the part of
      // the URL following the end of the matching text.  However, it's possible
      // that FormatUrl(), when formatting this suggestion for display,
      // mucks with the text.  We need to ensure that the text we're thinking
      // about highlighting isn't in the middle of a mucked sequence.  In
      // particular, for the omnibox input of "x" or "xn", we may get a match
      // in a punycoded domain name such as http://www.xn--blahblah.com/.
      // When FormatUrl() processes the xn--blahblah part of the hostname, it'll
      // transform the whole thing into a series of unicode characters.  It's
      // impossible to give the user an inline autocompletion of the text
      // following "x" or "xn" in this case because those characters no longer
      // exist in the displayed URL string.
      size_t offset =
          best_inlineable_prefix->prefix.length() + terms_vector[0].length();
      base::OffsetAdjuster::UnadjustOffset(adjustments, &offset);
      if (offset != std::u16string::npos) {
        // Initialize innermost_match.
        // The idea here is that matches that occur in the scheme or
        // "www." are worse than matches which don't.  For the URLs
        // "http://www.google.com" and "http://wellsfargo.com", we want
        // the omnibox input "w" to cause the latter URL to rank higher
        // than the former.  Note that this is not the same as checking
        // whether one match's inlinable prefix has more components than
        // the other match's, since in this example, both matches would
        // have an inlinable prefix of "http://", which is one component.
        //
        // Instead, we look for the overall best (i.e., most components)
        // prefix of the current URL, and then check whether the inlinable
        // prefix has that many components.  If it does, this is an
        // "innermost" match, and should be boosted.  In the example
        // above, the best prefixes for the two URLs have two and one
        // components respectively, while the inlinable prefixes each
        // have one component; this means the first match is not innermost
        // and the second match is innermost, resulting in us boosting the
        // second match.
        //
        // Now, the code that implements this.
        // The deepest prefix for this URL regardless of where the match is.
        const URLPrefix* best_prefix =
            URLPrefix::BestURLPrefix(gurl_spec, std::u16string());
        DCHECK(best_prefix);
        // If the URL is likely to be inlineable, we must have a match.  Note
        // the prefix that makes it inlineable may be empty.
        likely_can_inline = true;
        innermost_match = (best_inlineable_prefix->num_components ==
                           best_prefix->num_components);
      }
    }
  }

  // Calculate the score per `topicality_score`, `frequency_score`, and
  // `specificity_score`.
  const float topicality_score =
      GetTopicalityScore(terms_vector.size(), gurl, adjustments,
                         terms_to_word_starts_offsets, word_starts);
  float frequency_score = 0, specificity_score = 0;
  if (topicality_score > 0) {
    // No need to calculate these intermediates; when `topicality_score` is 0,
    // they'll be unused.
    frequency_score = GetFrequency(now, is_url_bookmarked, visits);
    specificity_score = GetDocumentSpecificityScore(num_matching_pages);
  }
  raw_score_before_domain_boosting =
      base::saturated_cast<int>(GetFinalRelevancyScore(
          topicality_score, frequency_score, specificity_score, 1));

  // Calculate the score considering `domain_score` as well (if enabled).
  static float domain_suggestions_score_factor =
      OmniboxFieldTrial::kDomainSuggestionsScoreFactor.Get();
  DCHECK_GE(domain_suggestions_score_factor, 1);
  const float domain_score =
      is_highly_visited_host ? domain_suggestions_score_factor : 1;
  raw_score_after_domain_boosting =
      domain_score > 1 ? base::saturated_cast<int>(GetFinalRelevancyScore(
                             topicality_score, frequency_score,
                             specificity_score, domain_score))
                       : raw_score_before_domain_boosting;
  DCHECK(domain_score > 1 ? raw_score_before_domain_boosting <=
                                raw_score_after_domain_boosting
                          : raw_score_before_domain_boosting ==
                                raw_score_after_domain_boosting);

  // Calculate the score using an alternative domain scoring (if enabled).
  static bool domain_suggestions_alternative_scoring =
      OmniboxFieldTrial::kDomainSuggestionsAlternativeScoring.Get();
  if (is_highly_visited_host && domain_suggestions_alternative_scoring) {
    raw_score_after_domain_boosting =
        std::max(GetDomainRelevancyScore(now), raw_score_after_domain_boosting);
  }

  // If the domain suggestions feature is CF enabled, use the un-boosted score;
  // if non-CF enabled, use the boosted score; and if disabled, it doesn't
  // matter as the scores are equal.
  static const bool domain_suggestions_counterfactual =
      OmniboxFieldTrial::kDomainSuggestionsCounterfactual.Get();
  raw_score = domain_suggestions_counterfactual
                  ? raw_score_before_domain_boosting
                  : raw_score_after_domain_boosting;

  if (also_do_hup_like_scoring_ && likely_can_inline) {
    // HistoryURL-provider-like scoring gives any match that is
    // capable of being inlined a certain minimum score.  Some of these
    // are given a higher score that lets them be shown in inline.
    // This test here derives from the test in
    // HistoryURLProvider::PromoteMatchForInlineAutocomplete().
    const bool promote_to_inline =
        (row.typed_count() > 1) || (IsHostOnly() && (row.typed_count() == 1));
    int hup_like_score =
        promote_to_inline
            ? HistoryURLProvider::kScoreForBestInlineableResult
            : HistoryURLProvider::kBaseScoreForNonInlineableResult;

    // Also, if the user types the hostname of a host with a typed
    // visit, then everything from that host get given inlineable scores
    // (because the URL-that-you-typed will go first and everything
    // else will be assigned one minus the previous score, as coded
    // at the end of HistoryURLProvider::DoAutocomplete().
    if (base::UTF8ToUTF16(gurl.host()) == terms_vector[0])
      hup_like_score = HistoryURLProvider::kScoreForBestInlineableResult;

    // HistoryURLProvider has the function PromoteOrCreateShorterSuggestion()
    // that's meant to promote prefixes of the best match (if they've
    // been visited enough related to the best match) or
    // create/promote host-only suggestions (even if they've never
    // been typed).  The code is complicated and we don't try to
    // duplicate the logic here.  Instead, we handle a simple case: in
    // low-typed-count ranges, give host-only matches (i.e.,
    // http://www.foo.com/ vs. http://www.foo.com/bar.html) a boost so
    // that the host-only match outscores all the other matches that
    // would normally have the same base score.  This behavior is not
    // identical to what happens in HistoryURLProvider even in these
    // low typed count ranges--sometimes it will create/promote when
    // this test does not (indeed, we cannot create matches like HUP
    // can) and vice versa--but the underlying philosophy is similar.
    if (!promote_to_inline && IsHostOnly())
      hup_like_score++;

    // All the other logic to goes into hup-like-scoring happens in
    // the tie-breaker case of MatchScoreGreater().

    // Incorporate hup_like_score into raw_score.
    raw_score = std::max(raw_score, hup_like_score);
  }

  url_matches = DeoverlapMatches(url_matches);
  title_matches = DeoverlapMatches(title_matches);

  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets = OffsetsFromTermMatches(url_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches = ReplaceOffsetsInTermMatches(url_matches, offsets);

  // Now that url_matches contains the unadjusted offsets referring to the
  // original URL, we can calculate which components the matches are for.
  std::vector<AutocompleteMatch::MatchPosition> match_positions;
  for (auto& url_match : url_matches) {
    match_positions.push_back(
        std::make_pair(url_match.offset, url_match.offset + url_match.length));
  }
  AutocompleteMatch::GetMatchComponents(gurl, match_positions, &match_in_scheme,
                                        &match_in_subdomain);
}

ScoredHistoryMatch::ScoredHistoryMatch(const ScoredHistoryMatch& other) =
    default;
ScoredHistoryMatch::ScoredHistoryMatch(ScoredHistoryMatch&& other) = default;
ScoredHistoryMatch& ScoredHistoryMatch::operator=(
    const ScoredHistoryMatch& other) = default;
ScoredHistoryMatch& ScoredHistoryMatch::operator=(ScoredHistoryMatch&& other) =
    default;
ScoredHistoryMatch::~ScoredHistoryMatch() = default;

// Comparison function for sorting ScoredMatches by their scores with
// intelligent tie-breaking.
bool ScoredHistoryMatch::MatchScoreGreater(const ScoredHistoryMatch& m1,
                                           const ScoredHistoryMatch& m2) {
  if (m1.raw_score != m2.raw_score)
    return m1.raw_score > m2.raw_score;

  // This tie-breaking logic is inspired by / largely copied from the
  // ordering logic in history_url_provider.cc CompareHistoryMatch().

  // A URL that has been typed at all is better than one that has never been
  // typed.  (Note "!"s on each side.)
  if (!m1.url_info.typed_count() != !m2.url_info.typed_count())
    return m1.url_info.typed_count() > m2.url_info.typed_count();

  // Innermost matches (matches after any scheme or "www.") are better than
  // non-innermost matches.
  if (m1.innermost_match != m2.innermost_match)
    return m1.innermost_match;

  // URLs that have been typed more often are better.
  if (m1.url_info.typed_count() != m2.url_info.typed_count())
    return m1.url_info.typed_count() > m2.url_info.typed_count();

  // For URLs that have each been typed once, a host (alone) is better
  // than a page inside.
  if (m1.url_info.typed_count() == 1) {
    if (m1.IsHostOnly() != m2.IsHostOnly())
      return m1.IsHostOnly();
  }

  // URLs that have been visited more often are better.
  if (m1.url_info.visit_count() != m2.url_info.visit_count())
    return m1.url_info.visit_count() > m2.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return m1.url_info.last_visit() > m2.url_info.last_visit();
}

// static
TermMatches ScoredHistoryMatch::FilterUrlTermMatches(
    const WordStarts& terms_to_word_starts_offsets,
    const GURL& url,
    const WordStarts& url_word_starts,
    const base::OffsetAdjuster::Adjustments& adjustments,
    const TermMatches& url_matches) {
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  size_t host_pos = parsed.CountCharactersBefore(url::Parsed::HOST, true);
  size_t path_pos = parsed.CountCharactersBefore(url::Parsed::PATH, true);
  size_t query_pos = parsed.CountCharactersBefore(url::Parsed::QUERY, true);
  size_t last_part_of_host_pos =
      url.possibly_invalid_spec().rfind('.', path_pos);

  // |word_starts| and |url_matches| both contain offsets for the cleaned up
  // URL used for matching, so we have to follow those adjustments.
  base::OffsetAdjuster::AdjustOffset(adjustments, &host_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &path_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &query_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &last_part_of_host_pos);

  // Filter all matches not at a word boundary and in the path (or
  // later).
  TermMatches filtered_matches = FilterTermMatchesByWordStarts(
      url_matches, terms_to_word_starts_offsets, url_word_starts, path_pos,
      std::string::npos, true);
  if (url.has_scheme()) {
    // Also filter matches not at a word boundary and in the scheme.
    filtered_matches = FilterTermMatchesByWordStarts(
        filtered_matches, terms_to_word_starts_offsets, url_word_starts, 0,
        host_pos, true);
  }
  return filtered_matches;
}

// static
ScoredHistoryMatch::UrlMatchingSignals
ScoredHistoryMatch::ComputeUrlMatchingSignals(
    const WordStarts& terms_to_word_starts_offsets,
    const GURL& url,
    const WordStarts& url_word_starts,
    const base::OffsetAdjuster::Adjustments& adjustments,
    const TermMatches& url_matches) {
  auto next_word_starts = url_word_starts.begin();
  auto end_word_starts = url_word_starts.end();

  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  size_t host_pos = parsed.CountCharactersBefore(url::Parsed::HOST, true);
  size_t path_pos = parsed.CountCharactersBefore(url::Parsed::PATH, true);
  size_t query_pos = parsed.CountCharactersBefore(url::Parsed::QUERY, true);
  size_t last_part_of_host_pos =
      url.possibly_invalid_spec().rfind('.', path_pos);

  // Get end position for 'www'. Not set if 'www' not exists in host.
  std::optional<size_t> www_end_pos;
  if (base::ToLowerASCII(url.spec().substr(host_pos, 3)).compare("www") == 0) {
    www_end_pos = host_pos + 2;
  }

  // |word_starts| and |url_matches| both contain offsets for the cleaned up
  // URL used for matching, so we have to follow those adjustments.
  base::OffsetAdjuster::AdjustOffset(adjustments, &host_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &path_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &query_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &last_part_of_host_pos);
  if (www_end_pos.has_value()) {
    size_t end_pos = *www_end_pos;
    base::OffsetAdjuster::AdjustOffset(adjustments, &end_pos);
    www_end_pos = end_pos;
  }

  std::optional<bool> host_match_at_word_boundary = std::nullopt;
  std::optional<bool> has_non_scheme_www_match = std::nullopt;
  std::optional<size_t> first_url_match_position = std::nullopt;
  size_t total_url_match_length = 0;
  size_t total_host_match_length = 0;
  size_t total_path_match_length = 0;
  size_t total_query_or_ref_match_length = 0;
  size_t num_input_terms_matched_by_url = 0;

  if (!url_matches.empty()) {
    // URL matches are sorted by offsets.
    first_url_match_position = url_matches[0].offset;
  }
  num_input_terms_matched_by_url = CountUniqueMatchTerms(url_matches);

  for (const auto& url_match : url_matches) {
    // Calculate the offset in the URL string where the meaningful (word) part
    // of the term starts.  This takes into account times when a term starts
    // with punctuation such as "/foo".
    const size_t term_word_offset =
        url_match.offset + terms_to_word_starts_offsets[url_match.term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < term_word_offset)) {
      ++next_word_starts;
    }
    const bool at_word_boundary = (next_word_starts != end_word_starts) &&
                                  (*next_word_starts == term_word_offset);
    if (term_word_offset >= query_pos) {
      // The match is in the query or ref component.
      total_query_or_ref_match_length += url_match.length;
    } else if (term_word_offset >= path_pos) {
      // The match is in the path component.
      total_path_match_length += url_match.length;
    } else if (term_word_offset >= host_pos) {
      if (host_match_at_word_boundary.has_value()) {
        host_match_at_word_boundary =
            *host_match_at_word_boundary || at_word_boundary;
      } else {
        host_match_at_word_boundary = at_word_boundary;
      }
      if (has_non_scheme_www_match.has_value()) {
        has_non_scheme_www_match = *has_non_scheme_www_match ||
                                   !www_end_pos.has_value() ||
                                   (term_word_offset > *www_end_pos);
      } else {
        has_non_scheme_www_match =
            !www_end_pos.has_value() || (term_word_offset > *www_end_pos);
      }
      total_host_match_length += url_match.length;
    }
    total_url_match_length += url_match.length;
  }

  UrlMatchingSignals matching_signals = {
      host_match_at_word_boundary,     has_non_scheme_www_match,
      first_url_match_position,        total_url_match_length,
      total_host_match_length,         total_path_match_length,
      total_query_or_ref_match_length, num_input_terms_matched_by_url};

  return matching_signals;
}

// static
TermMatches ScoredHistoryMatch::FilterTermMatchesByWordStarts(
    const TermMatches& term_matches,
    const WordStarts& terms_to_word_starts_offsets,
    const WordStarts& word_starts,
    size_t start_pos,
    size_t end_pos,
    bool allow_midword_continuations) {
  // Return early if no filtering is needed.
  if (start_pos == std::string::npos)
    return term_matches;
  TermMatches filtered_matches;
  auto next_word_starts = word_starts.begin();
  auto end_word_starts = word_starts.end();
  size_t last_end = 0;
  for (const auto& term_match : term_matches) {
    const size_t term_offset =
        terms_to_word_starts_offsets[term_match.term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < (term_match.offset + term_offset)))
      ++next_word_starts;
    // Add the match if it's (1) before the position we start filtering at, (2)
    // after the position we stop filtering at (assuming we have a position
    // to stop filtering at), (3) at a word boundary, (4) void of words (e.g.
    // the term '-' contains no words), or, (5) if allow_midword_continuations
    // is true, continues where the previous match left off (e.g. inputs such
    // as 'stack overflow' will match text such as 'stackoverflow').
    if (term_match.offset < start_pos ||
        (end_pos != std::string::npos && term_match.offset >= end_pos) ||
        (next_word_starts != end_word_starts &&
         *next_word_starts == term_match.offset + term_offset) ||
        term_offset == term_match.length ||
        (allow_midword_continuations && term_match.offset == last_end)) {
      last_end = term_match.offset + term_match.length;
      filtered_matches.push_back(term_match);
    }
  }
  return filtered_matches;
}

// static
size_t ScoredHistoryMatch::ComputeTotalMatchLength(
    const WordStarts& terms_to_word_starts_offsets,
    const TermMatches& matches,
    const WordStarts& word_starts,
    size_t num_words_to_allow) {
  int total_match_length = 0;
  auto next_word_starts = word_starts.begin();
  auto end_word_starts = word_starts.end();
  size_t word_num = 0;
  for (const auto& match : matches) {
    // Calculate the offset in the title string where the meaningful (word) part
    // of the term starts.  This takes into account times when a term starts
    // with punctuation such as "/foo".
    const size_t term_word_offset =
        match.offset + terms_to_word_starts_offsets[match.term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < term_word_offset)) {
      ++next_word_starts;
      ++word_num;
    }

    // Only count up to the number of allowed words.
    if (word_num >= num_words_to_allow) {
      break;
    }
    total_match_length += match.length;
  }
  return total_match_length;
}

// static
size_t ScoredHistoryMatch::CountUniqueMatchTerms(
    const TermMatches& term_matches) {
  // Find unique `term_num`s in term_matches
  std::set<int> unique_term_nums;
  for (const auto& match : term_matches) {
    unique_term_nums.insert(match.term_num);
  }
  return unique_term_nums.size();
}

// static
void ScoredHistoryMatch::Init() {
  static bool initialized = false;

  if (initialized)
    return;

  initialized = true;
  also_do_hup_like_scoring_ = OmniboxFieldTrial::HQPAlsoDoHUPLikeScoring();
  bookmark_value_ = OmniboxFieldTrial::HQPBookmarkValue();
  typed_value_ = OmniboxFieldTrial::HQPTypedValue();
  max_visits_to_score_ = OmniboxFieldTrial::HQPMaxVisitsToScore();
  allow_tld_matches_ = OmniboxFieldTrial::HQPAllowMatchInTLDValue();
  allow_scheme_matches_ = OmniboxFieldTrial::HQPAllowMatchInSchemeValue();
  num_title_words_to_allow_ = OmniboxFieldTrial::HQPNumTitleWordsToAllow();
  topicality_threshold_ =
      OmniboxFieldTrial::HQPExperimentalTopicalityThreshold();

  InitRawTermScoreToTopicalityScoreArray();
  InitDaysAgoToRecencyScoreArray();
}

float ScoredHistoryMatch::GetTopicalityScore(
    const int num_terms,
    const GURL& url,
    const base::OffsetAdjuster::Adjustments& adjustments,
    const WordStarts& terms_to_word_starts_offsets,
    const RowWordStarts& word_starts) {
  // A vector that accumulates per-term scores.  The strongest match--a
  // match in the hostname at a word boundary--is worth 10 points.
  // Everything else is less.  In general, a match that's not at a word
  // boundary is worth about 1/4th or 1/5th of a match at the word boundary
  // in the same part of the URL/title.
  DCHECK_GT(num_terms, 0);
  std::vector<int> term_scores(num_terms, 0);

  // Process term matches in the URL.
  url_matches = FilterUrlTermMatches(terms_to_word_starts_offsets, url,
                                     word_starts.url_word_starts_, adjustments,
                                     url_matches);
  IncrementUrlMatchTermScores(terms_to_word_starts_offsets, url,
                              word_starts.url_word_starts_, adjustments,
                              &term_scores);

  // Process term matches in the title.
  title_matches = FilterTermMatchesByWordStarts(
      title_matches, terms_to_word_starts_offsets,
      word_starts.title_word_starts_, 0, std::string::npos, true);
  IncrementTitleMatchTermScores(terms_to_word_starts_offsets,
                                word_starts.title_word_starts_, &term_scores);

  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled()) {
    // Url matching signals.
    const auto url_matching_signals = ComputeUrlMatchingSignals(
        terms_to_word_starts_offsets, url, word_starts.url_word_starts_,
        adjustments, url_matches);
    if (url_matching_signals.first_url_match_position.has_value()) {
      // Not set if there is no URL match.
      scoring_signals->set_first_url_match_position(
          *(url_matching_signals.first_url_match_position));
    }
    if (url_matching_signals.host_match_at_word_boundary.has_value()) {
      // Not set if there is no match in the host.
      scoring_signals->set_host_match_at_word_boundary(
          *(url_matching_signals.host_match_at_word_boundary));
      scoring_signals->set_has_non_scheme_www_match(
          *(url_matching_signals.has_non_scheme_www_match));
    }
    scoring_signals->set_total_url_match_length(
        url_matching_signals.total_url_match_length);
    scoring_signals->set_total_host_match_length(
        url_matching_signals.total_host_match_length);
    scoring_signals->set_total_path_match_length(
        url_matching_signals.total_path_match_length);
    scoring_signals->set_total_query_or_ref_match_length(
        url_matching_signals.total_query_or_ref_match_length);
    scoring_signals->set_num_input_terms_matched_by_url(
        url_matching_signals.num_input_terms_matched_by_url);

    // Title matching signals.
    size_t total_title_match_length = ComputeTotalMatchLength(
        terms_to_word_starts_offsets, title_matches,
        word_starts.title_word_starts_, num_title_words_to_allow_);
    scoring_signals->set_total_title_match_length(total_title_match_length);
    scoring_signals->set_num_input_terms_matched_by_title(
        CountUniqueMatchTerms(title_matches));
  }

  // TODO(mpearson): Restore logic for penalizing out-of-order matches.
  // (Perhaps discount them by 0.8?)
  // TODO(mpearson): Consider: if the earliest match occurs late in the string,
  // should we discount it?
  // TODO(mpearson): Consider: do we want to score based on how much of the
  // input string the input covers?  (I'm leaning toward no.)

  // Compute the topicality_score as the sum of transformed term_scores.
  float topicality_score = 0;
  for (int term_score : term_scores) {
    topicality_score += raw_term_score_to_topicality_score[std::min(
        term_score, kMaxRawTermScore - 1)];
  }
  // TODO(mpearson): If there are multiple terms, consider taking the
  // geometric mean of per-term scores rather than the arithmetic mean.

  const float final_topicality_score = topicality_score / num_terms;

  // Demote the URL if the topicality score is less than threshold.
  if (final_topicality_score < topicality_threshold_) {
    return 0.0;
  }

  return final_topicality_score;
}

void ScoredHistoryMatch::IncrementUrlMatchTermScores(
    const WordStarts& terms_to_word_starts_offsets,
    const GURL& url,
    const WordStarts& url_word_starts,
    const base::OffsetAdjuster::Adjustments& adjustments,
    std::vector<int>* term_scores) {
  auto next_word_starts = url_word_starts.begin();
  auto end_word_starts = url_word_starts.end();

  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  size_t host_pos = parsed.CountCharactersBefore(url::Parsed::HOST, true);
  size_t path_pos = parsed.CountCharactersBefore(url::Parsed::PATH, true);
  size_t query_pos = parsed.CountCharactersBefore(url::Parsed::QUERY, true);
  size_t last_part_of_host_pos =
      url.possibly_invalid_spec().rfind('.', path_pos);

  // |word_starts| and |url_matches| both contain offsets for the cleaned up
  // URL used for matching, so we have to follow those adjustments.
  base::OffsetAdjuster::AdjustOffset(adjustments, &host_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &path_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &query_pos);
  base::OffsetAdjuster::AdjustOffset(adjustments, &last_part_of_host_pos);

  for (const auto& url_match : url_matches) {
    // Calculate the offset in the URL string where the meaningful (word) part
    // of the term starts.  This takes into account times when a term starts
    // with punctuation such as "/foo".
    const size_t term_word_offset =
        url_match.offset + terms_to_word_starts_offsets[url_match.term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < term_word_offset)) {
      ++next_word_starts;
    }
    const bool at_word_boundary = (next_word_starts != end_word_starts) &&
                                  (*next_word_starts == term_word_offset);
    if (term_word_offset >= query_pos) {
      // The match is in the query or ref component.
      if (term_scores) {
        (*term_scores)[url_match.term_num] += 5;
      }
    } else if (term_word_offset >= path_pos) {
      // The match is in the path component.
      if (term_scores) {
        (*term_scores)[url_match.term_num] += 8;
      }
    } else if (term_word_offset >= host_pos) {
      if (term_scores) {
        if (term_word_offset < last_part_of_host_pos) {
          // Either there are no dots in the hostname or this match isn't
          // the last dotted component.
          (*term_scores)[url_match.term_num] += at_word_boundary ? 10 : 2;
        } else {
          // The match is in the last part of a dotted hostname (usually this
          // is the top-level domain .com, .net, etc.).
          if (allow_tld_matches_) {
            (*term_scores)[url_match.term_num] += at_word_boundary ? 10 : 0;
          }
        }
      }
    } else {
      // The match is in the protocol (a.k.a. scheme).
      // Matches not at a word boundary should have been filtered already.
      if (allow_scheme_matches_ && term_scores) {
        (*term_scores)[url_match.term_num] += 10;
      }
    }
  }
}

void ScoredHistoryMatch::IncrementTitleMatchTermScores(
    const WordStarts& terms_to_word_starts_offsets,
    const WordStarts& title_word_starts,
    std::vector<int>* term_scores) {
  auto next_word_starts = title_word_starts.begin();
  auto end_word_starts = title_word_starts.end();
  size_t word_num = 0;
  for (const auto& title_match : title_matches) {
    // Calculate the offset in the title string where the meaningful (word) part
    // of the term starts.  This takes into account times when a term starts
    // with punctuation such as "/foo".
    const size_t term_word_offset =
        title_match.offset + terms_to_word_starts_offsets[title_match.term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < term_word_offset)) {
      ++next_word_starts;
      ++word_num;
    }
    if (word_num >= num_title_words_to_allow_) {
      break;  // only count the first ten words
    }
    if (term_scores &&
        term_scores->size() > static_cast<size_t>(title_match.term_num)) {
      (*term_scores)[title_match.term_num] += 8;
    }
  }
}

float ScoredHistoryMatch::GetRecencyScore(int last_visit_days_ago) const {
  // Lookup the score in days_ago_to_recency_score, treating
  // everything older than what we've precomputed as the oldest thing
  // we've precomputed.  The std::max is to protect against corruption
  // in the database (in case last_visit_days_ago is negative).
  return days_ago_to_recency_score[std::max(
      std::min(last_visit_days_ago, kDaysToPrecomputeRecencyScoresFor - 1), 0)];
}

float ScoredHistoryMatch::GetFrequency(const base::Time& now,
                                       const bool bookmarked,
                                       const VisitInfoVector& visits) const {
  // Compute the weighted sum of |value_of_transition| over the last at most
  // |max_visits_to_score_| visits, where each visit is weighted using
  // GetRecencyScore() based on how many days ago it happened.
  //
  // Here are example frequency scores, assuming |max_visits_to_score_| is 10.
  // - a single typed visit more than three months ago, no other visits -> 0.45
  //   ( = 1.5 typed visit score * 0.3 recency score)
  // - a single visit recently -> 1.0
  //   ( = 1.0 visit score * 1.0 recency score)
  // - a single typed visit recently -> 1.5
  //   ( = 1.5 typed visit score * 1.0 recency score)
  // - 10+ visits, once every three days, no typed visits -> about 7.5
  //   ( 10+ visits, averaging about 0.75 recency score each)
  // - 10+ typed visits, once a week -> about 7.5
  //   ( 10+ visits, average of 1.5 typed visit score * 0.5 recency score)
  // - 10+ visit, once every day, no typed visits -> about 9.0
  //   ( 10+ visits, average about 0.9 recency score each)
  // - 10+ typed visit, once every three days -> about 11
  //   ( 10+ visits, averaging about 1.5 typed visit *  0.75 recency score each)
  // - 10+ typed visits today -> 15
  //   ( 10+ visits, each worth 1.5 typed visit score)
  float summed_visit_points = 0;
  auto visits_end =
      visits.begin() + std::min(visits.size(), max_visits_to_score_);
  // Visits should be in newest to oldest order.
  DCHECK(base::ranges::adjacent_find(visits.begin(), visits_end, std::less<>(),
                                     &history::VisitInfo::first) == visits_end);
  for (auto i = visits.begin(); i != visits_end; ++i) {
    const bool is_page_transition_typed =
        ui::PageTransitionCoreTypeIs(i->second, ui::PAGE_TRANSITION_TYPED);
    float value_of_transition = is_page_transition_typed ? typed_value_ : 1;
    if (bookmarked)
      value_of_transition = std::max(value_of_transition, bookmark_value_);
    const float bucket_weight = GetRecencyScore((now - i->first).InDays());
    summed_visit_points += (value_of_transition * bucket_weight);
  }
  return summed_visit_points;
}

float ScoredHistoryMatch::GetDocumentSpecificityScore(
    size_t num_matching_pages) const {
  // A mapping from the number of matching pages to their associated document
  // specificity scores.  See omnibox_field_trial.h for more details.
  static base::NoDestructor<OmniboxFieldTrial::NumMatchesScores>
      default_matches_to_specificity(OmniboxFieldTrial::HQPNumMatchesScores());
  OmniboxFieldTrial::NumMatchesScores* matches_to_specificity =
      matches_to_specificity_override_ ? matches_to_specificity_override_
                                       : default_matches_to_specificity.get();

  // The floating point value below must be less than the lowest score the
  // server would send down.
  OmniboxFieldTrial::NumMatchesScores::const_iterator it = std::upper_bound(
      matches_to_specificity->begin(), matches_to_specificity->end(),
      std::pair<size_t, double>{num_matching_pages, -1});
  return (it != matches_to_specificity->end()) ? it->second : 1.0;
}

// static
float ScoredHistoryMatch::GetFinalRelevancyScore(float topicality_score,
                                                 float frequency_score,
                                                 float specificity_score,
                                                 float domain_score) {
  // |relevance_buckets| gives a mapping from intermediate score to the final
  // relevance score.
  static base::NoDestructor<ScoreMaxRelevances> default_relevance_buckets(
      GetHQPBuckets());
  ScoreMaxRelevances* relevance_buckets = relevance_buckets_override_
                                              ? relevance_buckets_override_
                                              : default_relevance_buckets.get();
  DCHECK(!relevance_buckets->empty());
  DCHECK_EQ(0.0, (*relevance_buckets)[0].first);

  if (topicality_score == 0) {
    DCHECK_EQ(frequency_score, 0);
    DCHECK_EQ(specificity_score, 0);
    return 0;
  }

  // Compute an intermediate score by multiplying the topicality, specificity,
  // and frequency scores, then map it to the range [0, 1399].  For typical
  // ranges, remember:
  // * topicality score is usually around 1.0; typical range is [0.5, 2.5].
  //   1.0 is the value assigned when a single-term input matches in the
  //   hostname.  For more details, see GetTopicalityScore().
  // * specificity score is usually 1.0; typical range is [1.0, 3.0].
  //   1.0 is the default value when the user's input matches many documents.
  //   For more details, see GetDocumentSpecificityScore().
  // * frequency score has a much wider range depending on the number of
  //   visits; typical range is [0.3, 15.0].  For more details, see
  //   GetFrequency().
  //
  // The default scoring buckets: "0.0:550,1:625,9.0:1300,90.0:1399"
  // will linearly interpolate the scores between:
  //      0.0 to 1.0  --> 550 to 625
  //      1.0 to 9.0  --> 625 to 1300
  //      9.0 to 90.0 --> 1300 to 1399
  //      >= 90.0     --> 1399
  //
  // The score maxes out at 1399 (i.e., cannot beat a good inlineable result
  // from HistoryURL provider).
  const float intermediate_score =
      topicality_score * frequency_score * specificity_score * domain_score;

  // Find the threshold where intermediate score is greater than bucket.
  size_t i = 1;
  for (; i < relevance_buckets->size(); ++i) {
    const ScoreMaxRelevance& bucket = (*relevance_buckets)[i];
    if (intermediate_score >= bucket.first) {
      continue;
    }
    const ScoreMaxRelevance& previous_bucket = (*relevance_buckets)[i - 1];
    const float slope = ((bucket.second - previous_bucket.second) /
                         (bucket.first - previous_bucket.first));
    return (previous_bucket.second +
            (slope * (intermediate_score - previous_bucket.first)));
  }
  // It will reach this stage when the score is > highest bucket score.
  // Return the highest bucket score.
  return (*relevance_buckets)[i - 1].second;
}

// static
std::vector<ScoredHistoryMatch::ScoreMaxRelevance>
ScoredHistoryMatch::GetHQPBuckets() {
  std::string relevance_buckets_str =
      OmniboxFieldTrial::HQPExperimentalScoringBuckets();
  static constexpr char kDefaultHQPBuckets[] =
      "0.0:550,1:625,9.0:1300,90.0:1399";
  if (relevance_buckets_str.empty())
    relevance_buckets_str = kDefaultHQPBuckets;
  return GetHQPBucketsFromString(relevance_buckets_str);
}

// static
ScoredHistoryMatch::ScoreMaxRelevances
ScoredHistoryMatch::GetHQPBucketsFromString(const std::string& buckets_str) {
  DCHECK(!buckets_str.empty());
  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(buckets_str, ':', ',', &kv_pairs))
    return ScoreMaxRelevances();
  ScoreMaxRelevances hqp_buckets;
  for (base::StringPairs::const_iterator it = kv_pairs.begin();
       it != kv_pairs.end(); ++it) {
    ScoreMaxRelevance bucket;
    bool is_valid_intermediate_score =
        base::StringToDouble(it->first, &bucket.first);
    DCHECK(is_valid_intermediate_score);
    bool is_valid_hqp_score = base::StringToInt(it->second, &bucket.second);
    DCHECK(is_valid_hqp_score);
    hqp_buckets.push_back(bucket);
  }
  return hqp_buckets;
}

int ScoredHistoryMatch::GetDomainRelevancyScore(base::Time now) const {
  // Domain scores consider only the last visit time as they're intended for
  // pages the user hasn't yet visited many times. The goal is to score them
  // highly enough to surface but not so high they constantly displace
  // traditional suggestions. Otherwise, for inputs matching a highly visited
  // domain, domain suggestions would overwhelm all other suggestions. Besides,
  // if scored conservatively, they'll still be boosted by traditional scores
  // after they're selected.

  // For simplicity, score them linearly: 1000 - 80 / day.
  // 80 because (1000-200) / (10-0) = 80.
  constexpr int max_score = 1000;
  constexpr int min_score = 200;
  constexpr auto demote_start = base::Days(0);
  constexpr auto demote_end = base::Days(10);

  auto elapsed = now - url_info.last_visit();

  // If visited more recently than `demote_start`, return `max_score`.
  if (elapsed <= demote_start)
    return max_score;
  // If visited less recently than `demote_end`, return 0 (not `min_score`).
  if (elapsed >= demote_end)
    return 0;
  // Otherwise, linearly interpolate `max_score` and `min_score`.
  return max_score - (elapsed - demote_start) / (demote_end - demote_start) *
                         (max_score - min_score);
}
