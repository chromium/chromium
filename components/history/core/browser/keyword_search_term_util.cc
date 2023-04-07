// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/keyword_search_term_util.h"

#include <algorithm>

#include "base/time/time.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/keyword_search_term.h"

namespace history {

namespace {

// Calculates the score for the given number of visits in a given day.
// Recent visits count more than historical ones, so multiply in a boost
// depending on how long ago this day was. This boost is a curve that
// smoothly goes through these values: Today gets 3x, a week ago 2x, three
// weeks ago 1.5x, gradually falling off to 1x at the very end.
double GetMostVisitedFrecencyScore(int visit_count,
                                   base::Time day,
                                   base::Time now) {
  double day_score = 1.0 + log(static_cast<double>(visit_count));
  int days_ago = (now - day).InDays();
  double recency_boost = 1.0 + (2.0 * (1.0 / (1.0 + days_ago / 7.0)));
  return recency_boost * day_score;
}

// Returns whether two search terms are identical - i.e., they have the same
// normalized search terms.
bool IsSameSearchTerm(const KeywordSearchTermVisit& lhs,
                      const KeywordSearchTermVisit& rhs) {
  return lhs.normalized_term == rhs.normalized_term;
}

// Return whether a visit to a search term constitutes a duplicative visit -
// i.e., a visit to the same search term in an interval smaller than
// kAutocompleteDuplicateVisitIntervalThreshold.
// Called with identical search terms only. i.e., IsSameSearchTerm() is true.
bool IsDuplicativeVisitToSearchTerm(const KeywordSearchTermVisit& lhs,
                                    const KeywordSearchTermVisit& rhs) {
  DCHECK(IsSameSearchTerm(lhs, rhs));
  return lhs.last_visit_time - rhs.last_visit_time <=
         kAutocompleteDuplicateVisitIntervalThreshold;
}

// Transforms a visit time to its timeslot, i.e., day of the viist.
base::Time VisitTimeToTimeslot(base::Time visit_time) {
  return visit_time.LocalMidnight();
}

// Returns whether two search term visits are in the same timeslot.
// Called with identical search terms only. i.e., IsSameSearchTerm() is true.
bool IsInSameTimeslot(const KeywordSearchTermVisit& lhs,
                      const KeywordSearchTermVisit& rhs) {
  DCHECK(IsSameSearchTerm(lhs, rhs));
  return VisitTimeToTimeslot(lhs.last_visit_time) ==
         VisitTimeToTimeslot(rhs.last_visit_time);
}

}  // namespace

const base::TimeDelta kAutocompleteDuplicateVisitIntervalThreshold =
    base::Minutes(5);

// Returns the frecency score of the visit based on the following formula:
//            (frequency ^ kFrequencyExponent) * kRecencyDecayUnitSec
// frecency = ————————————————————————————————————————————————————————————————
//                   recency_in_seconds + kRecencyDecayUnitSec
double GetFrecencyScore(int visit_count,
                        base::Time visit_time,
                        base::Time now) {
  // The number of seconds until the recency component decays by half.
  constexpr base::TimeDelta kRecencyDecayUnitSec = base::Seconds(60);
  // The factor by which the frequency component is exponentiated.
  constexpr double kFrequencyExponent = 1.15;

  const double recency_decayed =
      kRecencyDecayUnitSec /
      (base::TimeDelta(now - visit_time) + kRecencyDecayUnitSec);
  const double frequency_powered = pow(visit_count, kFrequencyExponent);
  return frequency_powered * recency_decayed;
}

// AutocompleteSearchTermHelper ------------------------------------------------

// A helper class to aggregate keyword search term visits returned by the
// `KeywordSearchTermVisitEnumerator` into unique search terms with
// `visit_count` aggregated across the visits for use as prefix or zero-prefix
// suggestions in the omnibox.
class AutocompleteSearchTermHelper {
 public:
  AutocompleteSearchTermHelper() = default;
  AutocompleteSearchTermHelper(const AutocompleteSearchTermHelper&) = delete;
  AutocompleteSearchTermHelper& operator=(const AutocompleteSearchTermHelper&) =
      delete;
  ~AutocompleteSearchTermHelper() = default;

