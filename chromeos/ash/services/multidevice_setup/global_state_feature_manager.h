// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_H_

namespace ash {

namespace multidevice_setup {

// Manages the state of a feature whose host enabled state is synced across
// all connected devices. The global host enabled state will be used to
// determine whether the feature is enabled on this client device.
// Such features are different from normal features where the host enabled state
// is solely set by the host device, and a local enabled state is used to
// control whetether the feature is enabled on this client device.
class GlobalStateFeatureManager {
 public:
  virtual ~GlobalStateFeatureManager() = default;
  GlobalStateFeatureManager(const GlobalStateFeatureManager&) = delete;
  GlobalStateFeatureManager& operator=(const GlobalStateFeatureManager&) =
      delete;

  // Attempts to enable/disable the managed feature on the backend for the host
  // device that is synced at the time SetIsFeatureEnabled is called.
  virtual void SetIsFeatureEnabled(bool enabled) = 0;

  // Returns whether the managed feature is enabled/disabled.
  virtual bool IsFeatureEnabled() = 0;

 protected:
  GlobalStateFeatureManager() = default;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_H_
