// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace multidevice_setup {

const char kCanShowWifiSyncAnnouncementPrefName[] =
    "multidevice_setup.can_show_wifi_sync_announcement";

// static
WifiSyncNotificationController::Factory*
    WifiSyncNotificationController::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<WifiSyncNotificationController>
WifiSyncNotificationController::Factory::Create(
    GlobalStateFeatureManager* wifi_sync_feature_manager,
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AccountStatusChangeDelegateNotifier* delegate_notifier) {
  if (test_factory_) {
    return test_factory_->CreateInstance(wifi_sync_feature_manager,
                                         host_status_provider, pref_service,
                                         device_sync_client, delegate_notifier);
  }

  return base::WrapUnique(new WifiSyncNotificationController(
      wifi_sync_feature_manager, host_status_provider, pref_service,
      device_sync_client, delegate_notifier));
}

// static
void WifiSyncNotificationController::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

WifiSyncNotificationController::Factory::~Factory() = default;

void WifiSyncNotificationController::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCanShowWifiSyncAnnouncementPrefName, true);
}

WifiSyncNotificationController::WifiSyncNotificationController(
    GlobalStateFeatureManager* wifi_sync_feature_manager,
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AccountStatusChangeDelegateNotifier* delegate_notifier)
    : wifi_sync_feature_manager_(wifi_sync_feature_manager),
      host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      delegate_notifier_(delegate_notifier) {
  if (pref_service_->GetBoolean(kCanShowWifiSyncAnnouncementPrefName)) {
    session_manager::SessionManager::Get()->AddObserver(this);
    base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
    did_register_session_observers_ = true;
  }
}

WifiSyncNotificationController::~WifiSyncNotificationController() {
  if (did_register_session_observers_) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
    base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  }
}

void WifiSyncNotificationController::OnSessionStateChanged() {
  TRACE_EVENT0("login",
               "WifiSyncNotificationController::OnSessionStateChanged");
  ShowAnnouncementNotificationIfEligible();
}

void WifiSyncNotificationController::OnResume() {
  ShowAnnouncementNotificationIfEligible();
}

void WifiSyncNotificationController::ShowAnnouncementNotificationIfEligible() {
  TRACE_EVENT0(
      "ui",
      "WifiSyncNotificationController::ShowAnnouncementNotificationIfEligible");
  // Show the announcement notification when the device is unlocked and
  // eligible for wi-fi sync.  This is done on unlock/resume to avoid showing
  // it on the first sign-in when it would distract from showoff and other
  // announcements.

  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }

  if (!IsFeatureAllowed(mojom::Feature::kWifiSync, pref_service_)) {
    return;
  }

  if (!pref_service_->GetBoolean(kCanShowWifiSyncAnnouncementPrefName)) {
    return;
  }

  if (!IsWifiSyncSupported()) {
    return;
  }

  if (host_status_provider_->GetHostWithStatus().host_status() !=
          mojom::HostStatus::kHostVerified ||
      wifi_sync_feature_manager_->IsFeatureEnabled()) {
    pref_service_->SetBoolean(kCanShowWifiSyncAnnouncementPrefName, false);
    return;
  }

  if (!delegate_notifier_->delegate()) {
    return;
  }

  delegate_notifier_->delegate()->OnBecameEligibleForWifiSync();
  pref_service_->SetBoolean(kCanShowWifiSyncAnnouncementPrefName, false);
}

bool WifiSyncNotificationController::IsWifiSyncSupported() {
  HostStatusProvider::HostStatusWithDevice host_with_status =
      host_status_provider_->GetHostWithStatus();
  if (host_with_status.host_status() != mojom::HostStatus::kHostVerified) {
    return false;
  }

  std::optional<multidevice::RemoteDeviceRef> host_device =
      host_with_status.host_device();
  if (!host_device) {
    PA_LOG(ERROR) << "WifiSyncNotificationController::" << __func__
                  << ": Host device unexpectedly null.";
    return false;
  }

  if (host_device->GetSoftwareFeatureState(
          multidevice::SoftwareFeature::kWifiSyncHost) ==
      multidevice::SoftwareFeatureState::kNotSupported) {
    return false;
  }

  std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!local_device) {
    PA_LOG(ERROR) << "WifiSyncNotificationController::" << __func__
                  << ": Local device unexpectedly null.";
    return false;
  }

  if (local_device->GetSoftwareFeatureState(
          multidevice::SoftwareFeature::kWifiSyncClient) ==
      multidevice::SoftwareFeatureState::kNotSupported) {
    return false;
  }

  return true;
}

}  // namespace multidevice_setup

}  // namespace ash