  // `enumerator` enumerates keyword search term visits from the URLDatabase.
  std::unique_ptr<KeywordSearchTermVisit> GetNextUniqueSearchTermFromEnumerator(
      KeywordSearchTermVisitEnumerator& enumerator) {
    // `next_visit` acts as the fast pointer and `last_search_term_` acts as the
    // slow pointer aggregating the search term visit counts across visits.
    while (auto next_visit = enumerator.GetNextVisit()) {
      if (last_search_term_ &&
          IsSameSearchTerm(*next_visit, *last_search_term_)) {
        // Ignore duplicative visits.
        if (IsDuplicativeVisitToSearchTerm(*next_visit, *last_search_term_)) {
          continue;
        }
        // Encountered the same search term:
        // 1. Move `last_search_term_` forward.
        // 2. Add up the search term visit count.
        int visit_count = last_search_term_->visit_count;
        last_search_term_ = std::move(next_visit);
        last_search_term_->visit_count += visit_count;
      } else if (last_search_term_) {
        // Encountered a new search term:
        // 1. Move `last_search_term_` forward.
        // 2. Return the old `last_search_term_`.
        auto search_term_to_return = std::move(last_search_term_);
        last_search_term_ = std::move(next_visit);
        return search_term_to_return;
      } else {
        // Encountered the first search term:
        // 1. Move `last_search_term_` forward.
        last_search_term_ = std::move(next_visit);
      }
    }

    return last_search_term_ ? std::move(last_search_term_) : nullptr;
  }

 private:
  // The last seen search term.
  std::unique_ptr<KeywordSearchTermVisit> last_search_term_;
};

void GetAutocompleteSearchTermsFromEnumerator(
    KeywordSearchTermVisitEnumerator& enumerator,
    const size_t count,
    SearchTermRankingPolicy ranking_policy,
    KeywordSearchTermVisitList* search_terms) {
  AutocompleteSearchTermHelper helper;
  const base::Time now = base::Time::Now();
  while (auto search_term =
             helper.GetNextUniqueSearchTermFromEnumerator(enumerator)) {
    if (ranking_policy == SearchTermRankingPolicy::kFrecency) {
      search_term->score = GetFrecencyScore(search_term->visit_count,
                                            search_term->last_visit_time, now);
    }
    search_terms->push_back(std::move(search_term));
  }
  // Populate `search_terms` with the top `count` search terms in descending
  // recency or frecency scores.
  size_t num_search_terms = std::min(search_terms->size(), count);
  base::ranges::partial_sort(
      search_terms->begin(), std::next(search_terms->begin(), num_search_terms),
      search_terms->end(), [&](const auto& a, const auto& b) {
        return ranking_policy == SearchTermRankingPolicy::kFrecency
                   ? a->score > b->score
                   : a->last_visit_time > b->last_visit_time;
      });
  search_terms->resize(num_search_terms);
}

// MostRepeatedSearchTermHelper ------------------------------------------------

// A helper class to aggregate keyword search term visits returned by the
// `KeywordSearchTermVisitEnumerator` into unique search terms with
// `visit_count` and `score` aggregated across the days of visit for use in the
// Most Visited tiles.
class MostRepeatedSearchTermHelper {
 public:
  MostRepeatedSearchTermHelper() = default;
  MostRepeatedSearchTermHelper(const MostRepeatedSearchTermHelper&) = delete;
  MostRepeatedSearchTermHelper& operator=(const MostRepeatedSearchTermHelper&) =
      delete;
  ~MostRepeatedSearchTermHelper() = default;

