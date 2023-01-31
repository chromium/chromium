// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"

namespace {

constexpr char private_key_status_prefix[] =
    "[DeviceSyncer group private key status: ";
constexpr char better_together_metadata_status_prefix[] =
    "[DeviceSyncer better together metadata status: ";

}  // namespace

namespace ash::device_sync {

std::ostream& operator<<(std::ostream& stream,
                         const GroupPrivateKeyStatus& status) {
  switch (status) {
    case GroupPrivateKeyStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      stream << private_key_status_prefix
             << "Status unavailable because device sync is not initialized]";
      break;
    case GroupPrivateKeyStatus::kStatusUnavailableBecauseNoDeviceSyncerSet:
      stream << private_key_status_prefix
             << "Status unavailable because no device syncer is set]";
      break;
    case GroupPrivateKeyStatus::kWaitingForGroupPrivateKey:
      stream << private_key_status_prefix
             << "Waiting to receive group private key]";
      break;
    case GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived:
      stream << private_key_status_prefix
             << "No encrypted group private key received]";
      break;
    case GroupPrivateKeyStatus::kEncryptedGroupPrivateKeyEmpty:
      stream << private_key_status_prefix
             << "Encrypted group private key empty]";
      break;
    case GroupPrivateKeyStatus::kLocalDeviceSyncBetterTogetherKeyMissing:
      stream << private_key_status_prefix
             << "Local device sync better together key missing]";
      break;
    case GroupPrivateKeyStatus::kGroupPrivateKeyDecryptionFailed:
      stream << private_key_status_prefix
             << "Group private key decryption failed]";
      break;
    case GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted:
      stream << private_key_status_prefix
             << "Group private key successfully decrypted]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherMetadataStatus& status) {
  switch (status) {
    case BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      stream << better_together_metadata_status_prefix
             << "Status unavailable because device sync is not initialized]";
      break;
    case BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      stream << better_together_metadata_status_prefix
             << "Status unavailable because no device syncer is set]";
      break;
    case BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata:
      stream << better_together_metadata_status_prefix
             << "Waiting to process device metadata]";
      break;
    case BetterTogetherMetadataStatus::kGroupPrivateKeyMissing:
      stream << better_together_metadata_status_prefix
             << "Group private key is missing]";
      break;
    case BetterTogetherMetadataStatus::kEncryptedMetadataEmpty:
      stream << better_together_metadata_status_prefix
             << "Encrypted metadata is empty]";
      break;
    case BetterTogetherMetadataStatus::kMetadataDecrypted:
      stream << better_together_metadata_status_prefix << "Metadata decrypted]";
      break;
  }

  return stream;
}

}  // namespace ash::device_sync