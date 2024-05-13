// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/key_derivation_params.h"

#include "base/check_op.h"

namespace syncer {

KeyDerivationParams::KeyDerivationParams(KeyDerivationMethod method,
                                         const std::string& scrypt_salt)
    : method_(method), scrypt_salt_(scrypt_salt) {}

KeyDerivationParams::KeyDerivationParams(const KeyDerivationParams& other) =
    default;
KeyDerivationParams::KeyDerivationParams(KeyDerivationParams&& other) = default;

KeyDerivationParams& KeyDerivationParams::operator=(
    const KeyDerivationParams& other) = default;

const std::string& KeyDerivationParams::scrypt_salt() const {
  DCHECK_EQ(method_, KeyDerivationMethod::SCRYPT_8192_8_11);
  return scrypt_salt_;
}

KeyDerivationParams KeyDerivationParams::CreateForPbkdf2() {
  return {KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003, /*scrypt_salt_=*/""};
}

KeyDerivationParams KeyDerivationParams::CreateForScrypt(
    const std::string& salt) {
  return {KeyDerivationMethod::SCRYPT_8192_8_11, salt};
}

}  // namespace syncer
