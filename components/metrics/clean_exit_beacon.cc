// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {
namespace {

// Denotes whether Chrome should perform clean shutdown steps: signaling that
// Chrome is exiting cleanly and then CHECKing that is has shutdown cleanly.
// This may be modified by SkipCleanShutdownStepsForTesting().
bool g_skip_clean_shutdown_steps = false;

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
    local_state->CommitPendingWrite();
  }
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           base::clamp(num_crashes, 0, 100));
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

#if defined(OS_WIN) || defined(OS_IOS)
  // An enumeration of all possible permutations of the the beacon state in the
  // registry (Windows) or NSUserDefaults (iOS) and in Local State.
  enum class CleanExitBeaconConsistency {
    kCleanClean = 0,
    kCleanDirty = 1,
    kCleanMissing = 2,
    kDirtyClean = 3,
    kDirtyDirty = 4,
    kDirtyMissing = 5,
    kMissingClean = 6,
    kMissingDirty = 7,
    kMissingMissing = 8,
    kMaxValue = kMissingMissing,
  };
  CleanExitBeaconConsistency consistency =
      CleanExitBeaconConsistency::kDirtyDirty;

  bool local_state_beacon_is_missing =
      !local_state_->HasPrefPath(prefs::kStabilityExitedCleanly);
  bool local_state_was_last_shutdown_clean = did_previous_session_exit_cleanly_;

  bool backup_beacon_was_last_shutdown_clean = true;
  bool backup_beacon_is_missing = false;
#if defined(OS_WIN)
  base::win::RegKey regkey;
  DWORD value = 0u;
  if (regkey.Open(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                  KEY_ALL_ACCESS) == ERROR_SUCCESS &&
      regkey.ReadValueDW(
          base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(), &value) ==
          ERROR_SUCCESS) {
    backup_beacon_was_last_shutdown_clean = value ? true : false;
  } else {
    backup_beacon_is_missing = true;
  }
#elif defined(OS_IOS)
  if (HasUserDefaultsBeacon()) {
    backup_beacon_was_last_shutdown_clean = GetUserDefaultsBeacon();
  } else {
    backup_beacon_is_missing = true;
  }
#endif  // defined(OS_IOS)

  if (backup_beacon_is_missing) {
    if (local_state_beacon_is_missing) {
      consistency = CleanExitBeaconConsistency::kMissingMissing;
    } else {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kMissingClean
                        : CleanExitBeaconConsistency::kMissingDirty;
    }
  } else {
    if (local_state_beacon_is_missing) {
      consistency = backup_beacon_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kCleanMissing
                        : CleanExitBeaconConsistency::kDirtyMissing;
    } else if (backup_beacon_was_last_shutdown_clean) {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kCleanClean
                        : CleanExitBeaconConsistency::kCleanDirty;
    } else {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kDirtyClean
                        : CleanExitBeaconConsistency::kDirtyDirty;
    }
  }
  base::UmaHistogramEnumeration("UMA.CleanExitBeaconConsistency2", consistency);

#if defined(OS_IOS)
  if (ShouldUseUserDefaultsBeacon())
    did_previous_session_exit_cleanly_ = backup_beacon_was_last_shutdown_clean;
#endif
#endif  // defined(OS_WIN) || defined(OS_IOS)

  MaybeIncrementCrashStreak(did_previous_session_exit_cleanly_, local_state_);
}

CleanExitBeacon::~CleanExitBeacon() = default;

void CleanExitBeacon::WriteBeaconValue(bool exited_cleanly,
                                       bool write_synchronously,
                                       bool update_beacon) {
  if (g_skip_clean_shutdown_steps)
    return;

  UpdateLastLiveTimestamp();
  if (update_beacon)
    local_state_->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);

  if (write_synchronously) {
    {
      // Time the write for two experiment groups: the group which only writes
      // prefs and the group which updates and writes prefs.
      SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
          "Variations.ExtendedSafeMode.WritePrefsTime");
      local_state_->CommitPendingWriteSynchronously();
    }
  } else {
    local_state_->CommitPendingWrite();
  }

#if defined(OS_WIN)
  base::win::RegKey regkey;
  if (regkey.Create(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                    KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    regkey.WriteValue(base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(),
                      exited_cleanly ? 1u : 0u);
  }
#elif defined(OS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
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
  if (!g_skip_clean_shutdown_steps)
    CHECK(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
}

// static
void CleanExitBeacon::SetStabilityExitedCleanlyForTesting(
    PrefService* local_state,
    bool exited_cleanly) {
  local_state->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);
#if defined(OS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // defined(OS_IOS)
}

// static
void CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(
    PrefService* local_state) {
  local_state->ClearPref(prefs::kStabilityExitedCleanly);
#if defined(OS_IOS)
  ResetUserDefaultsBeacon();
#endif  // defined(OS_IOS)
}

// static
void CleanExitBeacon::SkipCleanShutdownStepsForTesting() {
  g_skip_clean_shutdown_steps = true;
}

}  // namespace metrics
