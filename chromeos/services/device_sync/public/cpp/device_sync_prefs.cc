// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/device_sync_prefs.h"

#include "base/feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_manager_impl.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/services/device_sync/cryptauth_scheduler_impl.h"
#include "chromeos/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"
#include "chromeos/services/device_sync/synced_bluetooth_address_tracker_impl.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace device_sync {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  CryptAuthGCMManager::RegisterPrefs(registry);
  CryptAuthDeviceManager::RegisterPrefs(registry);
  if (base::FeatureList::IsEnabled(
          chromeos::features::kCryptAuthV2Enrollment)) {
    CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(registry);
    CryptAuthKeyRegistryImpl::RegisterPrefs(registry);
    CryptAuthSchedulerImpl::RegisterPrefs(registry);
  } else {
    CryptAuthEnrollmentManagerImpl::RegisterPrefs(registry);
  }

  if (features::ShouldUseV2DeviceSync()) {
    CryptAuthDeviceRegistryImpl::RegisterPrefs(registry);
    CryptAuthMetadataSyncerImpl::RegisterPrefs(registry);
    SyncedBluetoothAddressTrackerImpl::RegisterPrefs(registry);
  }
}

}  // namespace device_sync
}  // namespace chromeos
