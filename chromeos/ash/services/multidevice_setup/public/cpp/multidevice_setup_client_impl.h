// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace multidevice_setup {

// Concrete implementation of MultiDeviceSetupClient.
class MultiDeviceSetupClientImpl : public MultiDeviceSetupClient,
                                   public mojom::HostStatusObserver,
                                   public mojom::FeatureStateObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<MultiDeviceSetupClient> Create(
        mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupClient> CreateInstance(
        mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup) = 0;

   private:
    static Factory* test_factory_;
  };

  MultiDeviceSetupClientImpl(const MultiDeviceSetupClientImpl&) = delete;
  MultiDeviceSetupClientImpl& operator=(const MultiDeviceSetupClientImpl&) =
      delete;

  ~MultiDeviceSetupClientImpl() override;

  // MultiDeviceSetupClient:
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      const std::string& auth_token,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  const HostStatusWithDevice& GetHostStatus() const override;
  void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const std::optional<std::string>& auth_token,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback)
      override;
  const FeatureStatesMap& GetFeatureStates() const override;
  void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback)
      override;
  void SetQuickStartPhoneInstanceID(
      const std::string& qs_phone_instance_id) override;

  // mojom::HostStatusObserver:
  void OnHostStatusChanged(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDevice>& host_device) override;

  // mojom::FeatureStateObserver:
  void OnFeatureStatesChanged(
      const FeatureStatesMap& feature_states_map) override;

 private:
  friend class MultiDeviceSetupClientImplTest;

  explicit MultiDeviceSetupClientImpl(
      mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup);

  void OnGetEligibleHostDevicesCompleted(
      GetEligibleHostDevicesCallback callback,
      const multidevice::RemoteDeviceList& eligible_host_devices);

  void OnFeatureStateMetricTimerFired();

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

  // We want to delay the initial logging of the feature states map to better
  // understand the frequency of when the client is not ready, and we are stuck
  // waiting for the device sync.
  base::OneShotTimer initial_feature_state_metric_logging_timer_;
  base::RepeatingTimer feature_state_metric_timer_;

  HostStatusWithDevice host_status_with_device_;
  FeatureStatesMap feature_states_map_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
