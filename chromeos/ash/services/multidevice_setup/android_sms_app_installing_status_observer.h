// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"

class PrefService;

namespace ash {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegate;

// Listens for status changes in multidevice state and installs the Android
// Messages PWA if needed.
class AndroidSmsAppInstallingStatusObserver
    : public HostStatusProvider::Observer,
      public FeatureStateManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<AndroidSmsAppInstallingStatusObserver> Create(
        HostStatusProvider* host_status_provider,
        FeatureStateManager* feature_state_manager,
        AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
        PrefService* pref_service);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
    CreateInstance(
        HostStatusProvider* host_status_provider,
        FeatureStateManager* feature_state_manager,
        AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate) = 0;

   private:
    static Factory* test_factory_;
  };

  AndroidSmsAppInstallingStatusObserver(
      const AndroidSmsAppInstallingStatusObserver&) = delete;
  AndroidSmsAppInstallingStatusObserver& operator=(
      const AndroidSmsAppInstallingStatusObserver&) = delete;

  ~AndroidSmsAppInstallingStatusObserver() override;

 private:
  AndroidSmsAppInstallingStatusObserver(
      HostStatusProvider* host_status_provider,
      FeatureStateManager* feature_state_manager,
      AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
      PrefService* pref_service);

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // FeatureStateManager::Observer:
  void OnFeatureStatesChange(
      const FeatureStateManager::FeatureStatesMap& feature_states_map) override;

  bool DoesFeatureStateAllowInstallation();
  void UpdatePwaInstallationState();

  raw_ptr<HostStatusProvider> host_status_provider_;
  raw_ptr<FeatureStateManager> feature_state_manager_;
  raw_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;
  raw_ptr<PrefService> pref_service_;
  base::WeakPtrFactory<AndroidSmsAppInstallingStatusObserver> weak_ptr_factory_{
      this};
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_
