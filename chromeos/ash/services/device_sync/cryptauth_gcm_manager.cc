// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"

#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace device_sync {

CryptAuthGCMManager::Observer::~Observer() {}

void CryptAuthGCMManager::Observer::OnGCMRegistrationResult(bool success) {}

void CryptAuthGCMManager::Observer::OnReenrollMessage(
    const absl::optional<std::string>& session_id,
    const absl::optional<CryptAuthFeatureType>& feature_type) {}

void CryptAuthGCMManager::Observer::OnResyncMessage(
    const absl::optional<std::string>& session_id,
    const absl::optional<CryptAuthFeatureType>& feature_type) {}

// static.
void CryptAuthGCMManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthGCMRegistrationId,
                               std::string());
}

}  // namespace device_sync

}  // namespace ash
