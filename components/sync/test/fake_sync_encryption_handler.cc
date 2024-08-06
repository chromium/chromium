// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_encryption_handler.h"

#include "base/base64.h"
#include "base/logging.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler() = default;

FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() = default;

void FakeSyncEncryptionHandler::NotifyInitialStateToObservers() {}

DataTypeSet FakeSyncEncryptionHandler::GetEncryptedTypes() {
  return AlwaysEncryptedUserTypes();
}

Cryptographer* FakeSyncEncryptionHandler::GetCryptographer() {
  // GetCryptographer() must never return null.
  return &fake_cryptographer_;
}

PassphraseType FakeSyncEncryptionHandler::GetPassphraseType() {
  return PassphraseType::kKeystorePassphrase;
}

bool FakeSyncEncryptionHandler::NeedKeystoreKey() const {
  return keystore_key_.empty();
}

bool FakeSyncEncryptionHandler::SetKeystoreKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty()) {
    return false;
  }
  std::vector<uint8_t> new_key = keys.back();
  if (new_key.empty()) {
    return false;
  }
  keystore_key_ = new_key;

  return true;
}

void FakeSyncEncryptionHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncEncryptionHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncEncryptionHandler::SetEncryptionPassphrase(
    const std::string& passphrase,
    const KeyDerivationParams& key_derivation_params) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::SetExplicitPassphraseDecryptionKey(
    std::unique_ptr<Nigori> key) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& encryption_keys) {
  // Do nothing.
}

base::Time FakeSyncEncryptionHandler::GetKeystoreMigrationTime() {
  return base::Time();
}

KeystoreKeysHandler* FakeSyncEncryptionHandler::GetKeystoreKeysHandler() {
  return this;
}

const sync_pb::NigoriSpecifics::TrustedVaultDebugInfo&
FakeSyncEncryptionHandler::GetTrustedVaultDebugInfo() {
  return sync_pb::NigoriSpecifics::TrustedVaultDebugInfo::default_instance();
}

}  // namespace syncer
