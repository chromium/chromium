// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_

#include <memory>

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

class PrefService;
class PrefRegistrySimple;

namespace cryptauth {
class GcmDeviceInfoProvider;
}  // namespace cryptauth

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
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
class MultiDeviceSetupService : public service_manager::Service {
 public:
  MultiDeviceSetupService(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AuthTokenValidator* auth_token_validator,
      OobeCompletionTracker* oobe_completion_tracker,
      std::unique_ptr<AndroidSmsAppHelperDelegate>
          android_sms_app_helper_delegate,
      std::unique_ptr<AndroidSmsPairingStateTracker>
          android_sms_pairing_state_tracker,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider);
  ~MultiDeviceSetupService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  std::unique_ptr<MultiDeviceSetupBase> multidevice_setup_;
  std::unique_ptr<PrivilegedHostDeviceSetterBase>
      privileged_host_device_setter_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupService);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_H_
