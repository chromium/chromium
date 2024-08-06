// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

#include <optional>

#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_education/common/feature_promo_data.h"

namespace {
// Promo data will be saved as a dictionary in the PrefService of a profile.

// PrefService path. The "snooze" bit is a legacy implementation detail.
const char kIPHPromoDataPath[] = "in_product_help.snoozed_feature";

// Path to the boolean indicates if an IPH was dismissed.
// in_product_help.snoozed_feature.[iph_name].is_dismissed
constexpr char kIPHIsDismissedPath[] = "is_dismissed";
// Path to the enum that indicates how an IPH was dismissed.
// in_product_help.snoozed_feature.[iph_name].last_dismissed_by
constexpr char kIPHLastDismissedByPath[] = "last_dismissed_by";
// Path to the timestamp an IPH was first shown.
// in_product_help.snoozed_feature.[iph_name].first_show_time
constexpr char kIPHFirstShowTimePath[] = "first_show_time";
// Path to the timestamp an IPH was last shown.
// in_product_help.snoozed_feature.[iph_name].last_show_time
constexpr char kIPHLastShowTimePath[] = "last_show_time";
// Path to the timestamp an IPH was last snoozed.
// in_product_help.snoozed_feature.[iph_name].last_snooze_time
constexpr char kIPHLastSnoozeTimePath[] = "last_snooze_time";
// Path to the count of how many times this IPH has been snoozed.
// in_product_help.snoozed_feature.[iph_name].snooze_count
constexpr char kIPHSnoozeCountPath[] = "snooze_count";
// Path to the count of how many times this IPH has been shown.
// in_product_help.snoozed_feature.[iph_name].show_count
constexpr char kIPHShowCountPath[] = "show_count";
// Path to the index into a cycling promo.
constexpr char kIPHPromoIndexPath[] = "promo_index";
// Path to a list of app IDs that the IPH was shown for; applies to app-specific
// IPH only.
constexpr char kIPHShownForAppsPath[] = "shown_for_apps";

// Path to the most recent session start time.
constexpr char kIPHSessionStartPath[] = "in_product_help.session_start_time";
// Path to the most recent active time.
constexpr char kIPHSessionLastActiveTimePath[] =
    "in_product_help.session_last_active_time";

// Path to the time of the most recent heavyweight promo.
constexpr char kIPHPolicyLastHeavyweightPromoPath[] =
    "in_product_help.policy_last_heavyweight_promo_time";

// New Badge base path.
constexpr char kNewBadgePath[] = "in_product_help.new_badge";

// The number of times a badge has been shown.
constexpr char kNewBadgeShowCountPath[] = "show_count";

// The number of times the user has used the badge's entry point.
constexpr char kNewBadgeUsedCountPath[] = "used_count";

// The time the promoted feature was first enabled.
constexpr char kNewBadgeFeatureEnabledTimePath[] = "feature_enabled_time";

// Base path to recent session start times.
constexpr char kRecentSessionStartTimesPath[] =
    "in_product_help.recent_session_start_times";

// Base path to track when recent sessions were enabled.
constexpr char kRecentSessionEnabledTimePath[] =
    "in_product_help.recent_session_enabled_time";

constexpr char kKeyedPromoKey[] = "key";
constexpr char kKeyedPromoShowCount[] = "show_count";
constexpr char kKeyedPromoLastShownTime[] = "last_show_time";

// Tries to read keyed promo data from a `base::Value`. Returns null on failure;
// on old or missing/corrupt data, writes in sensible default values.
std::optional<user_education::KeyedFeaturePromoDataMap::value_type> ReadKeyData(
    const base::Value& value,
    base::Time now) {
  std::string key;
  user_education::KeyedFeaturePromoData data;

  if (const auto* key_ptr = value.GetIfString()) {
    // This is old-style data. Set some sensible defaults for the values that
    // aren't present.
    data.show_count = 1;
    data.last_shown_time = now;
    return std::make_pair(*key_ptr, data);
  }

  if (const auto* key_data = value.GetIfDict()) {
    if (const auto* maybe_key = key_data->FindString(kKeyedPromoKey)) {
      // Show count needs to be at least 1.
      data.show_count =
          std::max(1, key_data->FindInt(kKeyedPromoShowCount).value_or(1));

      // Time may or may not be set and may or may not be valid; if invalid,
      // default to now.
      const auto* const time_val = key_data->Find(kKeyedPromoLastShownTime);
      const std::optional<base::Time> time =
          time_val ? base::ValueToTime(*time_val) : std::nullopt;
      data.last_shown_time = time.value_or(now);

      // This should now be a (relatively) well-formed entry.
      return std::make_pair(*maybe_key, data);
    }
  }

  return std::nullopt;
}

// Writes keyed promo data to a `base::Value`.
base::Value WriteKeyData(const std::string& key,
                         const user_education::KeyedFeaturePromoData& data) {
  base::Value::Dict value;
  value.Set(kKeyedPromoKey, key);
  value.Set(kKeyedPromoShowCount, data.show_count);
  value.Set(kKeyedPromoLastShownTime, base::TimeToValue(data.last_shown_time));
  return base::Value(std::move(value));
}

// Common logic for clearing recent sessions.
void ClearRecentSessionData(Profile& profile) {
  profile.GetPrefs()->ClearPref(kRecentSessionStartTimesPath);
  profile.GetPrefs()->ClearPref(kRecentSessionEnabledTimePath);
}

}  // namespace

