// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/pref_names.h"

namespace ash {

namespace device_sync {

namespace prefs {
// (CryptAuth v1 and v2) The GCM registration id used for receiving push
// messages from CryptAuth.
const char kCryptAuthGCMRegistrationId[] = "cryptauth.gcm_registration_id";

// (CryptAuth v2) The dictionary of devices synced from CryptAuth, used to
// populate and persist the CryptAuthDeviceRegistry.
const char kCryptAuthDeviceRegistry[] = "cryptauth.device_registry";

// (CryptAuth v2) The dictionary of key bundles enrolled with CryptAuth, used to
// populate and persist the CryptAuthKeyRegistry.
const char kCryptAuthKeyRegistry[] = "cryptauth.key_registry";

// (CryptAuth v2) The hash of the last enrolled ClientAppMetadata. If this hash
// changes, a re-enrollment should occur.
const char kCryptAuthLastEnrolledClientAppMetadataHash[] =
    "cryptauth.enrollment.last_enrolled_client_app_metadata_hash";

// (CryptAuth v2) The encrypted and unencrypted local device
// CryptAuthBetterTogetherMetadata, along with the encrypting group public key,
// sent during the most recent successful SyncMetadata call. We don't want to
// re-encrypt the metadata if the metadata and group public key have not
// changed. Because a different session key is used for each new encryption, the
// encrypted blob would change, and CryptAuth would notify all user devices.
const char kCryptAuthLastSyncedEncryptedLocalDeviceMetadata[] =
    "cryptauth.device_sync.last_synced_encrypted_local_device_metadata";
const char kCryptAuthLastSyncedGroupPublicKey[] =
    "cryptauth.device_sync.last_synced_group_public_key";
const char kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata[] =
    "cryptauth.device_sync.last_synced_unencrypted_local_device_metadata";

// (CryptAuth v2) The Bluetooth address provided during the most recent
// DeviceSync attempt.
const char kCryptAuthBluetoothAddressProvidedDuringLastSync[] =
    "cryptauth.device_sync.last_bluetooth_address";

// (CryptAuth v2) The generation time of the most recently-generated certs.
extern const char kCryptAuthAttestationCertificatesLastGeneratedTimestamp[] =
    "cryptauth.device_sync.attestation_certificates_last_generated_timestamp";

// (CryptAuth v2) The most recent ClientDirective sent to the
// CryptAuthScheduler.
const char kCryptAuthSchedulerClientDirective[] =
    "cryptauth.scheduler.client_directive";

// (CryptAuth v2) The ClientMetadata of the last scheduled enrollment request.
const char kCryptAuthSchedulerNextEnrollmentRequestClientMetadata[] =
    "cryptauth.scheduler.next_enrollment_request_client_metadata";

// (CryptAuth v2) The ClientMetadata of the last scheduled DeviceSync request.
const char kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata[] =
    "cryptauth.scheduler.next_device_sync_request_client_metadata";

// (CryptAuth v2) The time of the last enrollment attempt.
const char kCryptAuthSchedulerLastEnrollmentAttemptTime[] =
    "cryptauth.scheduler.last_enrollment_attempt_time";

// (CryptAuth v2) The time of the last DeviceSync attempt.
const char kCryptAuthSchedulerLastDeviceSyncAttemptTime[] =
    "cryptauth.scheduler.last_device_sync_attempt_time";

// (CryptAuth v2) The time of the last successful enrollment.
const char kCryptAuthSchedulerLastSuccessfulEnrollmentTime[] =
    "cryptauth.scheduler.last_successful_enrollment_time";

// (CryptAuth v2) The time of the last successful DeviceSync.
const char kCryptAuthSchedulerLastSuccessfulDeviceSyncTime[] =
    "cryptauth.scheduler.last_device_sync_enrollment_time";

}  // namespace prefs

}  // namespace device_sync

}  // namespace ash
