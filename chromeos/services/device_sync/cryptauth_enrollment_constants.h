// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_CONSTANTS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_CONSTANTS_H_

namespace chromeos {

namespace device_sync {

// The special strings used in SyncSingleKeyRequest::key_name. These strings are
// not arbitrary; CryptAuth must be able to identify these names.
extern const char kCryptAuthUserKeyPairName[];
extern const char kCryptAuthLegacyMasterKeyName[];
extern const char kCryptAuthDeviceSyncBetterTogetherKeyName[];

// CryptAuth demands that the kUserKeyPair key bundle have a lone key with this
// handle for backward compatibility reasons.
extern const char kCryptAuthFixedUserKeyPairHandle[];

// The salt used in HKDF to derive symmetric keys from Diffie-Hellman handshake.
// This value is part of the CryptAuth v2 Enrollment specifications.
extern const char kCryptAuthSymmetricKeyDerivationSalt[];

// The salt used in HKDF for symmetric key proofs. Also, for asymmetric key
// proofs, the salt is prepended to the payload before being signed by the
// private key. This value is part of the CryptAuth v2 Enrollment
// specifications.
extern const char kCryptAuthKeyProofSalt[];

// The client version sent to CryptAuth in the SyncKeysRequest.
extern const char kCryptAuthClientVersion[];

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_CONSTANTS_H_