RecentSessionData::RecentSessionData() = default;
RecentSessionData::RecentSessionData(const RecentSessionData&) = default;
RecentSessionData& RecentSessionData::operator=(const RecentSessionData&) =
    default;
RecentSessionData::~RecentSessionData() = default;

BrowserFeaturePromoStorageService::BrowserFeaturePromoStorageService(
    Profile* profile)
    : profile_(profile) {
  set_profile_creation_time(profile->GetCreationTime());
}

BrowserFeaturePromoStorageService::~BrowserFeaturePromoStorageService() =
    default;

// static
void BrowserFeaturePromoStorageService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kIPHPromoDataPath);
  registry->RegisterDictionaryPref(kNewBadgePath);
  registry->RegisterTimePref(kIPHSessionStartPath, base::Time());
  // This pref is updated frequently and an exact value is not required on e.g.
  // browser crash, so marking as `LOSSY_PREF` will prevent frequent disk
  // writes that could harm performance. The pref should still be written both
  // occasionally during browser operation and at shutdown.
  registry->RegisterTimePref(kIPHSessionLastActiveTimePath, base::Time(),
                             PrefRegistry::LOSSY_PREF);
  registry->RegisterTimePref(kIPHPolicyLastHeavyweightPromoPath, base::Time());
  registry->RegisterListPref(kRecentSessionStartTimesPath);
  registry->RegisterTimePref(kRecentSessionEnabledTimePath, base::Time());
}

// static
void BrowserFeaturePromoStorageService::ClearUsageHistory(Profile* profile) {
  // Recent session data stores timestamps of recent sessions in prefs, so
  // should be cleared when the user clears their browsing data.
  ClearRecentSessionData(*profile);
}

void BrowserFeaturePromoStorageService::Reset(
    const base::Feature& iph_feature) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  update->RemoveByDottedPath(iph_feature.name);
}

std::optional<user_education::FeaturePromoData>
BrowserFeaturePromoStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const std::string path_prefix = std::string(iph_feature.name) + ".";

  const auto& pref_data = profile_->GetPrefs()->GetDict(kIPHPromoDataPath);
  std::optional<bool> is_dismissed =
      pref_data.FindBoolByDottedPath(path_prefix + kIPHIsDismissedPath);
  std::optional<int> last_dismissed_by =
      pref_data.FindIntByDottedPath(path_prefix + kIPHLastDismissedByPath);
  std::optional<base::Time> first_show_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHFirstShowTimePath));
  std::optional<base::Time> last_show_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastShowTimePath));
  std::optional<base::Time> snooze_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastSnoozeTimePath));
  std::optional<int> snooze_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHSnoozeCountPath);
  std::optional<int> show_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHShowCountPath);
  std::optional<int> promo_index =
      pref_data.FindIntByDottedPath(path_prefix + kIPHPromoIndexPath);
  const base::Value::List* keyed_data =
      pref_data.FindListByDottedPath(path_prefix + kIPHShownForAppsPath);

  std::optional<user_education::FeaturePromoData> promo_data;

  if (!is_dismissed || !snooze_time || !snooze_count) {
    // IPH data is corrupt. Ignore the previous data.
    return promo_data;
  }

  if (!last_show_time || !show_count) {
    // This data was stored by a previous version. Assume previous IPH were
    // snoozed.
    last_show_time = *snooze_time - base::Seconds(1);
    show_count = *snooze_count;
  }

  if (!first_show_time) {
    // This data was stored by a previous version. First show time inherits
    // last show time, or null.
    first_show_time = show_count > 0 ? *last_show_time : base::Time();
  }

  promo_data = user_education::FeaturePromoData();
  promo_data->is_dismissed = *is_dismissed;
  promo_data->first_show_time = *first_show_time;
  promo_data->last_show_time = *last_show_time;
  promo_data->last_snooze_time = *snooze_time;
  promo_data->snooze_count = *snooze_count;
  promo_data->show_count = *show_count;
  promo_data->promo_index = promo_index.value_or(0);

  // Since `last_dismissed_by` was not previously recorded, default to
  // "canceled" if the data isn't present or is invalid.
  if (!last_dismissed_by || *last_dismissed_by < 0 ||
      *last_dismissed_by >
          static_cast<int>(
              user_education::FeaturePromoClosedReason::kMaxValue)) {
    promo_data->last_dismissed_by =
        user_education::FeaturePromoClosedReason::kCancel;
  } else if (last_dismissed_by) {
    promo_data->last_dismissed_by =
        static_cast<user_education::FeaturePromoClosedReason>(
            *last_dismissed_by);
  }

  if (keyed_data) {
    const auto now = GetCurrentTime();
    for (auto& keyed : *keyed_data) {
      if (const auto key_value = ReadKeyData(keyed, now)) {
        promo_data->shown_for_keys.emplace(*key_value);
      }
    }
  }

  return promo_data;
}

