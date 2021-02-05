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

// The launcher takes scores from providers, these all have different
// distributions and ranges, which makes them difficult to compare. Here we have
// implemented a way to normalize ChromeSearchResults so relevance scores can be
// compared for the launcher.
class ScoreNormalizer {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;

  ScoreNormalizer(const std::string& provider,
                  Profile* profile,
                  const int reservoir_size);

  ~ScoreNormalizer();

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