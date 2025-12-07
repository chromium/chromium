// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_POLICY_CACHED_DEVICE_POLICY_UPDATER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_POLICY_CACHED_DEVICE_POLICY_UPDATER_H_

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace ash {
class FakeSessionManagerClient;
}  // namespace ash

namespace policy {

// In production, once device policy is fetched from the server, it is stored
// in session_manager daemon (so that, in the next session, even if the device
// is not connected to the network, the policy can be used).
// In tests, we use FakeSessionManagerClient as an interface to fake
// the communication with session_manager (because there's no session_manager
// daemon running). This class supports to update the device_policy cached
// in the fake session manager.
//
// Preconditions:
// - FakeSessionManagerClient is set up with PolicyStorageType::kInMemory.
//
// How to use:
//  CachedDevicePolicyUpdater policy_updater;
//  // policy_updater.policy_data()/payload() holds the current device policy.
//  // Update it as needed. E.g.:
//  policy_updater.payload().mutable_device_reporting()
//      ->set_report_login_logout(true);
//  // Finally invoke Commit() to update the policy in FakeSessionManager.
//  // Commit() must be called once exactly, to avoid forgetting to set the
//  // data, nor committing twice. Otherwise it'll be CHECK()ed.
//  policy_updater.Commit();
//
// Note: in many cases, it may need to wait for the propagation of the policy
// update. E.g., ash::CrosSettingsWaiter may be useful for the update in
// CrosSettings.
class CachedDevicePolicyUpdater {
 public:
  CachedDevicePolicyUpdater();
  CachedDevicePolicyUpdater(const CachedDevicePolicyUpdater&) = delete;
  CachedDevicePolicyUpdater& operator=(const CachedDevicePolicyUpdater&) =
      delete;
  ~CachedDevicePolicyUpdater();

  void Commit();

  enterprise_management::PolicyData& policy_data();
  const enterprise_management::PolicyData& policy_data() const;
  enterprise_management::ChromeDeviceSettingsProto& payload();
  const enterprise_management::ChromeDeviceSettingsProto& payload() const;

 private:
  const raw_ref<ash::FakeSessionManagerClient> session_manager_;
  DevicePolicyBuilder policy_builder_;

  // Tracks whether the Commit() is invoked.
  bool committed_ = false;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_POLICY_CACHED_DEVICE_POLICY_UPDATER_H_
