// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"

namespace ash {

namespace device_sync {

const char kCryptAuthUserKeyPairName[] = "PublicKey";
const char kCryptAuthLegacyAuthzenKeyName[] = "authzen";
const char kCryptAuthDeviceSyncBetterTogetherKeyName[] =
    "DeviceSync:BetterTogether";
const char kCryptAuthFixedUserKeyPairHandle[] = "device_key";
const char kCryptAuthSymmetricKeyDerivationSalt[] = "CryptAuth Enrollment";
const char kCryptAuthKeyProofSalt[] = "CryptAuth Key Proof";
const char kCryptAuthClientVersion[] = "1.0.0";

}  // namespace device_sync

}  // namespace ash
