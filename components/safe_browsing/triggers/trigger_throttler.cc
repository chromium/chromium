// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {
const size_t kAdPopupTriggerDefaultQuota = 1;
const size_t kAdRedirectTriggerDefaultQuota = 1;
const size_t kAdSamplerTriggerDefaultQuota = 10;
const size_t kSuspiciousSiteTriggerDefaultQuota = 5;
const char kAdPopupTriggerQuotaParam[] = "ad_popup_trigger_quota";
const char kAdRedirectTriggerQuotaParam[] = "ad_redirect_trigger_quota";
const char kSuspiciousSiteTriggerQuotaParam[] = "suspicious_site_trigger_quota";
const char kTriggerTypeAndQuotaParam[] = "trigger_type_and_quota_csv";

namespace {
const size_t kUnlimitedTriggerQuota = std::numeric_limits<size_t>::max();
constexpr base::TimeDelta kOneDayTimeDelta = base::TimeDelta::FromDays(1);

// Predicate used to search |trigger_type_and_quota_list_| by trigger type.
class TriggerTypeIs {
 public:
  explicit TriggerTypeIs(const TriggerType type) : type_(type) {}
  bool operator()(const TriggerTypeAndQuotaItem& trigger_type_and_quota) {
    return type_ == trigger_type_and_quota.first;
  }

 private:
  TriggerType type_;
};

void ParseTriggerTypeAndQuotaParam(
    std::vector<TriggerTypeAndQuotaItem>* trigger_type_and_quota_list) {
  DCHECK(trigger_type_and_quota_list);
  trigger_type_and_quota_list->clear();

  // First, handle the trigger-specific features.
  int suspicious_site_quota = base::GetFieldTrialParamByFeatureAsInt(
      kSuspiciousSiteTriggerQuotaFeature, kSuspiciousSiteTriggerQuotaParam,
      kSuspiciousSiteTriggerDefaultQuota);
  if (suspicious_site_quota > 0) {
    trigger_type_and_quota_list->push_back(
        std::make_pair(TriggerType::SUSPICIOUS_SITE, suspicious_site_quota));
  }

  int ad_popup_quota = base::GetFieldTrialParamByFeatureAsInt(
      kAdPopupTriggerFeature, kAdPopupTriggerQuotaParam,
      kAdPopupTriggerDefaultQuota);
  if (ad_popup_quota > 0) {
    trigger_type_and_quota_list->push_back(
        std::make_pair(TriggerType::AD_POPUP, ad_popup_quota));
  }

  int ad_redirect_quota = base::GetFieldTrialParamByFeatureAsInt(
      kAdRedirectTriggerFeature, kAdRedirectTriggerQuotaParam,
      kAdRedirectTriggerDefaultQuota);
  if (ad_redirect_quota > 0) {
    trigger_type_and_quota_list->push_back(
        std::make_pair(TriggerType::AD_REDIRECT, ad_redirect_quota));
  }

  // If the feature is disabled we just use the default list. Otherwise the list
  // from the Finch param will be the one used.
  if (!base::FeatureList::IsEnabled(kTriggerThrottlerDailyQuotaFeature)) {
    return;
  }

  const std::string& trigger_and_quota_csv_param =
      base::GetFieldTrialParamValueByFeature(kTriggerThrottlerDailyQuotaFeature,
                                             kTriggerTypeAndQuotaParam);
  if (trigger_and_quota_csv_param.empty()) {
    return;
  }

  std::vector<std::string> split =
      base::SplitString(trigger_and_quota_csv_param, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  // If we don't have the right number of pairs in the csv then don't bother
  // parsing further.
  if (split.size() % 2 != 0) {
    return;
  }
  for (size_t i = 0; i < split.size(); i += 2) {
    // Make sure both the trigger type and quota are integers. Skip them if not.
    int trigger_type_int = -1;
    int quota_int = -1;
    if (!base::StringToInt(split[i], &trigger_type_int) ||
        !base::StringToInt(split[i + 1], &quota_int)) {
      continue;
    }
    trigger_type_and_quota_list->push_back(
        std::make_pair(static_cast<TriggerType>(trigger_type_int), quota_int));
  }

  std::sort(trigger_type_and_quota_list->begin(),
            trigger_type_and_quota_list->end(),
            [](const TriggerTypeAndQuotaItem& a,
               const TriggerTypeAndQuotaItem& b) { return a.first < b.first; });
}

// Looks in |trigger_quota_list| for |trigger_type|. If found, sets |out_quota|
// to the configured quota, and returns true. If not found, returns false.
bool TryFindQuotaForTrigger(
    const TriggerType trigger_type,
    const std::vector<TriggerTypeAndQuotaItem>& trigger_quota_list,
    size_t* out_quota) {
  const auto& trigger_quota_iter =
      std::find_if(trigger_quota_list.begin(), trigger_quota_list.end(),
                   TriggerTypeIs(trigger_type));
  if (trigger_quota_iter != trigger_quota_list.end()) {
    *out_quota = trigger_quota_iter->second;
    return true;
  }
  return false;
}

}  // namespace

TriggerThrottler::TriggerThrottler(PrefService* local_state_prefs)
    : local_state_prefs_(local_state_prefs),
      clock_(base::DefaultClock::GetInstance()) {
  ParseTriggerTypeAndQuotaParam(&trigger_type_and_quota_list_);
  LoadTriggerEventsFromPref();
}

TriggerThrottler::~TriggerThrottler() {}

void TriggerThrottler::SetClockForTesting(base::Clock* test_clock) {
  clock_ = test_clock;
}

bool TriggerThrottler::TriggerCanFire(const TriggerType trigger_type) const {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);

