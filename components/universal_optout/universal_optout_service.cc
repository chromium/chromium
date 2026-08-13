// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/universal_optout/universal_optout_service.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/prefs.h"

namespace universal_optout {

namespace {

enum class EligibilityCategory {
  kEligible,
  kTrailingEligible,
  kIneligible,
};

bool IsEligibleLocation(const std::string& geo_level1) {
  return std::ranges::contains(features::GetTargetLocations(),
                               base::ToLowerASCII(geo_level1));
}

// Categorizes the user's eligibility status based on their location history
// within the sliding windows.
EligibilityCategory GetEligibilityCategory(const base::DictValue& history_dict,
                                           base::Time current_day) {
  int recent_total_days = 0;
  int recent_eligible_days = 0;
  int trailing_total_days = 0;
  int trailing_eligible_days = 0;

  base::TimeDelta recent_window = features::kEligibilityWindow.Get();
  base::TimeDelta trailing_window = features::kTrailingEligibilityWindow.Get();

  for (auto [key, value] : history_dict) {
    std::optional<base::Time> record_day = base::ValueToTime(base::Value(key));
    if (!record_day.has_value()) {
      continue;
    }

    base::TimeDelta diff = current_day - *record_day;
    if (diff >= base::TimeDelta()) {
      bool is_eligible = value.is_bool() && value.GetBool();
      if (diff < recent_window) {
        recent_total_days++;
        if (is_eligible) {
          recent_eligible_days++;
        }
      }
      if (diff < trailing_window) {
        trailing_total_days++;
        if (is_eligible) {
          trailing_eligible_days++;
        }
      }
    }
  }

  bool is_recent_eligible =
      recent_total_days > 0 &&
      (static_cast<double>(recent_eligible_days) / recent_total_days >=
       UniversalOptOutService::kEligibilityThresholdRatio);
  if (is_recent_eligible) {
    return EligibilityCategory::kEligible;
  }

  bool is_trailing_eligible =
      trailing_total_days > 0 &&
      (static_cast<double>(trailing_eligible_days) / trailing_total_days >=
       UniversalOptOutService::kEligibilityThresholdRatio);
  if (is_trailing_eligible) {
    return EligibilityCategory::kTrailingEligible;
  }

  return EligibilityCategory::kIneligible;
}

}  // namespace

UniversalOptOutService::UniversalOptOutService(
    PrefService& pref_service,
    variations::VariationsService& variations_service,
    const base::Clock& clock)
    : pref_service_(pref_service),
      variations_service_(variations_service),
      clock_(clock) {
  variations_service_observation_.Observe(&variations_service);
  RecordLocationAndUpdateEligibility();
}

UniversalOptOutService::~UniversalOptOutService() = default;

void UniversalOptOutService::Shutdown() {
  variations_service_observation_.Reset();
}

void UniversalOptOutService::OnSeedFetched() {
  RecordLocationAndUpdateEligibility();
}

bool UniversalOptOutService::IsEligible() const {
  return pref_service_->GetBoolean(prefs::kUniversalOptOutEligible);
}

void UniversalOptOutService::RecordLocationAndUpdateEligibility() {
  base::Time current_day = GetCurrentDay();
  ScopedDictPrefUpdate history_update(
      &pref_service_.get(), prefs::kUniversalOptOutEligibilityHistory);
  RecordLocation(current_day, history_update);
  PruneExpiredHistory(current_day, history_update);
  UpdateEligibility(current_day);
}

void UniversalOptOutService::RecordLocation(
    base::Time current_day,
    ScopedDictPrefUpdate& history_update) {
  // TODO(b/538460105): Check RegionalCapabilitiesService country matches the
  // target location.
  std::string geo_level1 = variations_service_->GetLatestGeoLevel1();

  if (!geo_level1.empty()) {
    std::string day_key = base::TimeToValue(current_day).GetString();
    bool is_eligible = IsEligibleLocation(geo_level1);
    history_update->Set(day_key, is_eligible);
  }
}

void UniversalOptOutService::PruneExpiredHistory(
    base::Time current_day,
    ScopedDictPrefUpdate& history_update) {
  const base::DictValue& history_dict =
      pref_service_->GetDict(prefs::kUniversalOptOutEligibilityHistory);
  std::vector<std::string> keys_to_remove;

  base::TimeDelta retention_window =
      std::max(features::kEligibilityWindow.Get(),
               features::kTrailingEligibilityWindow.Get());

  for (auto [key, value] : history_dict) {
    std::optional<base::Time> record_day = base::ValueToTime(base::Value(key));
    if (!record_day.has_value()) {
      keys_to_remove.push_back(key);
      continue;
    }

    if (current_day - *record_day >= retention_window ||
        *record_day > current_day) {
      keys_to_remove.push_back(key);
    }
  }

  for (const auto& key : keys_to_remove) {
    history_update->Remove(key);
  }
}

void UniversalOptOutService::UpdateEligibility(base::Time current_day) {
  const base::DictValue& history_dict =
      pref_service_->GetDict(prefs::kUniversalOptOutEligibilityHistory);
  EligibilityCategory category =
      GetEligibilityCategory(history_dict, current_day);

  bool currently_eligible =
      pref_service_->GetBoolean(prefs::kUniversalOptOutEligible);
  bool is_opt_out_enabled =
      pref_service_->GetBoolean(prefs::kUniversalOptOutEnabled);

  if (currently_eligible) {
    if (!is_opt_out_enabled && category == EligibilityCategory::kIneligible) {
      pref_service_->SetBoolean(prefs::kUniversalOptOutEligible, false);
    }
  } else {
    if (category == EligibilityCategory::kEligible) {
      pref_service_->SetBoolean(prefs::kUniversalOptOutEligible, true);
    }
  }
}

base::Time UniversalOptOutService::GetCurrentDay() const {
  return clock_->Now().UTCMidnight();
}

}  // namespace universal_optout
