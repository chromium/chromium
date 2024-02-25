// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_SOFTWARE_FEATURE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_SOFTWARE_FEATURE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/software_feature_manager.h"

namespace ash {

namespace device_sync {

// Test implementation of SoftwareFeatureManager.
class FakeSoftwareFeatureManager : public SoftwareFeatureManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSetSoftwareFeatureStateCalled() {}
    virtual void OnSetFeatureStatusCalled() {}
    virtual void OnFindEligibleDevicesCalled() {}
  };

  struct SetSoftwareFeatureStateArgs {
    SetSoftwareFeatureStateArgs(
        const std::string& public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        base::OnceClosure success_callback,
        base::OnceCallback<void(NetworkRequestError)> error_callback,
        bool is_exclusive);

    SetSoftwareFeatureStateArgs(const SetSoftwareFeatureStateArgs&) = delete;
    SetSoftwareFeatureStateArgs& operator=(const SetSoftwareFeatureStateArgs&) =
        delete;

    ~SetSoftwareFeatureStateArgs();

    std::string public_key;
    multidevice::SoftwareFeature software_feature;
    bool enabled;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
    bool is_exclusive;
  };

  struct SetFeatureStatusArgs {
    SetFeatureStatusArgs(
        const std::string& device_id,
        multidevice::SoftwareFeature feature,
        FeatureStatusChange status_change,
        base::OnceClosure success_callback,
        base::OnceCallback<void(NetworkRequestError)> error_callback);

    SetFeatureStatusArgs(const SetFeatureStatusArgs&) = delete;
    SetFeatureStatusArgs& operator=(const SetFeatureStatusArgs&) = delete;

    ~SetFeatureStatusArgs();

    std::string device_id;
    multidevice::SoftwareFeature feature;
    FeatureStatusChange status_change;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  struct FindEligibleDevicesArgs {
    FindEligibleDevicesArgs(
        multidevice::SoftwareFeature software_feature,
        base::OnceCallback<void(
            const std::vector<cryptauth::ExternalDeviceInfo>&,
            const std::vector<cryptauth::IneligibleDevice>&)> success_callback,
        base::OnceCallback<void(NetworkRequestError)> error_callback);

    FindEligibleDevicesArgs(const FindEligibleDevicesArgs&) = delete;
    FindEligibleDevicesArgs& operator=(const FindEligibleDevicesArgs&) = delete;

    ~FindEligibleDevicesArgs();

    multidevice::SoftwareFeature software_feature;
    base::OnceCallback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                            const std::vector<cryptauth::IneligibleDevice>&)>
        success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  FakeSoftwareFeatureManager();

  FakeSoftwareFeatureManager(const FakeSoftwareFeatureManager&) = delete;
  FakeSoftwareFeatureManager& operator=(const FakeSoftwareFeatureManager&) =
      delete;

  ~FakeSoftwareFeatureManager() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  const std::vector<std::unique_ptr<SetSoftwareFeatureStateArgs>>&
  set_software_feature_state_calls() {
    return set_software_feature_state_calls_;
  }

  const std::vector<std::unique_ptr<SetFeatureStatusArgs>>&
  set_feature_status_calls() {
    return set_feature_status_calls_;
  }

  const std::vector<std::unique_ptr<FindEligibleDevicesArgs>>&
  find_eligible_multidevice_host_calls() {
    return find_eligible_multidevice_host_calls_;
  }

  // SoftwareFeatureManager:
  void SetSoftwareFeatureState(
      const std::string& public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback,
      bool is_exclusive = false) override;
  void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;
  void FindEligibleDevices(
      multidevice::SoftwareFeature software_feature,
      base::OnceCallback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>
          success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

 private:
  raw_ptr<Delegate> delegate_ = nullptr;

  std::vector<std::unique_ptr<SetSoftwareFeatureStateArgs>>
      set_software_feature_state_calls_;
  std::vector<std::unique_ptr<SetFeatureStatusArgs>> set_feature_status_calls_;
  std::vector<std::unique_ptr<FindEligibleDevicesArgs>>
      find_eligible_multidevice_host_calls_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_SOFTWARE_FEATURE_MANAGER_H_
