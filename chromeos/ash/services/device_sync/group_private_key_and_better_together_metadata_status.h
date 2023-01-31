// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_GROUP_PRIVATE_KEY_AND_BETTER_TOGETHER_METADATA_STATUS_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_GROUP_PRIVATE_KEY_AND_BETTER_TOGETHER_METADATA_STATUS_H_

#include <ostream>

namespace ash::device_sync {

// The group private key and better together metadata status in the
// CryptAuthDeviceSyncer. These enums are declared in their own module so they
// can be consumed by mojom::DeviceSync and avoid a dependency cycle.
enum class GroupPrivateKeyStatus {
  // When Device Sync is not initialized, it cannot access the group private key
  // status and will return this value.
  kStatusUnavailableBecauseDeviceSyncIsNotInitialized,

  // When the CryptAuthV2 device manager hasn't initialized a device syncer, it
  // cannot access the group private key status and will return this value.
  kStatusUnavailableBecauseNoDeviceSyncerSet,

  // The CryptAuth SyncMetadata response that includes the encrypted group
  // private key hasn't been received yet.
  kWaitingForGroupPrivateKey,

  // The SyncMetadata response was been received, but doesn't include any
  // encrypted group private key. This is expected when no other user device
  // uploaded the key or if we already own the key.
  kNoEncryptedGroupPrivateKeyReceived,

  // The SyncMetadata response was received, but the included encrypted group
  // private key is empty.
  kEncryptedGroupPrivateKeyEmpty,

  // This device's CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether key is
  // missing, so the encrypted group private key cannot be decrypted.
  kLocalDeviceSyncBetterTogetherKeyMissing,

  // An error occurred when decrypting the group private key.
  kGroupPrivateKeyDecryptionFailed,

  // The group private key was successfully decrypted. This is the expected
  // final state of this flow.
  kGroupPrivateKeySuccessfullyDecrypted,
};

enum class BetterTogetherMetadataStatus {
  // When Device Sync is not initialized, it cannot access the better together
  // metadata status and will return this value.
  kStatusUnavailableBecauseDeviceSyncIsNotInitialized,

  // When the CryptAuthV2 device manager hasn't initialized a device syncer, it
  // cannot access the better together metadata status and will return this
  // value.
  kStatusUnavailableBecauseNoDeviceSyncerSet,

  // The attempt to process the encrypted device metadata hasn't started yet.
  // If the device sync attempt finishes and this is still the metadata
  // status, clients can inspect GroupPrivateKeyStatus to understand why.
  kWaitingToProcessDeviceMetadata,

  // The group private key required to decrypt the metadata is missing.
  // Clients can inspect GroupPrivateKeyStatus to understand why the group
  // private key is missing.
  kGroupPrivateKeyMissing,

  // CryptAuth didn't send any encrypted metadata.
  kEncryptedMetadataEmpty,

  // Device metadata was decrypted. This is the expected final state of this
  // flow.
  kMetadataDecrypted,
};

std::ostream& operator<<(std::ostream& stream,
                         const GroupPrivateKeyStatus& state);
std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherMetadataStatus& state);

}  // namespace ash::device_sync

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_GROUP_PRIVATE_KEY_AND_BETTER_TOGETHER_METADATA_STATUS_H_
