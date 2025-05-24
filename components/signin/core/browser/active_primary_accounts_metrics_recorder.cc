// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"

#include <optional>

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

namespace {

// A dictionary mapping base64-encoded Gaia ID hash to last-active timestamp.
constexpr char kActiveAccountsPrefName[] = "signin.active_accounts";

// A dictionary mapping base64-encoded Gaia ID hash to "is managed account"
// boolean.
constexpr char kActiveAccountsManagedPrefName[] =
    "signin.active_accounts_managed";

#if BUILDFLAG(IS_IOS)
// A list of timestamps of recent account switches.
constexpr char kAccountSwitchTimestampsPrefName[] =
    "signin.account_switch_timestamps";
#endif  // BUILDFLAG(IS_IOS)

constexpr char kTimerPrefName[] = "signin.active_accounts_last_emitted";

// How often the metrics get emitted.
constexpr base::TimeDelta kMetricsEmissionInterval = base::Hours(24);

// The minimum interval between successive updates to an account's
// last-active-time, to prevent excessive pref writes.
constexpr base::TimeDelta kMinPrefUpdateInterval = base::Hours(1);

}  // namespace

ActivePrimaryAccountsMetricsRecorder::ActivePrimaryAccountsMetricsRecorder(
    PrefService& local_state)
    : local_state_(local_state) {
  timer_ = std::make_unique<PersistentRepeatingTimer>(
      &local_state_.get(), kTimerPrefName, kMetricsEmissionInterval,
      base::BindRepeating(&ActivePrimaryAccountsMetricsRecorder::OnTimerFired,
                          base::Unretained(this)));
  timer_->Start();
}

ActivePrimaryAccountsMetricsRecorder::~ActivePrimaryAccountsMetricsRecorder() =
    default;

// static
void ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kActiveAccountsPrefName);
  registry->RegisterDictionaryPref(kActiveAccountsManagedPrefName);
#if BUILDFLAG(IS_IOS)
  registry->RegisterListPref(kAccountSwitchTimestampsPrefName);
#endif  // BUILDFLAG(IS_IOS)
  registry->RegisterTimePref(kTimerPrefName, base::Time());
}

void ActivePrimaryAccountsMetricsRecorder::MarkAccountAsActiveNow(
    const GaiaId& gaia_id,
    Tribool is_managed_account) {
  const base::Time now = base::Time::Now();

  // The metrics about active accounts aren't that fine-grained; don't bother
  // updating the pref too often.
  if (now - GetLastActiveTimeForAccount(gaia_id).value_or(base::Time()) <
      kMinPrefUpdateInterval) {
    return;
  }

  const std::string key = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
  ScopedDictPrefUpdate active_accounts_pref_update(&local_state_.get(),
                                                   kActiveAccountsPrefName);
  active_accounts_pref_update.Get().Set(key, base::TimeToValue(now));
  if (is_managed_account != Tribool::kUnknown) {
    ScopedDictPrefUpdate managed_accounts_pref_update(
        &local_state_.get(), kActiveAccountsManagedPrefName);
    managed_accounts_pref_update.Get().Set(
        key, TriboolToBoolOrDie(is_managed_account));
  }
}

void ActivePrimaryAccountsMetricsRecorder::MarkAccountAsManaged(
    const GaiaId& gaia_id,
    bool is_managed_account) {
  const base::Value::Dict& active_accounts =
      local_state_->GetDict(kActiveAccountsPrefName);
  const std::string key = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
  if (!active_accounts.contains(key)) {
    return;
  }

  ScopedDictPrefUpdate managed_accounts_pref_update(
      &local_state_.get(), kActiveAccountsManagedPrefName);
  managed_accounts_pref_update.Get().Set(key, is_managed_account);
}

#if BUILDFLAG(IS_IOS)
void ActivePrimaryAccountsMetricsRecorder::AccountWasSwitched() {
  const base::Time now = base::Time::Now();

  ScopedListPrefUpdate switch_timestamps_pref_update(
      &local_state_.get(), kAccountSwitchTimestampsPrefName);
  switch_timestamps_pref_update->Append(base::TimeToValue(now));

  // Ensure the number of entries in the pref doesn't grow unreasonably large.
  constexpr size_t kMaxTimestamps = 100;
  if (switch_timestamps_pref_update->size() > kMaxTimestamps) {
    size_t entries_to_erase =
        switch_timestamps_pref_update->size() - kMaxTimestamps;
    switch_timestamps_pref_update->erase(
        switch_timestamps_pref_update->begin(),
        switch_timestamps_pref_update->begin() + entries_to_erase);
  }
}
#endif  // BUILDFLAG(IS_IOS)

std::optional<base::Time>
ActivePrimaryAccountsMetricsRecorder::GetLastActiveTimeForAccount(
    const GaiaId& gaia_id) const {
  return base::ValueToTime(
      local_state_->GetDict(kActiveAccountsPrefName)
          .Find(GaiaIdHash::FromGaiaId(gaia_id).ToBase64()));
}

void ActivePrimaryAccountsMetricsRecorder::OnTimerFired() {
  CleanUpExpiredEntries();
  EmitMetrics();
}

