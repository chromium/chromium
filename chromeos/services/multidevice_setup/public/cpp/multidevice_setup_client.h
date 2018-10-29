// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace multidevice_setup {

// Provides clients access to the MultiDeviceSetup API.
class MultiDeviceSetupClient {
 public:
  using HostStatusWithDevice =
      std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDeviceRef>>;
  using FeatureStatesMap = base::flat_map<mojom::Feature, mojom::FeatureState>;

  class Observer {
   public:
    // Called whenever the host status changes. If the host status is
    // HostStatus::kNoEligibleHosts or
    // HostStatus::kEligibleHostExistsButNoHostSet, the provided RemoteDeviceRef
    // will be null.
    virtual void OnHostStatusChanged(
        const HostStatusWithDevice& host_device_with_status) {}

    // Called whenever the state of any feature has changed.
    virtual void OnFeatureStatesChanged(
        const FeatureStatesMap& feature_states_map) {}

   protected:
    virtual ~Observer() = default;
  };

  using GetEligibleHostDevicesCallback =
      base::OnceCallback<void(const cryptauth::RemoteDeviceRefList&)>;

  static HostStatusWithDevice GenerateDefaultHostStatusWithDevice();
  static FeatureStatesMap GenerateDefaultFeatureStatesMap();

  MultiDeviceSetupClient();
  virtual ~MultiDeviceSetupClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void GetEligibleHostDevices(
      GetEligibleHostDevicesCallback callback) = 0;
  virtual void SetHostDevice(
      const std::string& host_device_id,
      const std::string& auth_token,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) = 0;
  virtual void RemoveHostDevice() = 0;
  virtual const HostStatusWithDevice& GetHostStatus() const = 0;
  virtual void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const base::Optional<std::string>& auth_token,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) = 0;
  virtual const FeatureStatesMap& GetFeatureStates() const = 0;
  mojom::FeatureState GetFeatureState(mojom::Feature feature) const;
  virtual void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) = 0;
  virtual void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) = 0;

 protected:
  void NotifyHostStatusChanged(
      const HostStatusWithDevice& host_status_with_device);
  void NotifyFeatureStateChanged(const FeatureStatesMap& feature_states_map);

 private:
  friend class MultiDeviceSetupClientImplTest;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClient);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
