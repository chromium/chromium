// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_
#define COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// A class for detecting if an install is cloned. It does this by detecting
// when the hardware running Chrome changes.
class ClonedInstallDetector {
 public:
  ClonedInstallDetector();
  virtual ~ClonedInstallDetector();

  // Posts a task to |task_runner| to generate a machine ID and store it to a
  // local state pref. If the newly generated ID is different than the
  // previously stored one, then the install is considered cloned. The ID is a
  // 24-bit value based off of machine characteristics. This value should never
  // be sent over the network.
  // TODO(jwd): Implement change detection.
  void CheckForClonedInstall(PrefService* local_state);

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest, SaveId);
  FRIEND_TEST_ALL_PREFIXES(ClonedInstallDetectorTest, DetectClone);

  // Converts raw_id into a 24-bit hash and stores the hash in |local_state|.
  // |raw_id| is not a const ref because it's passed from a cross-thread post
  // task.
  void SaveMachineId(PrefService* local_state, const std::string& raw_id);

  base::WeakPtrFactory<ClonedInstallDetector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClonedInstallDetector);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLONED_INSTALL_DETECTOR_H_
