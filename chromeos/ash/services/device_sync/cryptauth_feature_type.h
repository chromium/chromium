// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_

#include <optional>
#include <ostream>
#include <string>

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/multidevice/software_feature.h"

namespace ash {

namespace device_sync {

// The BetterTogether feature types used by the CryptAuth v2 backend. Each
// feature has a separate type for the "supported" state and "enabled" state of
// the feature. Currently, the "supported" types are only used for the CryptAuth
// v2 DeviceSync BatchGetFeatureStatuses RPC. Each enum value is uniquely mapped
// to a string used in the protos and understood by CryptAuth.
//
// Example: The following FeatureStatus messages received in a
// BatchGetFeatureStatuses response would indicate that BetterTogether host is
// supported but not enabled:
//   [
//     message FeatureStatus {
//       string feature_type = "BETTER_TOGETHER_HOST_SUPPORTED";
//       bool enabled = true;
//     },
//     message FeatureStatus {
//       string feature_type = "BETTER_TOGETHER_HOST";
//       bool enabled = false;
//     }
//   ]
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class CryptAuthFeatureType {
  // Support for multi-device features in general.
  kBetterTogetherHostSupported = 0,
  kBetterTogetherHostEnabled = 1,
  kBetterTogetherClientSupported = 2,
  kBetterTogetherClientEnabled = 3,

  // Smart Lock, which gives the user the ability to unlock and/or sign into a
  // Chromebook using an Android phone.
  kEasyUnlockHostSupported = 4,
  kEasyUnlockHostEnabled = 5,
  kEasyUnlockClientSupported = 6,
  kEasyUnlockClientEnabled = 7,

  // Instant Tethering, which gives the user the ability to use an Android
  // phone's Internet connection on a Chromebook.
  kMagicTetherHostSupported = 8,
  kMagicTetherHostEnabled = 9,
  kMagicTetherClientSupported = 10,
  kMagicTetherClientEnabled = 11,

  // Messages for Web, which gives the user the ability to sync messages (e.g.,
  // SMS) between an Android phone and a Chromebook.
  kSmsConnectHostSupported = 12,
  kSmsConnectHostEnabled = 13,
  kSmsConnectClientSupported = 14,
  kSmsConnectClientEnabled = 15,

  // Phone Hub, which allows users to view phone metadata and send commands to
  // their phone directly from the Chrome OS UI.
  kPhoneHubHostSupported = 16,
  kPhoneHubHostEnabled = 17,
  kPhoneHubClientSupported = 18,
  kPhoneHubClientEnabled = 19,

  // Wifi Sync with Android, which allows users to sync wifi network
  // configurations between Chrome OS devices and a connected  Android phone
  kWifiSyncHostSupported = 20,
  kWifiSyncHostEnabled = 21,
  kWifiSyncClientSupported = 22,
  kWifiSyncClientEnabled = 23,

  // Eche
  kEcheHostSupported = 24,
  kEcheHostEnabled = 25,
  kEcheClientSupported = 26,
  kEcheClientEnabled = 27,

  // Camera Roll, which allows users to view and download recent photos and
  // videos from the Phone Hub tray.
  kPhoneHubCameraRollHostSupported = 28,
  kPhoneHubCameraRollHostEnabled = 29,
  kPhoneHubCameraRollClientSupported = 30,
  kPhoneHubCameraRollClientEnabled = 31,

  // Used for UMA logs.
  kMaxValue = kPhoneHubCameraRollClientEnabled
};

const base::flat_set<CryptAuthFeatureType>& GetAllCryptAuthFeatureTypes();
const base::flat_set<CryptAuthFeatureType>& GetSupportedCryptAuthFeatureTypes();
const base::flat_set<CryptAuthFeatureType>& GetEnabledCryptAuthFeatureTypes();
const base::flat_set<std::string>& GetAllCryptAuthFeatureTypeStrings();

// Provides a unique mapping between each CryptAuthFeatureType enum value and
// the corresponding string used in the protos and understood by CryptAuth.
// CryptAuthFeatureTypeFromString returns null if |feature_type_string| does not
// map to a known CryptAuthFeatureType.
const char* CryptAuthFeatureTypeToString(CryptAuthFeatureType feature_type);
std::optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string);

// Provides a unique mapping between a CryptAuthFeatureType and its
// corresponding encoded hash value that CryptAuth sends in GCM messages.
// CryptAuth sends a base64url-encoded, SHA-256 8-byte hash of the
// CryptAuthFeatureType string. CryptAuth chooses this hashing scheme to
// accommodate the limited bandwidth of GCM messages.
// CryptAuthFeatureTypeFromGcmHash returns null if |feature_type_hash| cannot be
// mapped to a CryptAuthFeatureType.
std::string CryptAuthFeatureTypeToGcmHash(CryptAuthFeatureType feature_type);
std::optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromGcmHash(
    const std::string& feature_type_hash);

// Provides a mapping between CryptAuthFeatureTypes and SoftwareFeatures.
//
// The "enabled" and "supported" feature types are mapped to the same
// SoftwareFeature. For example,
// CryptAuthFeatureType::kBetterTogetherHostEnabled and
// CryptAuthFeatureType::kBetterTogetherHostSupported are both mapped to
// SoftwareFeature::kBetterTogetherHost.
//
// A SoftwareFeature is mapped to the "enabled" CryptAuthFeatureType. For
// example, SoftwareFeature::kBetterTogetherHost maps to
// CryptAuthFeatureType::kBetterTogetherHostEnabled.
multidevice::SoftwareFeature CryptAuthFeatureTypeToSoftwareFeature(
    CryptAuthFeatureType feature_type);
CryptAuthFeatureType CryptAuthFeatureTypeFromSoftwareFeature(
    multidevice::SoftwareFeature software_feature);

std::ostream& operator<<(std::ostream& stream,
                         CryptAuthFeatureType feature_type);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
