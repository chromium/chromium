// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_SCORE_NORMALIZER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_SCORE_NORMALIZER_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/score_normalizer/balanced_reservoir.h"
#include "components/prefs/pref_service.h"

namespace app_list {

// ScoreNormalizer is responsible for normalizing relevance scores of search
// results from providers. It learns to transform all scores to a uniformly
// distributed normalized score between 0 to 1. The launcher takes scores of
// results from many providers, these all have different distributions and
// ranges, which makes them difficult to compare. After normalizing the
// relevance scores can be compared and ranked in the launcher. This class
// should only be initialized in a provider class where it can be called to
// record and normalize scores.
//
// To normalize scores the ScoreNormalizer uses a BalancedReservoir. The
// BalancedReservoir stores a subset of the search result scores, this subset of
// scores is called dividers. To normalize a score, the quantile of that score
// in the dividers is returned, this is always a value in (0,1). If a normalized
// score of 1 is returned, this is either due to empty dividers or index out of
// range when finding which index the score is in the dividers. The dividers are
// updated such that the counts of scores observed between each divider remains
// balanced. Each pair of adjacent dividers forms a histogram bin. To ensure
// bins remain balanced with each new score added bins are:
// 1. Split by adding the new score in between dividers.
// 2. Smallest bins are then merged.
// The L2 error is calculated to ensure splits and merges improve the balance of
// the reservoir.
//
// Use of the ScoreNormalizer:
// - Initialization. A profile is required since information about the provider
// scores in the BalancedReservoir class are stored in prefs. The reservoir size
// can be set to 25, or any other positive integer.
// - RecordResults() should be called right before new results are swapped in
// for old results in the providers. This updates the BalancedReservoir with the
// provider's score distribution.
// - NormalizeResults() should then be called. This changes the relevance scores
// of the ChromeSearchResult in place with the normalized score.
class ScoreNormalizer {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;

  ScoreNormalizer(const std::string& provider,
                  Profile* profile,
                  const int reservoir_size);

  ~ScoreNormalizer();

  ScoreNormalizer(const ScoreNormalizer&) = delete;
  ScoreNormalizer& operator=(const ScoreNormalizer&) = delete;

  // Records a score and updates the distribution by splitting and merging bins
  // if there is an improvement in the error.
  void RecordScore(const double score);

  // Records the results from a provider and updates the distribution.
  void RecordResults(const Results& results);

  // Takes the score from the provider and uses the
  // distribution that has been learnt about that provider
  // to return an updated score.
  double NormalizeScore(const double score) const;

  // Takes the results vector and updates each ChromeSearchResult relevance
  // score by normalizing the score.
  void NormalizeResults(Results* results);

 private:
  friend class ScoreNormalizerTest;

  // Distribution information, these are updated with every Record().
  const int reservoir_size_;
  BalancedReservoir reservoir_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_SCORE_NORMALIZER_H_