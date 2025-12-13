// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_
#define COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}

namespace metrics {

class MachineIdProvider;

// A struct that holds cloned install related fields in prefs that need to be
// reported in the system_profile.
struct ClonedInstallInfo {
  // The start of the latest session during which the install was detected as
  // cloned.
  int64_t last_detection_timestamp;

  int64_t last_reset_timestamp;
  int64_t first_reset_timestamp;
  int reset_count;
};

// A class for detecting if an install is cloned. It does this by detecting
// when the hardware running Chrome changes.
class ClonedInstallDetector {
 public:
  ClonedInstallDetector();

  ClonedInstallDetector(const ClonedInstallDetector&) = delete;
  ClonedInstallDetector& operator=(const ClonedInstallDetector&) = delete;

  virtual ~ClonedInstallDetector();

  // Posts a task to |task_runner| to generate a machine ID and store it to a
  // local state pref. If the newly generated ID is different than the
  // previously stored one, then the install is considered cloned. The ID is a
  // 24-bit value based off of machine characteristics. This value should never
  // be sent over the network.
  void CheckForClonedInstall(PrefService* local_state);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Reads cloned install info fields from |local_state| and returns them in
  // a ClonedInstallInfo.
  static ClonedInstallInfo ReadClonedInstallInfo(PrefService* local_state);

  // Clears cloned install info fields from |local_state|.
  static void ClearClonedInstallInfo(PrefService* local_state);

  // Updates cloned install info fields in |local_state| on reset.
  void RecordClonedInstallInfo(PrefService* local_state);

  // Returns true for the whole session if we detected a cloned install during
  // the construction of a client id.
  bool ShouldResetClientIds(PrefService* local_state);

  // Returns true for the whole session if we detect a cloned install this
  // session.
  bool ClonedInstallDetectedInCurrentSession() const;

  // Adds a callback that is run if this install is detected as cloned during
  // this session. If this is called after the detection had already occurred,
  // the callback is run immediately.
  base::CallbackListSubscription AddOnClonedInstallDetectedCallback(
      base::OnceClosure callback);

  // Wrapper around SaveMachineId(). Used for testing in UKM (the tests cannot
  // be declared as friends since they live in a different namespace).
  void SaveMachineIdForTesting(PrefService* local_state,
                               const std::string& raw_id);

  // Sets `machine_id_provider` as the global testing provider instance, that
  // gets used over the default ones used in prod.
  static void SetMachineIdProviderForTesting(
      MachineIdProvider* machine_id_provider);

 private:
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest, SaveId);
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest, DetectClone);
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest, ShouldResetClientIds);
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest,
                           ClonedInstallDetectedInCurrentSession);
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest,
                           ClonedInstallDetectedCallback);
  FRIEND_TEST_ALL_PREFIXES(MetricsStateManagerTest, CheckProviderResetIds);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest,
                           PurgeLogsOnClonedInstallDetected);

  // Converts `raw_id` into a 24-bit hash and stores the hash in `local_state`.
  // This function is called via a cross-thread, low priority posted task, and
  // `check_initiated_timestamp` corresponds to the time when the request to
  // check (get and then save) the machine ID was initiated. It will be written
  // to `local_state` if a machine ID mismatch is detected.
  void SaveMachineId(PrefService* local_state,
                     base::Time check_initiated_timestamp,
                     const std::string& raw_id);

  // Indicates that we detected a cloned install during the current session.
  bool detected_this_session_ = false;

  // Indicates that we detected a cloned install during the construction of a
  // client id and should reset client ids as a result.
  bool should_reset_client_ids_ = false;

  base::OnceClosureList callback_list_;

  base::WeakPtrFactory<ClonedInstallDetector> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_
