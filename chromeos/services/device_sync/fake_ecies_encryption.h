// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_ECIES_ENCRYPTION_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_ECIES_ENCRYPTION_H_

#include <string>

namespace chromeos {

namespace device_sync {

// Returns the |public_key| with "PRIVATE_KEY:" prepended.
std::string GetPrivateKeyFromPublicKeyForTest(const std::string& public_key);

// Input should be of the form "PRIVATE_KEY:<public key>", in other words,
// consistent with the output of GetPrivateKeyFromPublicKeyForTest().
//
// Returns <public key>, in other words, |private_key| without the
// "PRIVATE_KEY:" prefix.
std::string GetPublicKeyFromPrivateKeyForTest(const std::string& private_key);

// Outputs "|unencrypted_string|:ENCRYPTED:|encrypting_public_key|".
std::string MakeFakeEncryptedString(const std::string& unencrypted_string,
                                    const std::string& encrypting_public_key);

// Input should have the form
//   "<unencrypted string>:ENCRYPTED:<encrypting public key>",
// in other words, consistent with the output of MakeFakeEncryptedString().
//
// Returns <unencrypted string>.
//
// Verifys that |decrypting_private_key| is "PRIVATE:<encrypting public key>",
// in other words, GetPrivateKeyFromPublicKey(<encrypting public key>).
std::string DecryptFakeEncryptedString(
    const std::string& encrypted_string,
    const std::string& decrypting_private_key);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_ECIES_ENCRYPTION_H_
