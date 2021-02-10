// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/score_normalizer.h"

#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/score_normalizer/balanced_reservoir.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace app_list {

ScoreNormalizer::ScoreNormalizer(const std::string& provider,
                                 Profile* profile,
                                 const int reservoir_size)
    : reservoir_size_(reservoir_size),
      reservoir_(provider, profile, reservoir_size) {}

ScoreNormalizer::~ScoreNormalizer() {}

void ScoreNormalizer::RecordScore(const double score) {
  reservoir_.RecordScore(score);
}

void ScoreNormalizer::RecordResults(const Results& results) {
  for (const auto& result : results) {
    RecordScore(result->relevance());
  }
  UMA_HISTOGRAM_COUNTS_100("Apps.AppList.ScoreNormalizer.SearchResultsCount",
                           results.size());
  reservoir_.WritePrefs();
}

double ScoreNormalizer::NormalizeScore(const double score) const {
  // Returns the quantile of the score, bound between [0,1]
  // If dividers size is 0 we return 1.
  return reservoir_.NormalizeScore(score);
}

void ScoreNormalizer::NormalizeResults(Results* results) {
  for (auto& result : *results) {
    double score = result->relevance();
    result->set_relevance(NormalizeScore(score));
  }
}

}  // namespace app_list