void ActivePrimaryAccountsMetricsRecorder::EmitMetrics() {
  const base::Time now = base::Time::Now();

  int accounts_in_7_days = 0;
  int accounts_in_28_days = 0;
  int managed_accounts_in_7_days = 0;
  int managed_accounts_in_28_days = 0;
  const base::Value::Dict& accounts_timestamps =
      local_state_->GetDict(kActiveAccountsPrefName);
  const base::Value::Dict& accounts_managed =
      local_state_->GetDict(kActiveAccountsManagedPrefName);
  for (const auto [gaia_id_hash, last_active] : accounts_timestamps) {
    base::Time last_active_time =
        base::ValueToTime(last_active).value_or(base::Time());
    bool is_managed = accounts_managed.FindBool(gaia_id_hash).value_or(false);
    if (now - last_active_time <= base::Days(7)) {
      ++accounts_in_7_days;
      if (is_managed) {
        ++managed_accounts_in_7_days;
      }
    }
    if (now - last_active_time <= base::Days(28)) {
      ++accounts_in_28_days;
      if (is_managed) {
        ++managed_accounts_in_28_days;
      }
    }
  }

  base::UmaHistogramCounts100("Signin.NumberOfActiveAccounts.Last7Days",
                              accounts_in_7_days);
  base::UmaHistogramCounts100("Signin.NumberOfActiveAccounts.Last28Days",
                              accounts_in_28_days);

  base::UmaHistogramCounts100("Signin.NumberOfActiveManagedAccounts.Last7Days",
                              managed_accounts_in_7_days);
  base::UmaHistogramCounts100("Signin.NumberOfActiveManagedAccounts.Last28Days",
                              managed_accounts_in_28_days);

  if (managed_accounts_in_7_days > 0) {
    base::UmaHistogramCounts100(
        "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days",
        accounts_in_7_days);
  }
  if (managed_accounts_in_28_days > 0) {
    base::UmaHistogramCounts100(
        "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days",
        accounts_in_28_days);
  }

#if BUILDFLAG(IS_IOS)
  int switches_in_7_days = 0;
  int switches_in_28_days = 0;
  const base::Value::List& account_switch_timestamps =
      local_state_->GetList(kAccountSwitchTimestampsPrefName);
  for (const base::Value& timestamp : account_switch_timestamps) {
    base::Time switch_time =
        base::ValueToTime(timestamp).value_or(base::Time());
    if (now - switch_time <= base::Days(7)) {
      ++switches_in_7_days;
    }
    if (now - switch_time <= base::Days(28)) {
      ++switches_in_28_days;
    }
  }

  base::UmaHistogramCounts100("Signin.IOSNumberOfAccountSwitches.Last7Days",
                              switches_in_7_days);
  base::UmaHistogramCounts100("Signin.IOSNumberOfAccountSwitches.Last28Days",
                              switches_in_28_days);
#endif  // BUILDFLAG(IS_IOS)
}

void ActivePrimaryAccountsMetricsRecorder::CleanUpExpiredEntries() {
  const base::Time now = base::Time::Now();

  // Clean up all entries older than 28 days.
  const base::Value::Dict& active_accounts =
      local_state_->GetDict(kActiveAccountsPrefName);
  std::set<std::string> gaia_id_hashes_to_remove;
  for (const auto [gaia_id_hash, last_active] : active_accounts) {
    if (now - base::ValueToTime(last_active).value_or(base::Time()) >
        base::Days(28)) {
      gaia_id_hashes_to_remove.insert(gaia_id_hash);
    }
  }

  // Also clean up any managed-account entries that don't have a corresponding
  // timestamp. That generally shouldn't happen; this is just to ensure no data
  // gets "leaked".
  const base::Value::Dict& managed_accounts =
      local_state_->GetDict(kActiveAccountsManagedPrefName);
  for (const auto [gaia_id_hash, managed] : managed_accounts) {
    if (!active_accounts.contains(gaia_id_hash)) {
      gaia_id_hashes_to_remove.insert(gaia_id_hash);
    }
  }

  if (!gaia_id_hashes_to_remove.empty()) {
    ScopedDictPrefUpdate active_accounts_pref_update(&local_state_.get(),
                                                     kActiveAccountsPrefName);
    ScopedDictPrefUpdate managed_accounts_pref_update(
        &local_state_.get(), kActiveAccountsManagedPrefName);
    for (const std::string& gaia_id_hash : gaia_id_hashes_to_remove) {
      active_accounts_pref_update.Get().Remove(gaia_id_hash);
      managed_accounts_pref_update.Get().Remove(gaia_id_hash);
    }
  }

#if BUILDFLAG(IS_IOS)
  const base::Value::List& old_timestamps =
      local_state_->GetList(kAccountSwitchTimestampsPrefName);
  base::Value::List new_timestamps;
  for (const base::Value& timestamp : old_timestamps) {
    base::Time switch_time =
        base::ValueToTime(timestamp).value_or(base::Time());
    if (now - switch_time <= base::Days(28)) {
      new_timestamps.Append(timestamp.Clone());
    }
  }
  if (new_timestamps.size() != old_timestamps.size()) {
    local_state_->SetList(kAccountSwitchTimestampsPrefName,
                          std::move(new_timestamps));
  }
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace signin
