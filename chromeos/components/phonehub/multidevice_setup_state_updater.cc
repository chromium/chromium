// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/multidevice_setup_state_updater.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "chromeos/components/phonehub/util/histogram_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace phonehub {
namespace {
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::HostStatus;
}  // namespace

// static
void MultideviceSetupStateUpdater::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kIsAwaitingVerifiedHost, false);
}

MultideviceSetupStateUpdater::MultideviceSetupStateUpdater(
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    NotificationAccessManager* notification_access_manager)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      notification_access_manager_(notification_access_manager) {
  multidevice_setup_client_->AddObserver(this);
  notification_access_manager_->AddObserver(this);

  const HostStatus host_status =
      multidevice_setup_client_->GetHostStatus().first;
  UpdateIsAwaitingVerifiedHost(host_status);
}

MultideviceSetupStateUpdater::~MultideviceSetupStateUpdater() {
  multidevice_setup_client_->RemoveObserver(this);
  notification_access_manager_->RemoveObserver(this);
}

void MultideviceSetupStateUpdater::OnNotificationAccessChanged() {
  if (notification_access_manager_->GetAccessStatus() ==
      NotificationAccessManager::AccessStatus::kAccessGranted) {
    return;
  }

  PA_LOG(INFO) << "Disabling PhoneHubNotifications feature.";

  // Disable kPhoneHubNotifications if notification access has been revoked by
  // the phone.
  multidevice_setup_client_->SetFeatureEnabledState(
      Feature::kPhoneHubNotifications, /*enabled=*/false,
      /*auth_token=*/base::nullopt, base::DoNothing());
}

void MultideviceSetupStateUpdater::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  const HostStatus host_status = host_device_with_status.first;

  bool is_awaiting_verified_host =
      pref_service_->GetBoolean(prefs::kIsAwaitingVerifiedHost);

  // Enable the PhoneHub feature if phone is verified and there was an
  // intent to enable the feature.
  if (is_awaiting_verified_host && host_status == HostStatus::kHostVerified) {
    multidevice_setup_client_->SetFeatureEnabledState(
        Feature::kPhoneHub, /*enabled=*/true, /*auth_token=*/base::nullopt,
        base::DoNothing());
    util::LogFeatureOptInEntryPoint(util::OptInEntryPoint::kSetupFlow);
  }

  UpdateIsAwaitingVerifiedHost(host_status);
}

void MultideviceSetupStateUpdater::UpdateIsAwaitingVerifiedHost(
    HostStatus host_status) {
  // Keep |prefs::kIsAwaitingVerifiedHost| at the same state.
  if (host_status == HostStatus::kHostSetButNotYetVerified)
    return;

  // Begin waiting for a verified host if the phone is
  // |kHostSetLocallyButWaitingForBackendConfirmation|.
  bool is_awaiting_verified_host =
      host_status ==
      HostStatus::kHostSetLocallyButWaitingForBackendConfirmation;

  // Must be persisted in the event that this class is destroyed
  // before the phone is verified.
  pref_service_->SetBoolean(prefs::kIsAwaitingVerifiedHost,
                            is_awaiting_verified_host);
}

}  // namespace phonehub
}  // namespace chromeos
