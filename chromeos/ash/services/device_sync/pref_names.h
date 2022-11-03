// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PREF_NAMES_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PREF_NAMES_H_

namespace ash {

namespace device_sync {

namespace prefs {

// Prefs for CryptAuth v1:
extern const char kCryptAuthDeviceSyncLastSyncTimeSeconds[];
extern const char kCryptAuthDeviceSyncIsRecoveringFromFailure[];
extern const char kCryptAuthDeviceSyncReason[];
extern const char kCryptAuthDeviceSyncUnlockKeys[];
extern const char kCryptAuthEnrollmentIsRecoveringFromFailure[];
extern const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[];
extern const char kCryptAuthEnrollmentReason[];

// Prefs for CryptAuth v1 (and during migration to v2):
extern const char kCryptAuthEnrollmentUserPublicKey[];
extern const char kCryptAuthEnrollmentUserPrivateKey[];

// Prefs for CryptAuth v1 and v2:
extern const char kCryptAuthGCMRegistrationId[];

// Prefs for CryptAuth v2:
extern const char kCryptAuthDeviceRegistry[];
extern const char kCryptAuthKeyRegistry[];
extern const char kCryptAuthLastEnrolledClientAppMetadataHash[];
extern const char kCryptAuthLastSyncedEncryptedLocalDeviceMetadata[];
extern const char kCryptAuthLastSyncedGroupPublicKey[];
extern const char kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata[];
extern const char kCryptAuthBluetoothAddressProvidedDuringLastSync[];
extern const char kCryptAuthAttestationCertificatesLastGeneratedTimestamp[];
extern const char kCryptAuthSchedulerClientDirective[];
extern const char kCryptAuthSchedulerNextEnrollmentRequestClientMetadata[];
extern const char kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata[];
extern const char kCryptAuthSchedulerLastEnrollmentAttemptTime[];
extern const char kCryptAuthSchedulerLastDeviceSyncAttemptTime[];
extern const char kCryptAuthSchedulerLastSuccessfulEnrollmentTime[];
extern const char kCryptAuthSchedulerLastSuccessfulDeviceSyncTime[];

}  // namespace prefs

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PREF_NAMES_H_
