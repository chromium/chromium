// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
#define COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Reads and updates a beacon used to detect whether the previous browser
// process exited cleanly.
class CleanExitBeacon {
 public:
  // Instantiates a CleanExitBeacon whose value is stored in |local_state|'s
  // kStabilityExitedCleanly pref. |local_state| must be fully initialized.
  //
  // On Windows, |backup_registry_key| stores a backup of the beacon to verify
  // that the pref's value corresponds to the registry's. |backup_registry_key|
  // is ignored on other platforms, but iOS has a similar verification
  // mechanism using LastSessionExitedCleanly in
  // ios/chrome/browser/pref_names.cc.
  // TODO(crbug.com/1208077): Use the CleanExitBeacon for verification on iOS.
  CleanExitBeacon(const std::wstring& backup_registry_key,
                  PrefService* local_state);

  ~CleanExitBeacon();

  // Returns the original value of the beacon.
  bool exited_cleanly() const { return did_previous_session_exit_cleanly_; }

  // Returns the original value of the last live timestamp.
  base::Time browser_last_live_timestamp() const {
    return initial_browser_last_live_timestamp_;
  }

  // Writes the provided beacon value and updates the last live timestamp.
  void WriteBeaconValue(bool exited_cleanly);

  // Updates the last live timestamp.
  void UpdateLastLiveTimestamp();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // CHECKs that Chrome exited cleanly.
  static void EnsureCleanShutdown(PrefService* local_state);

 private:
  PrefService* const local_state_;
  const bool did_previous_session_exit_cleanly_;

  // This is the value of the last live timestamp from local state at the
  // time of construction. It notes a timestamp from the previous browser
  // session when the browser was known to be alive.
  const base::Time initial_browser_last_live_timestamp_;
  const std::wstring backup_registry_key_;

  DISALLOW_COPY_AND_ASSIGN(CleanExitBeacon);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
