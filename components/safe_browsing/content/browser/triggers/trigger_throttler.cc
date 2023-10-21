// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/triggers/trigger_throttler.h"

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {
const size_t kAdSamplerTriggerDefaultQuota = 10;
const size_t kSuspiciousSiteTriggerDefaultQuota = 5;
const char kSuspiciousSiteTriggerQuotaParam[] = "suspicious_site_trigger_quota";

namespace {
const size_t kUnlimitedTriggerQuota = std::numeric_limits<size_t>::max();
constexpr base::TimeDelta kOneDayTimeDelta = base::Days(1);

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
}

// Looks in |trigger_quota_list| for |trigger_type|. If found, sets |out_quota|
// to the configured quota, and returns true. If not found, returns false.
bool TryFindQuotaForTrigger(
    const TriggerType trigger_type,
    const std::vector<TriggerTypeAndQuotaItem>& trigger_quota_list,
    size_t* out_quota) {
  const auto& trigger_quota_iter = base::ranges::find(
      trigger_quota_list, trigger_type, &TriggerTypeAndQuotaItem::first);
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

  const base::Value::Dict& event_dict =
      local_state_prefs_->GetDict(prefs::kSafeBrowsingTriggerEventTimestamps);
  for (auto trigger_pair : event_dict) {
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
            base::Time::FromSecondsSinceUnixEpoch(timestamp.GetDouble()));
    }
  }
}

void TriggerThrottler::WriteTriggerEventsToPref() {
  if (!local_state_prefs_)
    return;

  base::Value::Dict trigger_dict;
  for (const auto& trigger_item : trigger_events_) {
    base::Value::List timestamps;
    for (const base::Time timestamp : trigger_item.second) {
      timestamps.Append(timestamp.InSecondsFSinceUnixEpoch());
    }

    trigger_dict.Set(base::NumberToString(static_cast<int>(trigger_item.first)),
                     std::move(timestamps));
  }

  local_state_prefs_->SetDict(prefs::kSafeBrowsingTriggerEventTimestamps,
                              std::move(trigger_dict));
}

size_t TriggerThrottler::GetDailyQuotaForTrigger(
    const TriggerType trigger_type) const {
  size_t quota = 0;
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
    case TriggerType::GAIA_PASSWORD_REUSE:
    case TriggerType::APK_DOWNLOAD:
    case TriggerType::PHISHY_SITE_INTERACTION:
      return kUnlimitedTriggerQuota;

    case TriggerType::DEPRECATED_AD_POPUP:
    case TriggerType::DEPRECATED_AD_REDIRECT:
      return 0;

    case TriggerType::AD_SAMPLE:
      // Check for non-default quota (needed for unit tests).
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota)) {
        return quota;
      }
      return kAdSamplerTriggerDefaultQuota;

    case TriggerType::SUSPICIOUS_SITE:
      // Suspicious Sites are disabled unless they are configured through Finch.
      if (TryFindQuotaForTrigger(trigger_type, trigger_type_and_quota_list_,
                                 &quota)) {
        return quota;
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
