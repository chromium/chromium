// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/pref_names.h"

namespace chromeos {

namespace device_sync {

namespace prefs {

// (CryptAuth v1) Whether the system is scheduling device_syncs more
// aggressively to recover from the previous device_sync failure.
const char kCryptAuthDeviceSyncIsRecoveringFromFailure[] =
    "cryptauth.device_sync.is_recovering_from_failure";

// (CryptAuth v1) The timestamp of the last successful CryptAuth device_sync in
// seconds.
const char kCryptAuthDeviceSyncLastSyncTimeSeconds[] =
    "cryptauth.device_sync.last_device_sync_time_seconds";

// (CryptAuth v1) The reason that the next device_sync is performed. This should
// be one of the enum values of cryptauth::InvocationReason in
// chromeos/services/device_sync/proto/cryptauth_api.proto.
const char kCryptAuthDeviceSyncReason[] = "cryptauth.device_sync.reason";

// (CryptAuth v1) A list of unlock keys (stored as dictionaries) synced from
// CryptAuth. Unlock Keys are phones belonging to the user that can unlock other
// devices, such as desktop PCs.
const char kCryptAuthDeviceSyncUnlockKeys[] =
    "cryptauth.device_sync.unlock_keys";

// (CryptAuth v1) Whether the system is scheduling enrollments more aggressively
// to recover from the previous enrollment failure.
const char kCryptAuthEnrollmentIsRecoveringFromFailure[] =
    "cryptauth.enrollment.is_recovering_from_failure";

// (CryptAuth v1) The timestamp of the last successful CryptAuth enrollment in
// seconds.
const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[] =
    "cryptauth.enrollment.last_enrollment_time_seconds";

// (CryptAuth v1) The reason that the next enrollment is performed. This should
// be one of the enum values of cryptauth::InvocationReason in
// chromeos/services/device_sync/proto/cryptauth_api.proto.
const char kCryptAuthEnrollmentReason[] = "cryptauth.enrollment.reason";

// (CryptAuth v1 and during migration to v2) The public key of the user and
// device enrolled with CryptAuth.
const char kCryptAuthEnrollmentUserPublicKey[] =
    "cryptauth.enrollment.user_public_key";

// (CryptAuth v1 and during migration to v2) The private key of the user and
// device enrolled with CryptAuth.
const char kCryptAuthEnrollmentUserPrivateKey[] =
    "cryptauth.enrollment.user_private_key";

// (CryptAuth v1 and v2) The GCM registration id used for receiving push
// messages from CryptAuth.
const char kCryptAuthGCMRegistrationId[] = "cryptauth.gcm_registration_id";

// (CryptAuth v2) The dictionary of devices synced from CryptAuth, used to
// populate and persist the CryptAuthDeviceRegistry.
const char kCryptAuthDeviceRegistry[] = "cryptauth.device_registry";

// (CryptAuth v2) The dictionary of key bundles enrolled with CryptAuth, used to
// populate and persist the CryptAuthKeyRegistry.
const char kCryptAuthKeyRegistry[] = "cryptauth.key_registry";

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

}  // namespace chromeos
