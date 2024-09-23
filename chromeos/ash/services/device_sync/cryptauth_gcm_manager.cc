// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace device_sync {

CryptAuthGCMManager::Observer::~Observer() {}

void CryptAuthGCMManager::Observer::OnGCMRegistrationResult(bool success) {}

void CryptAuthGCMManager::Observer::OnReenrollMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {}

void CryptAuthGCMManager::Observer::OnResyncMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {}

// static.
void CryptAuthGCMManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthGCMRegistrationId,
                               std::string());
}

// static.
bool CryptAuthGCMManager::IsRegistrationIdDeprecated(
    const std::string& registration_id) {
  // V4 GCM Tokens always contain a colon, while deprecated V3 tokens will not.
  bool deprecated = registration_id.find(":") == std::string::npos;
  if (deprecated) {
    PA_LOG(WARNING)
        << "CryptAuthGCMManager: GCM Registration ID is deprecated (V3).";
  } else {
    PA_LOG(VERBOSE)
        << "CryptAuthGCMManager: GCM Registration ID is current (V4).";
  }

  return deprecated;
}

}  // namespace device_sync

}  // namespace ash