  // Some basic corner cases for triggers that always fire, or disabled
  // triggers that never fire.
  if (trigger_quota == kUnlimitedTriggerQuota)
    return true;
  if (trigger_quota == 0)
    return false;

  // Other triggers are capped, see how many times this trigger has already
  // fired.
  if (!base::Contains(trigger_events_, trigger_type))
    return true;

  const std::vector<base::Time>& timestamps = trigger_events_.at(trigger_type);
  // More quota is available, so the trigger can fire again.
  if (trigger_quota > timestamps.size())
    return true;

  // Otherwise, we have more events than quota, check which day they occurred
  // on. Newest events are at the end of vector so we can simply look at the
  // Nth-from-last entry (where N is the quota) to see if it happened within
  // the current day or earlier.
  base::Time min_timestamp = clock_->Now() - kOneDayTimeDelta;
  const size_t pos = timestamps.size() - trigger_quota;
  return timestamps[pos] < min_timestamp;
}

void TriggerThrottler::TriggerFired(const TriggerType trigger_type) {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);

  // For triggers that always fire, don't bother tracking quota.
  if (trigger_quota == kUnlimitedTriggerQuota)
    return;

  // Otherwise, record that the trigger fired.
  std::vector<base::Time>* timestamps = &trigger_events_[trigger_type];
  timestamps->push_back(clock_->Now());

  // Clean up the trigger events map.
  CleanupOldEvents();

  // Update the pref
  WriteTriggerEventsToPref();
}

void TriggerThrottler::CleanupOldEvents() {
  for (const auto& map_iter : trigger_events_) {
    const TriggerType trigger_type = map_iter.first;
    const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);
    const std::vector<base::Time>& trigger_times = map_iter.second;

    // Skip the cleanup if we have quota room, quotas should generally be small.
    if (trigger_times.size() < trigger_quota)
      return;

    std::vector<base::Time> tmp_trigger_times;
    base::Time min_timestamp = clock_->Now() - kOneDayTimeDelta;
    // Go over the event times for this trigger and keep timestamps which are
    // newer than |min_timestamp|. We put timestamps in a temp vector that will
    // get swapped into the map in place of the existing vector.
    for (const base::Time timestamp : trigger_times) {
      if (timestamp > min_timestamp)
        tmp_trigger_times.push_back(timestamp);
    }

    trigger_events_[trigger_type].swap(tmp_trigger_times);
  }
}

void TriggerThrottler::LoadTriggerEventsFromPref() {
  trigger_events_.clear();
  if (!local_state_prefs_)
    return;

  const base::DictionaryValue* event_dict = local_state_prefs_->GetDictionary(
      prefs::kSafeBrowsingTriggerEventTimestamps);
  for (const auto& trigger_pair : event_dict->DictItems()) {
    // Check that the first item in the pair is convertible to a trigger type
    // and that the second item is a list.
    int trigger_type_int;
    if (!base::StringToInt(trigger_pair.first, &trigger_type_int) ||
        trigger_type_int < static_cast<int>(TriggerType::kMinTriggerType) ||
        trigger_type_int > static_cast<int>(TriggerType::kMaxTriggerType)) {
      continue;
    }
    if (!trigger_pair.second.is_list())
      continue;

    const TriggerType trigger_type = static_cast<TriggerType>(trigger_type_int);
    for (const auto& timestamp : trigger_pair.second.GetList()) {
      if (timestamp.is_double())
        trigger_events_[trigger_type].push_back(
            base::Time::FromDoubleT(timestamp.GetDouble()));
    }
  }
}

void TriggerThrottler::WriteTriggerEventsToPref() {
  if (!local_state_prefs_)
    return;

  base::DictionaryValue trigger_dict;
  for (const auto& trigger_item : trigger_events_) {
    base::Value* pref_timestamps = trigger_dict.SetKey(
        base::NumberToString(static_cast<int>(trigger_item.first)),
        base::Value(base::Value::Type::LIST));
    for (const base::Time timestamp : trigger_item.second) {
      pref_timestamps->Append(base::Value(timestamp.ToDoubleT()));
    }
  }

  local_state_prefs_->Set(prefs::kSafeBrowsingTriggerEventTimestamps,
                          trigger_dict);
}

size_t TriggerThrottler::GetDailyQuotaForTrigger(
    const TriggerType trigger_type) const {
  size_t quota_from_finch = 0;
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
    case TriggerType::GAIA_PASSWORD_REUSE:
    case TriggerType::APK_DOWNLOAD:
      return kUnlimitedTriggerQuota;
    case TriggerType::AD_POPUP:
      // Ad Popup reports are disabled unless they are configured through Finch.
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota_from_finch)) {
        return quota_from_finch;
      }
      break;
    case TriggerType::AD_REDIRECT:
      // Ad Redirects are disabled unless they are configured through Finch.
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota_from_finch)) {
        return quota_from_finch;
      }
      break;
    case TriggerType::AD_SAMPLE:
      // Ad Samples have a non-zero default quota, but it can be overwritten
      // through Finch.
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota_from_finch)) {
        return quota_from_finch;
      } else {
        return kAdSamplerTriggerDefaultQuota;
      }

      break;
    case TriggerType::SUSPICIOUS_SITE:
      // Suspicious Sites are disabled unless they are configured through Finch.
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota_from_finch)) {
        return quota_from_finch;
      }
      break;
  }
  // By default, unhandled or unconfigured trigger types have no quota.
  return 0;
}

void TriggerThrottler::ResetPrefsForTesting(PrefService* local_state_prefs) {
  local_state_prefs_ = local_state_prefs;
  LoadTriggerEventsFromPref();
}

}  // namespace safe_browsing
