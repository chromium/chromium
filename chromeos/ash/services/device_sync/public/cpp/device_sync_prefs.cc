// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/device_sync_prefs.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker_impl.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace device_sync {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  CryptAuthGCMManager::RegisterPrefs(registry);
  CryptAuthDeviceManager::RegisterPrefs(registry);
  CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(registry);
  CryptAuthKeyRegistryImpl::RegisterPrefs(registry);
  CryptAuthSchedulerImpl::RegisterPrefs(registry);

  if (features::ShouldUseV2DeviceSync()) {
    CryptAuthDeviceRegistryImpl::RegisterPrefs(registry);
    CryptAuthMetadataSyncerImpl::RegisterPrefs(registry);
    SyncedBluetoothAddressTrackerImpl::RegisterPrefs(registry);
  }

  if (features::IsCryptauthAttestationSyncingEnabled()) {
    AttestationCertificatesSyncerImpl::RegisterPrefs(registry);
  }
}

}  // namespace device_sync
}  // namespace ash
