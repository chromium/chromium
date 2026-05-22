// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_KEY_DERIVATION_PARAMS_H_
#define COMPONENTS_SYNC_NIGORI_KEY_DERIVATION_PARAMS_H_

#include <string>

#include "components/sync/base/passphrase_enums.h"

namespace syncer {

class KeyDerivationParams {
 public:
  static KeyDerivationParams CreateForPbkdf2();
  static KeyDerivationParams CreateForScrypt(const std::string& salt);

  KeyDerivationMethod method() const { return method_; }
  const std::string& scrypt_salt() const;

  KeyDerivationParams(const KeyDerivationParams& other);
  KeyDerivationParams(KeyDerivationParams&& other);
  KeyDerivationParams& operator=(const KeyDerivationParams& other);

  bool operator==(const KeyDerivationParams& other) const = default;

 private:
  KeyDerivationParams(KeyDerivationMethod method,
                      const std::string& scrypt_salt);

  KeyDerivationMethod method_;

  std::string scrypt_salt_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_KEY_DERIVATION_PARAMS_H_
