// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_BALANCED_RESERVOIR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_BALANCED_RESERVOIR_H_

#include <string>
#include <vector>

class Profile;

namespace app_list {

// To normalize scores from the provider we store the distribution of scores in
// an balanced reservoir. This reservoir is a subset of all provider scores.
// The reservoir aims to have the number of scores observed between each score
// in the reservoir to be evenly distributed.
class BalancedReservoir {
 public:
  BalancedReservoir(const std::string& provider,
                    Profile* profile,
                    const int max_number_of_dividers);

  ~BalancedReservoir();

  BalancedReservoir(const BalancedReservoir&) = delete;
  BalancedReservoir& operator=(const BalancedReservoir&) = delete;

  // Gets the index of the bin that the score belongs to.
  int GetBin(const double score) const;

  // Adds the score into the bins by updating counts or inserting dividers
  // if there are not enough dividers.
  void RecordScore(const double score);

  // Splits the bin using score.
  void SplitBinByScore(const int index, const double score);

  // Merges smallest adjacent bins.
  void MergeSmallestBins();

  // Calculates the mean squared error of counts.
  double GetError() const;

  // Normalizes the score by returning the quantile the score belongs in.
  // Also adds a continuous offset calculated with a linear mapping between bin
  // boundaries or hyperbolic decay at the left and rightmost bins to map to
  // a continuous score bound between quantile it belongs to and the next
  // quantile.
  double NormalizeScore(const double score) const;

  // Reads distribution parameters from prefs and updates member variables.
  // If data in prefs does not exist no update occurs.
  void ReadPrefs();

  // Writes to the prefs with information on the distribution.
  void WritePrefs();

  std::vector<double> get_dividers() const { return dividers_; }
  void set_dividers_for_test(const std::vector<double> dividers) {
    dividers_ = dividers;
  }

  std::vector<double> get_counts() const { return counts_; }
  void set_counts_for_test(const std::vector<double> counts) {
    counts_ = counts;
  }

 private:
  const int max_number_of_dividers_;
  std::vector<double> dividers_;
  std::vector<double> counts_;

  const std::string provider_;

  Profile* profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SCORE_NORMALIZER_BALANCED_RESERVOIR_H_