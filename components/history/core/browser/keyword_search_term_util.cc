// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/keyword_search_term_util.h"

#include "base/time/time.h"
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

// Returns whether two search terms are identical, i.e., have the same
// normalized search terms.
bool IsSameSearchTerm(const KeywordSearchTermVisit& search_term_visit,
                      const KeywordSearchTermVisit& other_search_term_visit) {
  return search_term_visit.normalized_term ==
         other_search_term_visit.normalized_term;
}

// Return whether a visit to a search term constitutes a duplicate visit, i.e.,
// a visit to the same search term in an interval smaller than
// kAutocompleteDuplicateVisitIntervalThreshold.
// Called with identical search terms only. i.e., IsSameSearchTerm() is true.
bool IsDuplicateVisit(const KeywordSearchTermVisit& search_term_visit,
                      const KeywordSearchTermVisit& other_search_term_visit) {
  return search_term_visit.last_visit_time -
             other_search_term_visit.last_visit_time <=
         kAutocompleteDuplicateVisitIntervalThreshold;
}

// Transforms a visit time to its timeslot, i.e., day of the viist.
base::Time VisitTimeToTimeslot(base::Time visit_time) {
  return visit_time.LocalMidnight();
}

// Returns whether two search term visits are in the same timeslot.
// Called with identical search terms only. i.e., IsSameSearchTerm() is true.
bool IsSameTimeslot(const KeywordSearchTermVisit& search_term_visit,
                    const KeywordSearchTermVisit& other_search_term_visit) {
  return VisitTimeToTimeslot(search_term_visit.last_visit_time) ==
         VisitTimeToTimeslot(other_search_term_visit.last_visit_time);
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

// SearchTermHelper ------------------------------------------------------------

// A helper class to return keyword search terms with visit counts accumulated
// across visits for use as prefix or zero-prefix suggestions in the omnibox.
class SearchTermHelper {
 public:
  SearchTermHelper() = default;

  SearchTermHelper(const SearchTermHelper&) = delete;
  SearchTermHelper& operator=(const SearchTermHelper&) = delete;

  ~SearchTermHelper() = default;

  // |enumerator| enumerates keyword search term visits from the URLDatabase.
  // |ignore_duplicate_visits| specifies whether duplicative visits to a search
  // term should be ignored.
  std::unique_ptr<KeywordSearchTermVisit> GetNextSearchTermFromEnumerator(
      KeywordSearchTermVisitEnumerator& enumerator,
      bool ignore_duplicate_visits) {
    // |next_visit| acts as the fast pointer and |last_search_term_| acts as the
    // slow pointer accumulating the search term visit counts across visits.
    while (auto next_visit = enumerator.GetNextVisit()) {
      if (last_search_term_ &&
          IsSameSearchTerm(*next_visit, *last_search_term_)) {
        if (ignore_duplicate_visits &&
            IsDuplicateVisit(*next_visit, *last_search_term_)) {
          continue;
        }
        // Encountered the same search term:
        // 1. Move |last_search_term_| forward.
        // 2. Add up the search term visit count.
        int visit_count = last_search_term_->visit_count;
        last_search_term_ = std::move(next_visit);
        last_search_term_->visit_count += visit_count;
      } else if (last_search_term_) {
        // Encountered a new search term and |last_search_term_| has a value:
        // 1. Move |last_search_term_| forward.
        // 2. Return the old |last_search_term_|.
        auto search_term_to_return = std::move(last_search_term_);
        last_search_term_ = std::move(next_visit);
        return search_term_to_return;
      } else {
        // Encountered a new search term and |last_search_term_| has no value:
        // 1. Move |last_search_term_| forward.
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
    bool ignore_duplicate_visits,
    SearchTermRankingPolicy ranking_policy,
    KeywordSearchTermVisitList* search_terms) {
  SearchTermHelper helper;
  const base::Time now = base::Time::Now();
  while (auto search_term = helper.GetNextSearchTermFromEnumerator(
             enumerator, ignore_duplicate_visits)) {
    if (ranking_policy == SearchTermRankingPolicy::kFrecency) {
      search_term->score = GetFrecencyScore(search_term->visit_count,
                                            search_term->last_visit_time, now);
    }
    search_terms->push_back(std::move(search_term));
  }
  // Order the search terms by descending recency or frecency.
  std::stable_sort(search_terms->begin(), search_terms->end(),
                   [&](const auto& a, const auto& b) {
                     return ranking_policy == SearchTermRankingPolicy::kFrecency
                                ? a->score > b->score
                                : a->last_visit_time > b->last_visit_time;
                   });
}

// MostRepeatedSearchTermHelper ------------------------------------------------

// A helper class to return keyword search terms with frecency scores
// accumulated across days for use in the Most Visited tiles.
class MostRepeatedSearchTermHelper {
 public:
  MostRepeatedSearchTermHelper() = default;

  MostRepeatedSearchTermHelper(const MostRepeatedSearchTermHelper&) = delete;
  MostRepeatedSearchTermHelper& operator=(const MostRepeatedSearchTermHelper&) =
      delete;

  ~MostRepeatedSearchTermHelper() = default;

  // |enumerator| enumerates keyword search term visits from the URLDatabase.
  // |now| is the time used to score the search term.
  std::unique_ptr<KeywordSearchTermVisit> GetNextSearchTermFromEnumerator(
      KeywordSearchTermVisitEnumerator& enumerator,
      base::Time now) {
    // |next_visit| acts as the fast pointer and |last_search_term_| acts as the
    // slow pointer accumulating the search term score across visits.
    while (auto next_visit = enumerator.GetNextVisit()) {
      bool is_same_search_term =
          last_search_term_ &&
          IsSameSearchTerm(*next_visit, *last_search_term_);
      if (is_same_search_term &&
          IsSameTimeslot(*next_visit, *last_search_term_)) {
        // The same timeslot for the same search term:
        // 1. Move |last_search_term_| forward.
        // 2. Add up the search term visit count in the timeslot.
        // 3. Carry over the search term score.
        int visit_count = last_search_term_->visit_count;
        double score = last_search_term_->score.value_or(0.0);
        last_search_term_ = std::move(next_visit);
        last_search_term_->visit_count += visit_count;
        last_search_term_->score =
            last_search_term_->score.value_or(0.0) + score;

      } else if (is_same_search_term) {
        // A new timeslot for the same search term:
        // 1. Update the search term score by adding the last timeslot's score.
        // 2. Move |last_search_term_| forward.
        // 3. Carry over the search term score.
        double score =
            last_search_term_->score.value_or(0.0) +
            GetMostVisitedFrecencyScore(
                last_search_term_->visit_count,
                VisitTimeToTimeslot(last_search_term_->last_visit_time), now);
        last_search_term_ = std::move(next_visit);
        last_search_term_->score = score;

      } else if (last_search_term_) {
        // Encountered a new search term and |last_search_term_| has a value:
        // 1. Update the search term score by adding the last timeslot's score.
        // 2. Move |last_search_term_| forward.
        // 3. Return the old |last_search_term_|.
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
        // Encountered a new search term and |last_search_term_| has no value:
        // 1. Move |last_search_term_| forward.
        last_search_term_ = std::move(next_visit);
      }
    }

    // |last_search_term_| has a value:
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
    KeywordSearchTermVisitList* search_terms) {
  MostRepeatedSearchTermHelper helper;
  const base::Time now = base::Time::Now();
  while (auto search_term =
             helper.GetNextSearchTermFromEnumerator(enumerator, now)) {
    search_terms->push_back(std::move(search_term));
  }
  // Order the search terms by descending frecency scores.
  std::stable_sort(
      search_terms->begin(), search_terms->end(),
      [](const auto& a, const auto& b) { return a->score > b->score; });
}

}  // namespace history
