// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_
#define CHROMEOS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Struct which holds keys about a user's encryption keys during signin flow.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) SyncTrustedVaultKeys {
 public:
  SyncTrustedVaultKeys();
  SyncTrustedVaultKeys(const SyncTrustedVaultKeys&);
  SyncTrustedVaultKeys(SyncTrustedVaultKeys&&);
  SyncTrustedVaultKeys& operator=(const SyncTrustedVaultKeys&);
  SyncTrustedVaultKeys& operator=(SyncTrustedVaultKeys&&);
  ~SyncTrustedVaultKeys();

  // Initialize an instance of this class with data received from javascript.
  // The input data must be of type SyncTrustedVaultKeys as defined in
  // authenticator.js.
  static SyncTrustedVaultKeys FromJs(const base::DictionaryValue& js_object);

  const std::vector<std::vector<uint8_t>>& encryption_keys() const;
  int last_encryption_key_version() const;
  const std::vector<std::vector<uint8_t>>& trusted_public_keys() const;

 private:
  std::vector<std::vector<uint8_t>> encryption_keys_;
  int last_encryption_key_version_ = 0;
  std::vector<std::vector<uint8_t>> trusted_public_keys_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_
