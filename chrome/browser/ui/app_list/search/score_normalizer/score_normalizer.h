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
#include "components/prefs/pref_service.h"

namespace app_list {

// The launcher takes scores from providers, these all have different
// distributions and ranges, which makes them difficult to compare. Here we have
// implemented a way to normalize ChromeSearchResults so relevance scores can be
// compared for the launcher.
class ScoreNormalizer {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;

  ScoreNormalizer(const std::string& provider, Profile* profile);

  ~ScoreNormalizer();

  // Record the results from a provider. Results are first converted into a
  // vector of doubles and the distribution is then updated.
  void Record(const Results& search_results);

  // Takes the score from the provider and uses the
  // distribution that has been learnt about that provider
  // to return an updated score.
  double NormalizeScore(const double score) const;

  // Takes the results vector and updates each ChromeSearchResult relevance
  // score by normalizing the score.
  void NormalizeResults(Results* results);

  std::string get_provider() const { return provider_; }

 private:
  friend class ScoreNormalizerTest;

  // Convert Results to a vector of doubles (scores).
  std::vector<double> ConvertResultsToScores(
      const ScoreNormalizer::Results& results) const;

  // Updates the mean of the distribution with the new scores.
  void UpdateDistribution(const std::vector<double>& new_scores);

  // Reads distribution parameters from prefs and updates member variables.
  // If data in prefs does not exist no update occurs.
  void ReadPrefs();

  // Writes to the prefs with information on the distribution.
  void WritePrefs();

  const std::string provider_;

  Profile* profile_;

  // Distribution information, these are updated with every Record().
  int num_results_ = 0;
  double mean_ = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_SCORE_NORMALIZER_H_