void BrowserFeaturePromoStorageService::SavePromoData(
    const base::Feature& iph_feature,
    const user_education::FeaturePromoData& promo_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  auto& pref_data = update.Get();

  pref_data.SetByDottedPath(path_prefix + kIPHIsDismissedPath,
                            promo_data.is_dismissed);
  pref_data.SetByDottedPath(path_prefix + kIPHLastDismissedByPath,
                            static_cast<int>(promo_data.last_dismissed_by));
  pref_data.SetByDottedPath(path_prefix + kIPHFirstShowTimePath,
                            base::TimeToValue(promo_data.first_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastShowTimePath,
                            base::TimeToValue(promo_data.last_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastSnoozeTimePath,
                            base::TimeToValue(promo_data.last_snooze_time));
  pref_data.SetByDottedPath(path_prefix + kIPHSnoozeCountPath,
                            promo_data.snooze_count);
  pref_data.SetByDottedPath(path_prefix + kIPHShowCountPath,
                            promo_data.show_count);
  pref_data.SetByDottedPath(path_prefix + kIPHPromoIndexPath,
                            promo_data.promo_index);

  base::Value::List shown_for;
  for (auto& [key, data] : promo_data.shown_for_keys) {
    shown_for.Append(WriteKeyData(key, data));
  }
  pref_data.SetByDottedPath(path_prefix + kIPHShownForAppsPath,
                            std::move(shown_for));
}

void BrowserFeaturePromoStorageService::ResetSession() {
  auto* const prefs = profile_->GetPrefs();
  prefs->ClearPref(kIPHSessionStartPath);
  prefs->ClearPref(kIPHSessionLastActiveTimePath);
}

user_education::FeaturePromoSessionData
BrowserFeaturePromoStorageService::ReadSessionData() const {
  user_education::FeaturePromoSessionData data;
  auto* const prefs = profile_->GetPrefs();
  data.start_time = prefs->GetTime(kIPHSessionStartPath);
  data.most_recent_active_time = prefs->GetTime(kIPHSessionLastActiveTimePath);
  return data;
}

void BrowserFeaturePromoStorageService::SaveSessionData(
    const user_education::FeaturePromoSessionData& session_data) {
  auto* const prefs = profile_->GetPrefs();

  // Only write session start time if it has changed.
  const auto old_session_time = prefs->GetTime(kIPHSessionStartPath);
  if (old_session_time != session_data.start_time) {
    prefs->SetTime(kIPHSessionStartPath, session_data.start_time);
  }

  // This is a "lossy" pref which means we can write it whenever; it will get
  // written through to disk occasionally during operation and and shutdown.
  prefs->SetTime(kIPHSessionLastActiveTimePath,
                 session_data.most_recent_active_time);
}

user_education::FeaturePromoPolicyData
BrowserFeaturePromoStorageService::ReadPolicyData() const {
  user_education::FeaturePromoPolicyData data;
  data.last_heavyweight_promo_time =
      profile_->GetPrefs()->GetTime(kIPHPolicyLastHeavyweightPromoPath);
  return data;
}

void BrowserFeaturePromoStorageService::SavePolicyData(
    const user_education::FeaturePromoPolicyData& policy_data) {
  profile_->GetPrefs()->SetTime(kIPHPolicyLastHeavyweightPromoPath,
                                policy_data.last_heavyweight_promo_time);
}

void BrowserFeaturePromoStorageService::ResetPolicy() {
  profile_->GetPrefs()->ClearPref(kIPHPolicyLastHeavyweightPromoPath);
}

void BrowserFeaturePromoStorageService::ResetNewBadge(
    const base::Feature& new_badge_feature) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kNewBadgePath);
  update->RemoveByDottedPath(new_badge_feature.name);
}

