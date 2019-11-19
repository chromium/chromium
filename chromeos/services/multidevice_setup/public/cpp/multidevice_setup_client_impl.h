// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {

namespace multidevice_setup {

// Concrete implementation of MultiDeviceSetupClient.
class MultiDeviceSetupClientImpl : public MultiDeviceSetupClient,
                                   public mojom::HostStatusObserver,
                                   public mojom::FeatureStateObserver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupClient> BuildInstance(
        service_manager::Connector* connector);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupClientImpl() override;

  // MultiDeviceSetupClient:
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(
      const std::string& host_device_id,
      const std::string& auth_token,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  const HostStatusWithDevice& GetHostStatus() const override;
  void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const base::Optional<std::string>& auth_token,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback)
      override;
  const FeatureStatesMap& GetFeatureStates() const override;
  void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback)
      override;

  // mojom::HostStatusObserver:
  void OnHostStatusChanged(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDevice>& host_device) override;

  // mojom::FeatureStateObserver:
  void OnFeatureStatesChanged(
      const FeatureStatesMap& feature_states_map) override;

 private:
  friend class MultiDeviceSetupClientImplTest;

  explicit MultiDeviceSetupClientImpl(service_manager::Connector* connector);

  void OnGetEligibleHostDevicesCompleted(
      GetEligibleHostDevicesCallback callback,
      const multidevice::RemoteDeviceList& eligible_host_devices);

  mojo::PendingRemote<mojom::HostStatusObserver>
  GenerateHostStatusObserverRemote();
  mojo::PendingRemote<mojom::FeatureStateObserver>
  GenerateFeatureStatesObserverRemote();

  void FlushForTesting();

  mojo::Remote<mojom::MultiDeviceSetup> multidevice_setup_remote_;
  mojo::Receiver<mojom::HostStatusObserver> host_status_observer_receiver_{
      this};
  mojo::Receiver<mojom::FeatureStateObserver> feature_state_observer_receiver_{
      this};
  std::unique_ptr<multidevice::RemoteDeviceCache> remote_device_cache_;

  HostStatusWithDevice host_status_with_device_;
  FeatureStatesMap feature_states_map_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClientImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
