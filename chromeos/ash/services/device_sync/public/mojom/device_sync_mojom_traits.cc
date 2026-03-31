// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

ash::device_sync::mojom::ConnectivityStatus EnumTraits<
    ash::device_sync::mojom::ConnectivityStatus,
    cryptauthv2::ConnectivityStatus>::ToMojom(cryptauthv2::ConnectivityStatus
                                                  input) {
  switch (input) {
    case cryptauthv2::ConnectivityStatus::ONLINE:
      return ash::device_sync::mojom::ConnectivityStatus::kOnline;
    case cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY:
      return ash::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity;
    case cryptauthv2::ConnectivityStatus::OFFLINE:
      return ash::device_sync::mojom::ConnectivityStatus::kOffline;
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

cryptauthv2::ConnectivityStatus
EnumTraits<ash::device_sync::mojom::ConnectivityStatus,
           cryptauthv2::ConnectivityStatus>::
    FromMojom(ash::device_sync::mojom::ConnectivityStatus input) {
  switch (input) {
    case ash::device_sync::mojom::ConnectivityStatus::kOnline:
      return cryptauthv2::ConnectivityStatus::ONLINE;
    case ash::device_sync::mojom::ConnectivityStatus::kOffline:
      return cryptauthv2::ConnectivityStatus::OFFLINE;
    case ash::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity:
      return cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY;
  }

  NOTREACHED();
}

ash::device_sync::mojom::GroupPrivateKeyStatus
EnumTraits<ash::device_sync::mojom::GroupPrivateKeyStatus,
           ash::device_sync::GroupPrivateKeyStatus>::
    ToMojom(ash::device_sync::GroupPrivateKeyStatus input) {
  switch (input) {
    case ash::device_sync::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kStatusUnavailableBecauseDeviceSyncIsNotInitialized;
    case ash::device_sync::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kStatusUnavailableBecauseNoDeviceSyncerSet;
    case ash::device_sync::GroupPrivateKeyStatus::kWaitingForGroupPrivateKey:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kWaitingForGroupPrivateKey;
    case ash::device_sync::GroupPrivateKeyStatus::
        kNoEncryptedGroupPrivateKeyReceived:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kNoEncryptedGroupPrivateKeyReceived;
    case ash::device_sync::GroupPrivateKeyStatus::
        kEncryptedGroupPrivateKeyEmpty:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kEncryptedGroupPrivateKeyEmpty;
    case ash::device_sync::GroupPrivateKeyStatus::
        kLocalDeviceSyncBetterTogetherKeyMissing:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kLocalDeviceSyncBetterTogetherKeyMissing;
    case ash::device_sync::GroupPrivateKeyStatus::
        kGroupPrivateKeyDecryptionFailed:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kGroupPrivateKeyDecryptionFailed;
    case ash::device_sync::GroupPrivateKeyStatus::
        kGroupPrivateKeySuccessfullyDecrypted:
      return ash::device_sync::mojom::GroupPrivateKeyStatus::
          kGroupPrivateKeySuccessfullyDecrypted;
  }

  NOTREACHED();
}

ash::device_sync::GroupPrivateKeyStatus
EnumTraits<ash::device_sync::mojom::GroupPrivateKeyStatus,
           ash::device_sync::GroupPrivateKeyStatus>::
    FromMojom(ash::device_sync::mojom::GroupPrivateKeyStatus input) {
  switch (input) {
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      return ash::device_sync::GroupPrivateKeyStatus::
          kStatusUnavailableBecauseDeviceSyncIsNotInitialized;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      return ash::device_sync::GroupPrivateKeyStatus::
          kStatusUnavailableBecauseNoDeviceSyncerSet;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kWaitingForGroupPrivateKey:
      return ash::device_sync::GroupPrivateKeyStatus::
          kWaitingForGroupPrivateKey;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kNoEncryptedGroupPrivateKeyReceived:
      return ash::device_sync::GroupPrivateKeyStatus::
          kNoEncryptedGroupPrivateKeyReceived;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kEncryptedGroupPrivateKeyEmpty:
      return ash::device_sync::GroupPrivateKeyStatus::
          kEncryptedGroupPrivateKeyEmpty;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kLocalDeviceSyncBetterTogetherKeyMissing:
      return ash::device_sync::GroupPrivateKeyStatus::
          kLocalDeviceSyncBetterTogetherKeyMissing;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kGroupPrivateKeyDecryptionFailed:
      return ash::device_sync::GroupPrivateKeyStatus::
          kGroupPrivateKeyDecryptionFailed;
    case ash::device_sync::mojom::GroupPrivateKeyStatus::
        kGroupPrivateKeySuccessfullyDecrypted:
      return ash::device_sync::GroupPrivateKeyStatus::
          kGroupPrivateKeySuccessfullyDecrypted;
  }

  NOTREACHED();
}

ash::device_sync::mojom::BetterTogetherMetadataStatus
EnumTraits<ash::device_sync::mojom::BetterTogetherMetadataStatus,
           ash::device_sync::BetterTogetherMetadataStatus>::
    ToMojom(ash::device_sync::BetterTogetherMetadataStatus input) {
  switch (input) {
    case ash::device_sync::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kStatusUnavailableBecauseDeviceSyncIsNotInitialized;
    case ash::device_sync::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kStatusUnavailableBecauseNoDeviceSyncerSet;
    case ash::device_sync::BetterTogetherMetadataStatus::
        kWaitingToProcessDeviceMetadata:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kWaitingToProcessDeviceMetadata;
    case ash::device_sync::BetterTogetherMetadataStatus::
        kGroupPrivateKeyMissing:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kGroupPrivateKeyMissing;
    case ash::device_sync::BetterTogetherMetadataStatus::
        kEncryptedMetadataEmpty:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kEncryptedMetadataEmpty;
    case ash::device_sync::BetterTogetherMetadataStatus::kMetadataDecrypted:
      return ash::device_sync::mojom::BetterTogetherMetadataStatus::
          kMetadataDecrypted;
  }

  NOTREACHED();
}

ash::device_sync::BetterTogetherMetadataStatus
EnumTraits<ash::device_sync::mojom::BetterTogetherMetadataStatus,
           ash::device_sync::BetterTogetherMetadataStatus>::
    FromMojom(ash::device_sync::mojom::BetterTogetherMetadataStatus input) {
  switch (input) {
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      return ash::device_sync::BetterTogetherMetadataStatus::
          kStatusUnavailableBecauseDeviceSyncIsNotInitialized;
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      return ash::device_sync::BetterTogetherMetadataStatus::
          kStatusUnavailableBecauseNoDeviceSyncerSet;
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kWaitingToProcessDeviceMetadata:
      return ash::device_sync::BetterTogetherMetadataStatus::
          kWaitingToProcessDeviceMetadata;
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kGroupPrivateKeyMissing:
      return ash::device_sync::BetterTogetherMetadataStatus::
          kGroupPrivateKeyMissing;
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kEncryptedMetadataEmpty:
      return ash::device_sync::BetterTogetherMetadataStatus::
          kEncryptedMetadataEmpty;
    case ash::device_sync::mojom::BetterTogetherMetadataStatus::
        kMetadataDecrypted:
      return ash::device_sync::BetterTogetherMetadataStatus::kMetadataDecrypted;
  }

  NOTREACHED();
}

ash::device_sync::mojom::FeatureStatusChange
EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
           ash::device_sync::FeatureStatusChange>::
    ToMojom(ash::device_sync::FeatureStatusChange input) {
  switch (input) {
    case ash::device_sync::FeatureStatusChange::kEnableExclusively:
      return ash::device_sync::mojom::FeatureStatusChange::kEnableExclusively;
    case ash::device_sync::FeatureStatusChange::kEnableNonExclusively:
      return ash::device_sync::mojom::FeatureStatusChange::
          kEnableNonExclusively;
    case ash::device_sync::FeatureStatusChange::kDisable:
      return ash::device_sync::mojom::FeatureStatusChange::kDisable;
  }
}

ash::device_sync::FeatureStatusChange
EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
           ash::device_sync::FeatureStatusChange>::
    FromMojom(ash::device_sync::mojom::FeatureStatusChange input) {
  switch (input) {
    case ash::device_sync::mojom::FeatureStatusChange::kEnableExclusively:
      return ash::device_sync::FeatureStatusChange::kEnableExclusively;
    case ash::device_sync::mojom::FeatureStatusChange::kEnableNonExclusively:
      return ash::device_sync::FeatureStatusChange::kEnableNonExclusively;
    case ash::device_sync::mojom::FeatureStatusChange::kDisable:
      return ash::device_sync::FeatureStatusChange::kDisable;
  }

  NOTREACHED();
}

ash::device_sync::mojom::CryptAuthService EnumTraits<
    ash::device_sync::mojom::CryptAuthService,
    cryptauthv2::TargetService>::ToMojom(cryptauthv2::TargetService input) {
  switch (input) {
    case cryptauthv2::TargetService::ENROLLMENT:
      return ash::device_sync::mojom::CryptAuthService::kEnrollment;
    case cryptauthv2::TargetService::DEVICE_SYNC:
      return ash::device_sync::mojom::CryptAuthService::kDeviceSync;
    case cryptauthv2::TargetService::TARGET_SERVICE_UNSPECIFIED:
    case cryptauthv2::TargetService::TargetService_INT_MIN_SENTINEL_DO_NOT_USE_:
    case cryptauthv2::TargetService::TargetService_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

cryptauthv2::TargetService EnumTraits<ash::device_sync::mojom::CryptAuthService,
                                      cryptauthv2::TargetService>::
    FromMojom(ash::device_sync::mojom::CryptAuthService input) {
  switch (input) {
    case ash::device_sync::mojom::CryptAuthService::kEnrollment:
      return cryptauthv2::TargetService::ENROLLMENT;
    case ash::device_sync::mojom::CryptAuthService::kDeviceSync:
      return cryptauthv2::TargetService::DEVICE_SYNC;
  }

  NOTREACHED();
}

}  // namespace mojo
