// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/balanced_reservoir.h"

#include <cfloat>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace app_list {
namespace {

base::Value VectorToPrefsList(const std::vector<double>& vector) {
  base::Value prefs_list(base::Value::Type::LIST);
  for (const double x : vector) {
    prefs_list.Append(x);
  }
  return prefs_list;
}

std::vector<double> PrefsListToVector(
    const base::Value::ConstListView& prefs_list) {
  std::vector<double> prefs_vector;
  for (const base::Value& score : prefs_list) {
    if (score.is_double()) {
      prefs_vector.push_back(score.GetDouble());
    } else {
      // Return an empty vector if any of the values are not doubles.
      return std::vector<double>();
    }
  }
  return prefs_vector;
}

}  // namespace

BalancedReservoir::BalancedReservoir(const std::string& provider,
                                     Profile* profile,
                                     const int max_number_of_dividers)
    : max_number_of_dividers_(max_number_of_dividers),
      provider_(provider),
      profile_(profile) {
  DCHECK(max_number_of_dividers_ > 0);
  DCHECK(profile_);
  ReadPrefs();
}

BalancedReservoir::~BalancedReservoir() {}

int BalancedReservoir::GetBin(const double score) const {
  return std::upper_bound(dividers_.begin(), dividers_.end(), score) -
         dividers_.begin();
}

void BalancedReservoir::RecordScore(const double score) {
  const int index = GetBin(score);
  // If there aren't enough dividers yet, then directly add the result as a
  // divider.
  if (dividers_.size() < max_number_of_dividers_) {
    dividers_.insert(dividers_.begin() + index, score);
  } else {
    if (counts_[index] == DBL_MAX) {
      for (int i = 0; i < counts_.size(); ++i) {
        counts_[i] = counts_[i] / static_cast<double>(2);
      }
    }
    counts_[index]++;
    const double old_error = GetError();
    const std::vector<double> old_dividers = dividers_;
    const std::vector<double> old_counts = counts_;
    SplitBinByScore(index, score);
    MergeSmallestBins();
    // Undo split and merge if there is no improvement in error.
    if (old_error < GetError()) {
      dividers_ = old_dividers;
      counts_ = old_counts;
    }
  }
}

void BalancedReservoir::SplitBinByScore(const int index, const double score) {
  dividers_.insert(dividers_.begin() + index, score);
  const double count = counts_[index] / 2;
  counts_[index] = count;
  counts_.insert(counts_.begin() + index, count);
  // TODO(crbug.com/1156930): Change this to a better way to split counts than
  // halving the counts between the two new bins.
}

void BalancedReservoir::MergeSmallestBins() {
  double smallest_adjacent_bin_count = DBL_MAX;
  int smallest_bin_index = 0;
  for (int i = 0; i < counts_.size() - 1; i++) {
    if (counts_[i] + counts_[i + 1] < smallest_adjacent_bin_count) {
      smallest_adjacent_bin_count = counts_[i] + counts_[i + 1];
      smallest_bin_index = i;
    }
  }
  counts_[smallest_bin_index + 1] =
      counts_[smallest_bin_index] + counts_[smallest_bin_index + 1];
  dividers_.erase(dividers_.begin() + smallest_bin_index);
  counts_.erase(counts_.begin() + smallest_bin_index);
}

double BalancedReservoir::GetError() const {
  if (counts_.size() == 0) {
    return 0;
  }
  const double mean = std::accumulate(counts_.begin(), counts_.end(), 0.0) /
                      static_cast<double>(counts_.size());
  double error = 0;
  for (double count : counts_) {
    error += std::pow((count - mean), 2);
  }
  return error / static_cast<double>(counts_.size());
}

void BalancedReservoir::ReadPrefs() {
  PrefService* pref_service_ = profile_->GetPrefs();
  const base::DictionaryValue* distribution_data = pref_service_->GetDictionary(
      chromeos::prefs::kLauncherSearchNormalizerParameters);
  const base::Value* provider_data = distribution_data->FindKey(provider_);
  if (provider_data && provider_data->is_dict()) {
    const base::Value* pref_dividers = provider_data->FindKey("dividers");
    const base::Value* pref_counts = provider_data->FindKey("counts");
    if (pref_dividers && pref_counts && pref_dividers->is_list() &&
        pref_counts->is_list()) {
      dividers_ = PrefsListToVector(pref_dividers->GetList());
      counts_ = PrefsListToVector(pref_counts->GetList());
    }
  }
  if (dividers_.empty() || counts_.empty()) {
    dividers_ = std::vector<double>();
    counts_ = std::vector<double>(max_number_of_dividers_ + 1);
  } else {
    DCHECK(dividers_.size() + 1 == counts_.size());
  }
  // TODO(crbug.com/1156930): Add UMA histogram if ReadPrefs() fails because
  // entries in vectors are not doubles.
}

void BalancedReservoir::WritePrefs() {
  PrefService* pref_service = profile_->GetPrefs();
  DictionaryPrefUpdate update(
      pref_service, chromeos::prefs::kLauncherSearchNormalizerParameters);
  base::DictionaryValue* distribution_data = update.Get();
  base::DictionaryValue provider_data;
  provider_data.SetKey("dividers", VectorToPrefsList(dividers_));
  provider_data.SetKey("counts", VectorToPrefsList(counts_));
  distribution_data->SetKey(provider_, std::move(provider_data));
}

}  // namespace app_list