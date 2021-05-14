// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {
namespace {

// Increments kVariationsCrashStreak if |did_previous_session_exit_cleanly| is
// false. Also, emits the crash streak to a histogram.
void MaybeIncrementCrashStreak(bool did_previous_session_exit_cleanly,
                               PrefService* local_state) {
  // Increment the crash streak if the previous session crashed. Note that the
  // streak is not cleared if the previous run didn’t crash. Instead, it’s
  // incremented on each crash until Chrome is able to successfully fetch a new
  // seed. This way, a seed update that mostly destabilizes Chrome still results
  // in a fallback to safe mode.
  //
  // The crash streak is incremented here rather than in a variations-related
  // class for two reasons. First, the crash streak depends on the value of
  // kStabilityExitedCleanly. Second, if kVariationsCrashStreak were updated in
  // another function, any crash between CleanExitBeacon() and that function
  // would cause the crash streak to not be to incremented. A consequence of
  // failing to increment the crash streak is that variations safe mode might
  // undercount or be completely unaware of repeated crashes early on in
  // startup.
  int num_crashes =
      local_state->GetInteger(variations::prefs::kVariationsCrashStreak);
  if (!did_previous_session_exit_cleanly) {
    ++num_crashes;
    local_state->SetInteger(variations::prefs::kVariationsCrashStreak,
                            num_crashes);
  }
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           base::ClampToRange(num_crashes, 0, 100));
}

}  // namespace

CleanExitBeacon::CleanExitBeacon(const std::wstring& backup_registry_key,
                                 PrefService* local_state)
    : local_state_(local_state),
      did_previous_session_exit_cleanly_(
          local_state->GetBoolean(prefs::kStabilityExitedCleanly)),
      initial_browser_last_live_timestamp_(
          local_state->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp)),
      backup_registry_key_(backup_registry_key) {
  DCHECK_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            local_state_->GetInitializationStatus());

  MaybeIncrementCrashStreak(did_previous_session_exit_cleanly_, local_state_);

#if defined(OS_WIN)
  // An enumeration of all possible permutations of the the beacon state in the
  // registry and in Local State.
  enum {
    DIRTY_DIRTY,
    DIRTY_CLEAN,
    CLEAN_DIRTY,
    CLEAN_CLEAN,
    MISSING_DIRTY,
    MISSING_CLEAN,
    NUM_CONSISTENCY_ENUMS
  } consistency = DIRTY_DIRTY;

  base::win::RegKey regkey;
  DWORD value = 0u;
  if (regkey.Open(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                  KEY_ALL_ACCESS) == ERROR_SUCCESS &&
      regkey.ReadValueDW(
          base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(), &value) ==
          ERROR_SUCCESS) {
    if (value) {
      consistency =
          did_previous_session_exit_cleanly_ ? CLEAN_CLEAN : CLEAN_DIRTY;
    } else {
      consistency =
          did_previous_session_exit_cleanly_ ? DIRTY_CLEAN : DIRTY_DIRTY;
    }
  } else {
    consistency =
        did_previous_session_exit_cleanly_ ? MISSING_CLEAN : MISSING_DIRTY;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UMA.CleanExitBeaconConsistency", consistency, NUM_CONSISTENCY_ENUMS);
#endif  // defined(OS_WIN)
}

CleanExitBeacon::~CleanExitBeacon() = default;

void CleanExitBeacon::WriteBeaconValue(bool value) {
  UpdateLastLiveTimestamp();
  local_state_->SetBoolean(prefs::kStabilityExitedCleanly, value);

#if defined(OS_WIN)
  base::win::RegKey regkey;
  if (regkey.Create(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                    KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    regkey.WriteValue(base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(),
                      value ? 1u : 0u);
  }
#endif  // defined(OS_WIN)
}

void CleanExitBeacon::UpdateLastLiveTimestamp() {
  local_state_->SetTime(prefs::kStabilityBrowserLastLiveTimeStamp,
                        base::Time::Now());
}

// static
void CleanExitBeacon::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);

  registry->RegisterTimePref(prefs::kStabilityBrowserLastLiveTimeStamp,
                             base::Time(), PrefRegistry::LOSSY_PREF);

  // This variations-safe-mode-related pref is registered here rather than in
  // SafeSeedManager::RegisterPrefs() because the CleanExitBeacon is
  // responsible for incrementing this value. (See the comments in
  // MaybeIncrementCrashStreak() for more details.)
  registry->RegisterIntegerPref(variations::prefs::kVariationsCrashStreak, 0);
}

// static
void CleanExitBeacon::EnsureCleanShutdown(PrefService* local_state) {
  CHECK(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
}

}  // namespace metrics
