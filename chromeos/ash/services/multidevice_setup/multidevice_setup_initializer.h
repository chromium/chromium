// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_

#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class PrefService;

namespace ash {

namespace device_sync {
class GcmDeviceInfoProvider;
}

namespace multidevice_setup {

class AndroidSmsAppHelperDelegate;
class AndroidSmsPairingStateTracker;
class AuthTokenValidator;
class OobeCompletionTracker;

// Initializes the MultiDeviceSetup service. This class is responsible for
// waiting for asynchronous initialization steps to complete before creating
// the concrete implementation of the mojom::MultiDeviceSetup interface.
class MultiDeviceSetupInitializer
    : public MultiDeviceSetupBase,
      public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<MultiDeviceSetupBase> Create(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        AuthTokenValidator* auth_token_validator,
        OobeCompletionTracker* oobe_completion_tracker,
        AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
        AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
        const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
        bool is_secondary_user);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupBase> CreateInstance(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        AuthTokenValidator* auth_token_validator,
        OobeCompletionTracker* oobe_completion_tracker,
        AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
        AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
        const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
        bool is_secondary_user) = 0;

   private:
    static Factory* test_factory_;
  };

  MultiDeviceSetupInitializer(const MultiDeviceSetupInitializer&) = delete;
  MultiDeviceSetupInitializer& operator=(const MultiDeviceSetupInitializer&) =
      delete;

  ~MultiDeviceSetupInitializer() override;

 private:
  // Used for both SetHostDevice() and SetHostDeviceWithoutAuthToken().
  struct SetHostDeviceArgs {
    // For SetHostDevice().
    SetHostDeviceArgs(const std::string& host_instance_id_or_legacy_device_id,
                      const std::string& auth_token,
                      SetHostDeviceCallback callback);

    // For SetHostDeviceWithoutAuthToken().
    SetHostDeviceArgs(
        const std::string& host_instance_id_or_legacy_device_id,
        mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback);

    ~SetHostDeviceArgs();

    std::string host_instance_id_or_legacy_device_id;
    // Null for SetHostDeviceWithoutAuthToken().
    std::optional<std::string> auth_token;
    base::OnceCallback<void(bool)> callback;
  };

  MultiDeviceSetupInitializer(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AuthTokenValidator* auth_token_validator,
      OobeCompletionTracker* oobe_completion_tracker,
      AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
      AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
      const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
      bool is_secondary_user);

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
  void SetHostDevice(const std::string& host_instance_id_or_legacy_device_id,
                     const std::string& auth_token,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(mojom::Feature feature,
                              bool enabled,
                              const std::optional<std::string>& auth_token,
                              SetFeatureEnabledStateCallback callback) override;
  void GetFeatureStates(GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;
  void SetQuickStartPhoneInstanceID(
      const std::string& qs_phone_instance_id) override;
  void GetQuickStartPhoneInstanceID(
      GetQuickStartPhoneInstanceIDCallback callback) override;

  // MultiDeviceSetupBase:
  void SetHostDeviceWithoutAuthToken(
      const std::string& host_instance_id_or_legacy_device_id,
      mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback)
      override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;

  void InitializeImplementation();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<AuthTokenValidator> auth_token_validator_;
  raw_ptr<OobeCompletionTracker> oobe_completion_tracker_;
  raw_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;
  raw_ptr<AndroidSmsPairingStateTracker> android_sms_pairing_state_tracker_;
  raw_ptr<const device_sync::GcmDeviceInfoProvider> gcm_device_info_provider_;
  bool is_secondary_user_;

  std::unique_ptr<MultiDeviceSetupBase> multidevice_setup_impl_;

  // If API functions are called before initialization is complete, their
  // parameters are cached here. Once asynchronous initialization is complete,
  // the parameters are passed to |multidevice_setup_impl_|.
  mojo::PendingRemote<mojom::AccountStatusChangeDelegate> pending_delegate_;
  std::vector<mojo::PendingRemote<mojom::HostStatusObserver>>
      pending_host_status_observers_;
  std::vector<mojo::PendingRemote<mojom::FeatureStateObserver>>
      pending_feature_state_observers_;
  std::vector<GetEligibleHostDevicesCallback> pending_get_eligible_hosts_args_;
  std::vector<GetEligibleActiveHostDevicesCallback>
      pending_get_eligible_active_hosts_args_;
  std::vector<GetHostStatusCallback> pending_get_host_args_;
  std::vector<std::tuple<mojom::Feature,
                         bool,
                         std::optional<std::string>,
                         SetFeatureEnabledStateCallback>>
      pending_set_feature_enabled_args_;
  std::vector<GetFeatureStatesCallback> pending_get_feature_states_args_;
  std::vector<RetrySetHostNowCallback> pending_retry_set_host_args_;
  std::vector<std::string> pending_set_qs_phone_instance_id_args_;
  std::vector<GetQuickStartPhoneInstanceIDCallback>
      pending_get_qs_phone_instance_id_args_;

  // Special case: for SetHostDevice(), SetHostDeviceWithoutAuthToken(), and
  // RemoveHostDevice(), only keep track of the most recent call. Since each
  // call to either of these functions overwrites the previous call, only one
  // needs to be passed.
  std::optional<SetHostDeviceArgs> pending_set_host_args_;
  bool pending_should_remove_host_device_ = false;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