user_education::NewBadgeData
BrowserFeaturePromoStorageService::ReadNewBadgeData(
    const base::Feature& new_badge_feature) const {
  const std::string path_prefix = std::string(new_badge_feature.name) + ".";

  const auto& pref_data = profile_->GetPrefs()->GetDict(kNewBadgePath);

  std::optional<int> show_count =
      pref_data.FindIntByDottedPath(path_prefix + kNewBadgeShowCountPath);
  std::optional<int> used_count =
      pref_data.FindIntByDottedPath(path_prefix + kNewBadgeUsedCountPath);
  std::optional<base::Time> enabled_time =
      base::ValueToTime(pref_data.FindByDottedPath(
          path_prefix + kNewBadgeFeatureEnabledTimePath));

  user_education::NewBadgeData new_badge_data;
  new_badge_data.show_count = show_count.value_or(0);
  new_badge_data.used_count = used_count.value_or(0);
  new_badge_data.feature_enabled_time = enabled_time.value_or(base::Time());
  return new_badge_data;
}

void BrowserFeaturePromoStorageService::SaveNewBadgeData(
    const base::Feature& new_badge_feature,
    const user_education::NewBadgeData& new_badge_data) {
  std::string path_prefix = std::string(new_badge_feature.name) + ".";

  ScopedDictPrefUpdate update(profile_->GetPrefs(), kNewBadgePath);
  auto& pref_data = update.Get();

  pref_data.SetByDottedPath(path_prefix + kNewBadgeShowCountPath,
                            new_badge_data.show_count);
  pref_data.SetByDottedPath(path_prefix + kNewBadgeUsedCountPath,
                            new_badge_data.used_count);
  pref_data.SetByDottedPath(
      path_prefix + kNewBadgeFeatureEnabledTimePath,
      base::TimeToValue(new_badge_data.feature_enabled_time));
}

RecentSessionData BrowserFeaturePromoStorageService::ReadRecentSessionData()
    const {
  const auto& pref_data =
      profile_->GetPrefs()->GetList(kRecentSessionStartTimesPath);
  RecentSessionData data;
  std::optional<base::Time> prev;
  for (const auto& entry : pref_data) {
    // Ensure that the data is valid and correctly ordered. This guards against
    // corruption in the stored data causing logic errors in the program.
    const auto time = base::ValueToTime(entry);
    if (time && (!prev || *time < *prev)) {
      data.recent_session_start_times.emplace_back(*time);
      prev = time;
    }
  }

  // Get the time the feature was enabled.
  //
  // TODO(dfried): we could cull all data from before the enabled time, but
  // handling that case is probably something best left to the processing
  // logic.
  const auto enabled_time =
      profile_->GetPrefs()->GetTime(kRecentSessionEnabledTimePath);
  if (!enabled_time.is_null()) {
    data.enabled_time = enabled_time;
  }
  return data;
}

void BrowserFeaturePromoStorageService::SaveRecentSessionData(
    const RecentSessionData& recent_session_data) {
  ScopedListPrefUpdate update(profile_->GetPrefs(),
                              kRecentSessionStartTimesPath);
  auto& pref_data = update.Get();
  // `recent_session_data` contains the pref data, so rewrite the pref.
  pref_data.clear();
  for (const auto& time : recent_session_data.recent_session_start_times) {
    pref_data.Append(base::TimeToValue(time));
  }

  if (recent_session_data.enabled_time) {
    profile_->GetPrefs()->SetTime(kRecentSessionEnabledTimePath,
                                  *recent_session_data.enabled_time);
  } else {
    profile_->GetPrefs()->ClearPref(kRecentSessionEnabledTimePath);
  }
}

void BrowserFeaturePromoStorageService::ResetRecentSessionData() {
  ClearRecentSessionData(*profile_);
}