  // `enumerator` enumerates keyword search term visits from the URLDatabase.
  // `now` is used to score the unique search terms across the days of visit.
  std::unique_ptr<KeywordSearchTermVisit> GetNextUniqueSearchTermFromEnumerator(
      KeywordSearchTermVisitEnumerator& enumerator,
      base::Time now) {
    const bool ignore_duplicative_visits =
        kRepeatableQueriesIgnoreDuplicateVisits.Get();
    // `next_visit` acts as the fast pointer and `last_search_term_` acts as the
    // slow pointer accumulating the search term score across visits.
    while (auto next_visit = enumerator.GetNextVisit()) {
      bool is_same_search_term =
          last_search_term_ &&
          IsSameSearchTerm(*next_visit, *last_search_term_);
      if (is_same_search_term &&
          IsInSameTimeslot(*next_visit, *last_search_term_)) {
        // Ignore duplicative visits, if applicable.
        if (ignore_duplicative_visits &&
            IsDuplicativeVisitToSearchTerm(*next_visit, *last_search_term_)) {
          continue;
        }
        // Encountered the same timeslot for the same search term:
        // 1. Move `last_search_term_` forward.
        // 2. Add up the search term visit count in the timeslot.
        // 3. Carry over the search term score.
        int visit_count = last_search_term_->visit_count;
        double score = last_search_term_->score.value_or(0.0);
        last_search_term_ = std::move(next_visit);
        last_search_term_->visit_count += visit_count;
        last_search_term_->score =
            last_search_term_->score.value_or(0.0) + score;

      } else if (is_same_search_term) {
        // Ignore duplicative visits, if applicable.
        if (ignore_duplicative_visits &&
            IsDuplicativeVisitToSearchTerm(*next_visit, *last_search_term_)) {
          continue;
        }
        // Encountered a new timeslot for the same search term:
        // 1. Update the search term score by adding the last timeslot's score.
        // 2. Move `last_search_term_` forward.
        // 3. Carry over the search term score.
        double score =
            last_search_term_->score.value_or(0.0) +
            GetMostVisitedFrecencyScore(
                last_search_term_->visit_count,
                VisitTimeToTimeslot(last_search_term_->last_visit_time), now);
        last_search_term_ = std::move(next_visit);
        last_search_term_->score = score;

      } else if (last_search_term_) {
        // Encountered a new search term:
        // 1. Update the search term score by adding the last timeslot's score.
        // 2. Move `last_search_term_` forward.
        // 3. Return the old `last_search_term_`.
        double score =
            last_search_term_->score.value_or(0.0) +
            GetMostVisitedFrecencyScore(
                last_search_term_->visit_count,
                VisitTimeToTimeslot(last_search_term_->last_visit_time), now);
        last_search_term_->score = score;
        auto search_term_to_return = std::move(last_search_term_);
        last_search_term_ = std::move(next_visit);
        return search_term_to_return;
      } else {
        // Encountered the first search term:
        // 1. Move `last_search_term_` forward.
        last_search_term_ = std::move(next_visit);
      }
    }

    // `last_search_term_` has a value:
    // 1. Update the search term score by adding the last timeslot's score.
    if (last_search_term_) {
      double score =
          last_search_term_->score.value_or(0.0) +
          GetMostVisitedFrecencyScore(
              last_search_term_->visit_count,
              VisitTimeToTimeslot(last_search_term_->last_visit_time), now);
      last_search_term_->score = score;
    }

    return last_search_term_ ? std::move(last_search_term_) : nullptr;
  }

 private:
  // The last seen search term.
  std::unique_ptr<KeywordSearchTermVisit> last_search_term_;
};

void GetMostRepeatedSearchTermsFromEnumerator(
    KeywordSearchTermVisitEnumerator& enumerator,
    const size_t count,
    KeywordSearchTermVisitList* search_terms) {
  MostRepeatedSearchTermHelper helper;
  const base::Time now = base::Time::Now();
  while (auto search_term =
             helper.GetNextUniqueSearchTermFromEnumerator(enumerator, now)) {
    // Exclude searches that have not been repeated in some time.
    if (now - search_term->last_visit_time >
        base::Days(kRepeatableQueriesMaxAgeDays.Get())) {
      continue;
    }

    // Exclude searches that have not been repeated enough times.
    if (search_term->visit_count < kRepeatableQueriesMinVisitCount.Get()) {
      continue;
    }

    search_terms->push_back(std::move(search_term));
  }
  // Populate `search_terms` with the top `count` search terms in descending
  // frecency scores.
  size_t num_search_terms = std::min(search_terms->size(), count);
  base::ranges::partial_sort(
      search_terms->begin(), std::next(search_terms->begin(), num_search_terms),
      search_terms->end(),
      [](const auto& a, const auto& b) { return a->score > b->score; });
  search_terms->resize(num_search_terms);
}

}  // namespace history
