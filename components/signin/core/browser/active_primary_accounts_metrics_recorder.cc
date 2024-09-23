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

namespace signin {

namespace {

// A dictionary mapping base64-encoded Gaia ID hash to last-active timestamp.
constexpr char kActiveAccountsPrefName[] = "signin.active_accounts";

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
  registry->RegisterTimePref(kTimerPrefName, base::Time());
}

void ActivePrimaryAccountsMetricsRecorder::MarkAccountAsActiveNow(
    std::string_view gaia_id) {
  const base::Time now = base::Time::Now();

  // The metrics about active accounts aren't that fine-grained; don't bother
  // updating the pref too often.
  if (now - GetLastActiveTimeForAccount(gaia_id).value_or(base::Time()) <
      kMinPrefUpdateInterval) {
    return;
  }
  ScopedDictPrefUpdate pref_update(&local_state_.get(),
                                   kActiveAccountsPrefName);
  pref_update.Get().Set(GaiaIdHash::FromGaiaId(gaia_id).ToBase64(),
                        base::TimeToValue(now));
}

std::optional<base::Time>
ActivePrimaryAccountsMetricsRecorder::GetLastActiveTimeForAccount(
    std::string_view gaia_id) const {
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
  const base::Value::Dict& all_accounts =
      local_state_->GetDict(kActiveAccountsPrefName);
  for (const auto [gaia_id_hash, last_active] : all_accounts) {
    base::Time last_active_time =
        base::ValueToTime(last_active).value_or(base::Time());
    if (now - last_active_time <= base::Days(7)) {
      ++accounts_in_7_days;
    }
    if (now - last_active_time <= base::Days(28)) {
      ++accounts_in_28_days;
    }
  }

  base::UmaHistogramCounts100("Signin.NumberOfActiveAccounts.Last7Days",
                              accounts_in_7_days);
  base::UmaHistogramCounts100("Signin.NumberOfActiveAccounts.Last28Days",
                              accounts_in_28_days);
}

void ActivePrimaryAccountsMetricsRecorder::CleanUpExpiredEntries() {
  const base::Time now = base::Time::Now();

  const base::Value::Dict& all_accounts =
      local_state_->GetDict(kActiveAccountsPrefName);
  std::set<std::string> gaia_id_hashes_to_remove;
  for (const auto [gaia_id_hash, last_active] : all_accounts) {
    if (now - base::ValueToTime(last_active).value_or(base::Time()) >
        base::Days(28)) {
      gaia_id_hashes_to_remove.insert(gaia_id_hash);
    }
  }

  if (!gaia_id_hashes_to_remove.empty()) {
    ScopedDictPrefUpdate pref_update(&local_state_.get(),
                                     kActiveAccountsPrefName);
    for (const std::string& gaia_id_hash : gaia_id_hashes_to_remove) {
      pref_update.Get().Remove(gaia_id_hash);
    }
  }
}

}  // namespace signin
