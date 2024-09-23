// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_KEY_DERIVATION_PARAMS_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_KEY_DERIVATION_PARAMS_H_

#include <string>

#include "components/sync/base/passphrase_enums.h"

// TODO(crbug.com/41449994): Move this file to components/sync/nigori/. It lives
// in engine/nigori/ now because some engine code requires KeyDerivationParams
// to implement SyncEncryptionHandler::OnPassphraseRequired(). None of the
// implementations actually uses the parameter though, which means we can
// probably split the interface and depend on KeyDerivationParams only outside
// of the engine.

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

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_KEY_DERIVATION_PARAMS_H_
