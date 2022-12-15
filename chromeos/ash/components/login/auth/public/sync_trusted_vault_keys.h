// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SYNC_TRUSTED_VAULT_KEYS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SYNC_TRUSTED_VAULT_KEYS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"

namespace ash {

// Struct which holds keys about a user's encryption keys during signin flow.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    SyncTrustedVaultKeys {
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
  static SyncTrustedVaultKeys FromJs(const base::Value::Dict& js_object);

  const std::string& gaia_id() const;

  const std::vector<std::vector<uint8_t>>& encryption_keys() const;
  int last_encryption_key_version() const;

  struct TrustedRecoveryMethod {
    TrustedRecoveryMethod();
    TrustedRecoveryMethod(const TrustedRecoveryMethod&);
    TrustedRecoveryMethod& operator=(const TrustedRecoveryMethod&);
    ~TrustedRecoveryMethod();

    std::vector<uint8_t> public_key;
    int type_hint = 0;
  };
  const std::vector<TrustedRecoveryMethod>& trusted_recovery_methods() const;

 private:
  std::string gaia_id_;
  std::vector<std::vector<uint8_t>> encryption_keys_;
  int last_encryption_key_version_ = 0;
  std::vector<TrustedRecoveryMethod> trusted_recovery_methods_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SYNC_TRUSTED_VAULT_KEYS_H_
