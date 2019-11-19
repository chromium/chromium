// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromeos/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

class PrefService;

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
class GcmDeviceInfoProvider;
}  // namespace device_sync

namespace multidevice_setup {

class AccountStatusChangeDelegateNotifier;
class AndroidSmsAppHelperDelegate;
class AndroidSmsAppInstallingStatusObserver;
class AndroidSmsPairingStateTracker;
class AuthTokenValidator;
class DeviceReenroller;
class EligibleHostDevicesProvider;
class GrandfatheredEasyUnlockHostDisabler;
class HostBackendDelegate;
class HostDeviceTimestampManager;
class HostStatusProvider;
class HostVerifier;
class OobeCompletionTracker;

// Concrete MultiDeviceSetup implementation.
class MultiDeviceSetupImpl : public MultiDeviceSetupBase,
                             public HostStatusProvider::Observer,
                             public FeatureStateManager::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupBase> BuildInstance(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        AuthTokenValidator* auth_token_validator,
        OobeCompletionTracker* oobe_completion_tracker,
        AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
        AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
        const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupImpl() override;

 private:
  friend class MultiDeviceSetupImplTest;

  MultiDeviceSetupImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AuthTokenValidator* auth_token_validator,
      OobeCompletionTracker* oobe_completion_tracker,
      AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
      AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
      const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider);

  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate)
      override;
  void AddHostStatusObserver(
      mojo::PendingRemote<mojom::HostStatusObserver> observer) override;
  void AddFeatureStateObserver(
      mojo::PendingRemote<mojom::FeatureStateObserver> observer) override;
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void GetEligibleActiveHostDevices(
      GetEligibleActiveHostDevicesCallback callback) override;
  void SetHostDevice(const std::string& host_device_id,
                     const std::string& auth_token,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(mojom::Feature feature,
                              bool enabled,
                              const base::Optional<std::string>& auth_token,
                              SetFeatureEnabledStateCallback callback) override;
  void GetFeatureStates(GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  // MultiDeviceSetupBase:
  void SetHostDeviceWithoutAuthToken(
      const std::string& host_device_id,
      mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback)
      override;

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // FeatureStateManager::Observer:
  void OnFeatureStatesChange(
      const FeatureStateManager::FeatureStatesMap& feature_states_map) override;

  // Attempts to set the host device, returning a boolean of whether the attempt
  // was successful.
  bool AttemptSetHost(const std::string& host_device_id);
  bool IsAuthTokenRequiredForFeatureStateChange(mojom::Feature feature,
                                                bool enabled);

  void FlushForTesting();

  std::unique_ptr<EligibleHostDevicesProvider> eligible_host_devices_provider_;
  std::unique_ptr<HostBackendDelegate> host_backend_delegate_;
  std::unique_ptr<HostVerifier> host_verifier_;
  std::unique_ptr<HostStatusProvider> host_status_provider_;
  std::unique_ptr<GrandfatheredEasyUnlockHostDisabler>
      grandfathered_easy_unlock_host_disabler_;
  std::unique_ptr<FeatureStateManager> feature_state_manager_;
  std::unique_ptr<HostDeviceTimestampManager> host_device_timestamp_manager_;
  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;
  std::unique_ptr<DeviceReenroller> device_reenroller_;
  std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
      android_sms_app_installing_host_observer_;
  AuthTokenValidator* auth_token_validator_;

  mojo::RemoteSet<mojom::HostStatusObserver> host_status_observers_;
  mojo::RemoteSet<mojom::FeatureStateObserver> feature_state_observers_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
