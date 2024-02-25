// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

// Provides clients access to the MultiDeviceSetup API.
class MultiDeviceSetupClient {
 public:
  using HostStatusWithDevice =
      std::pair<mojom::HostStatus, std::optional<multidevice::RemoteDeviceRef>>;
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
      base::OnceCallback<void(const multidevice::RemoteDeviceRefList&)>;

  static HostStatusWithDevice GenerateDefaultHostStatusWithDevice();
  static FeatureStatesMap GenerateDefaultFeatureStatesMap(
      mojom::FeatureState default_value);

  MultiDeviceSetupClient();

  MultiDeviceSetupClient(const MultiDeviceSetupClient&) = delete;
  MultiDeviceSetupClient& operator=(const MultiDeviceSetupClient&) = delete;

  virtual ~MultiDeviceSetupClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void GetEligibleHostDevices(
      GetEligibleHostDevicesCallback callback) = 0;
  virtual void SetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      const std::string& auth_token,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) = 0;
  virtual void RemoveHostDevice() = 0;
  virtual const HostStatusWithDevice& GetHostStatus() const = 0;
  virtual void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const std::optional<std::string>& auth_token,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) = 0;
  virtual const FeatureStatesMap& GetFeatureStates() const = 0;
  mojom::FeatureState GetFeatureState(mojom::Feature feature) const;
  virtual void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) = 0;
  virtual void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) = 0;
  virtual void SetQuickStartPhoneInstanceID(
      const std::string& qs_phone_instance_id) = 0;

 protected:
  void NotifyHostStatusChanged(
      const HostStatusWithDevice& host_status_with_device);
  void NotifyFeatureStateChanged(const FeatureStatesMap& feature_states_map);

 private:
  friend class MultiDeviceSetupClientImplTest;

  base::ObserverList<Observer>::Unchecked observer_list_;
};

std::string FeatureStatesMapToString(
    const MultiDeviceSetupClient::FeatureStatesMap& map);
std::string HostStatusWithDeviceToString(
    const MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device);

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
