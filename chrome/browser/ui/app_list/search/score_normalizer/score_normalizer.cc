// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/score_normalizer.h"

#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace app_list {

ScoreNormalizer::ScoreNormalizer(const std::string& provider, Profile* profile)
    : provider_(provider), profile_(profile) {
  DCHECK(profile_);
  ReadPrefs();
}
ScoreNormalizer::~ScoreNormalizer() {}

void ScoreNormalizer::Record(const Results& search_results) {
  UpdateDistribution(ConvertResultsToScores(search_results));
  WritePrefs();
}

double ScoreNormalizer::NormalizeScore(const double score) const {
  // TODO(crbug.com/1156930): Basic implementation of subtracting the mean for
  // now. Will be updated later to a different normalization method.
  return score - mean_;
}

void ScoreNormalizer::NormalizeResults(Results* results) {
  for (auto& result : *results) {
    result->set_relevance(NormalizeScore(result->relevance()));
  }
}

std::vector<double> ScoreNormalizer::ConvertResultsToScores(
    const Results& results) const {
  std::vector<double> scores;
  for (const auto& result : results) {
    scores.push_back(result->relevance());
  }
  return scores;
}

void ScoreNormalizer::UpdateDistribution(
    const std::vector<double>& new_scores) {
  if (num_results_ > INT_MAX - new_scores.size()) {
    // If overflow will occur, we do not update the distribution.
    return;
  }

  num_results_ += new_scores.size();
  double new_sum = 0;
  for (double score : new_scores) {
    new_sum += score;
  }

  if (num_results_ == 0) {
    // If there are no results, keep the mean of the distribution at 0.
    // This is so no normalization occurs.
    return;
  } else {
    mean_ =
        (new_sum + mean_ * (num_results_ - new_scores.size())) / num_results_;
  }
}

void ScoreNormalizer::ReadPrefs() {
  PrefService* pref_service_ = profile_->GetPrefs();
  const base::DictionaryValue* distribution_data = pref_service_->GetDictionary(
      chromeos::prefs::kLauncherSearchNormalizerParameters);
  const base::Value* pref_mean = distribution_data->FindKey("mean");
  const base::Value* pref_num_results =
      distribution_data->FindKey("num_results");
  if (pref_mean && pref_num_results && pref_mean->is_double() &&
      pref_num_results->is_int()) {
    mean_ = pref_mean->GetDouble();
    num_results_ = pref_num_results->GetInt();
  }
}

void ScoreNormalizer::WritePrefs() {
  PrefService* pref_service = profile_->GetPrefs();
  DictionaryPrefUpdate update(
      pref_service, chromeos::prefs::kLauncherSearchNormalizerParameters);
  base::DictionaryValue* distribution_data = update.Get();
  distribution_data->SetIntPath("num_results", num_results_);
  distribution_data->SetDoublePath("mean", mean_);
}

}  // namespace app_list
