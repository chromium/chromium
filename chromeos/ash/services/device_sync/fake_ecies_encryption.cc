// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"

#include "base/check_op.h"
#include "base/containers/contains.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeEncryptionDelimiter[] = ":ENCRYPTED:";
const char kPrivateKeyPrefix[] = "PRIVATE_KEY:";

}  // namespace

std::string GetPrivateKeyFromPublicKeyForTest(const std::string& public_key) {
  return kPrivateKeyPrefix + public_key;
}

std::string GetPublicKeyFromPrivateKeyForTest(const std::string& private_key) {
  DCHECK(base::Contains(private_key, kPrivateKeyPrefix));

  return private_key.substr(strlen(kPrivateKeyPrefix), private_key.length());
}

std::string MakeFakeEncryptedString(const std::string& unencrypted_string,
                                    const std::string& encrypting_public_key) {
  return unencrypted_string + kFakeEncryptionDelimiter + encrypting_public_key;
}

std::string DecryptFakeEncryptedString(
    const std::string& encrypted_string,
    const std::string& decrypting_private_key) {
  std::string::size_type delimiter_pos =
      encrypted_string.find(kFakeEncryptionDelimiter);
  DCHECK_NE(std::string::npos, delimiter_pos);

  std::string encrypting_public_key =
      encrypted_string.substr(delimiter_pos + strlen(kFakeEncryptionDelimiter),
                              encrypted_string.length());
  DCHECK_EQ(GetPublicKeyFromPrivateKeyForTest(decrypting_private_key),
            encrypting_public_key);

  return encrypted_string.substr(0, delimiter_pos);
}

}  // namespace device_sync

}  // namespace ash
