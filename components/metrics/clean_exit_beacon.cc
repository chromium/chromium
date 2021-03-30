// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {

CleanExitBeacon::CleanExitBeacon(const std::wstring& backup_registry_key,
                                 PrefService* local_state)
    : local_state_(local_state),
      initial_value_(local_state->GetBoolean(prefs::kStabilityExitedCleanly)),
      initial_browser_last_live_timestamp_(
          local_state->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp)),
      backup_registry_key_(backup_registry_key) {
  DCHECK_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            local_state_->GetInitializationStatus());

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
    if (value)
      consistency = initial_value_ ? CLEAN_CLEAN : CLEAN_DIRTY;
    else
      consistency = initial_value_ ? DIRTY_CLEAN : DIRTY_DIRTY;
  } else {
    consistency = initial_value_ ? MISSING_CLEAN : MISSING_DIRTY;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UMA.CleanExitBeaconConsistency", consistency, NUM_CONSISTENCY_ENUMS);
#endif
}

CleanExitBeacon::~CleanExitBeacon() {
}

// static
void CleanExitBeacon::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);

  registry->RegisterTimePref(prefs::kStabilityBrowserLastLiveTimeStamp,
                             base::Time(), PrefRegistry::LOSSY_PREF);
}

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
#endif
}

void CleanExitBeacon::UpdateLastLiveTimestamp() {
  local_state_->SetTime(prefs::kStabilityBrowserLastLiveTimeStamp,
                        base::Time::Now());
}

}  // namespace metrics
