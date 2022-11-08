// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_

#include <memory>

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

namespace device_sync {
class DeviceSyncClient;
class GcmDeviceInfoProvider;
}  // namespace device_sync

namespace multidevice_setup {

class AndroidSmsAppHelperDelegate;
class AndroidSmsPairingStateTracker;
class AuthTokenValidator;
class MultiDeviceSetupBase;
class PrivilegedHostDeviceSetterBase;
class OobeCompletionTracker;

// Service which provides an implementation for mojom::MultiDeviceSetup. This
// service creates one implementation and shares it among all connection
// requests.
class MultiDeviceSetupService {
 public:
  MultiDeviceSetupService(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AuthTokenValidator* auth_token_validator,
      OobeCompletionTracker* oobe_completion_tracker,
      AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
      AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
      const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
      bool is_secondary_user);

  MultiDeviceSetupService(const MultiDeviceSetupService&) = delete;
  MultiDeviceSetupService& operator=(const MultiDeviceSetupService&) = delete;

  ~MultiDeviceSetupService();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void BindMultiDeviceSetup(
      mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver);
  void BindPrivilegedHostDeviceSetter(
      mojo::PendingReceiver<mojom::PrivilegedHostDeviceSetter> receiver);

 private:
  std::unique_ptr<MultiDeviceSetupBase> multidevice_setup_;
  std::unique_ptr<PrivilegedHostDeviceSetterBase>
      privileged_host_device_setter_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_
