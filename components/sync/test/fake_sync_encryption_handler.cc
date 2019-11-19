// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_encryption_handler.h"

#include "components/sync/base/passphrase_enums.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/syncable/nigori_util.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler()
    : encrypted_types_(SensitiveTypes()),
      encrypt_everything_(false),
      passphrase_type_(PassphraseType::kImplicitPassphrase) {}
FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() {}

bool FakeSyncEncryptionHandler::Init() {
  // Set up a basic cryptographer.
  KeyParams keystore_params = {KeyDerivationParams::CreateForPbkdf2(),
                               "keystore_key"};
  cryptographer_.AddKey(keystore_params);
  return true;
}

bool FakeSyncEncryptionHandler::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  if (nigori.encrypt_everything())
    EnableEncryptEverything();
  if (nigori.keybag_is_frozen())
    passphrase_type_ = PassphraseType::kCustomPassphrase;

  // TODO(zea): consider adding fake support for migration.
  if (cryptographer_.CanDecrypt(nigori.encryption_keybag()))
    cryptographer_.InstallKeys(nigori.encryption_keybag());
  else if (nigori.has_encryption_keybag())
    cryptographer_.SetPendingKeys(nigori.encryption_keybag());

  if (cryptographer_.has_pending_keys()) {
    DVLOG(1) << "OnPassPhraseRequired Sent";
    sync_pb::EncryptedData pending_keys = cryptographer_.GetPendingKeys();
    for (auto& observer : observers_)
      observer.OnPassphraseRequired(REASON_DECRYPTION,
                                    KeyDerivationParams::CreateForPbkdf2(),
                                    pending_keys);
  } else if (!cryptographer_.CanEncrypt()) {
    DVLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
             << "ready";
    for (auto& observer : observers_) {
      observer.OnPassphraseRequired(REASON_ENCRYPTION,
                                    KeyDerivationParams::CreateForPbkdf2(),
                                    sync_pb::EncryptedData());
    }
  }

  return true;
}

void FakeSyncEncryptionHandler::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    const syncable::BaseTransaction* const trans) const {
  syncable::UpdateNigoriFromEncryptedTypes(encrypted_types_,
                                           encrypt_everything_, nigori);
}

bool FakeSyncEncryptionHandler::NeedKeystoreKey() const {
  return keystore_key_.empty();
}

bool FakeSyncEncryptionHandler::SetKeystoreKeys(
    const std::vector<std::string>& keys) {
  if (keys.empty())
    return false;
  std::string new_key = keys.back();
  if (new_key.empty())
    return false;
  keystore_key_ = new_key;

  DVLOG(1) << "Keystore bootstrap token updated.";
  for (auto& observer : observers_)
    observer.OnBootstrapTokenUpdated(keystore_key_, KEYSTORE_BOOTSTRAP_TOKEN);

  return true;
}

const Cryptographer* FakeSyncEncryptionHandler::GetCryptographer(
    const syncable::BaseTransaction* const trans) const {
  return &cryptographer_;
}

const DirectoryCryptographer*
FakeSyncEncryptionHandler::GetDirectoryCryptographer(
    const syncable::BaseTransaction* const trans) const {
  return &cryptographer_;
}

ModelTypeSet FakeSyncEncryptionHandler::GetEncryptedTypes(
    const syncable::BaseTransaction* const trans) const {
  return encrypted_types_;
}

void FakeSyncEncryptionHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncEncryptionHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncEncryptionHandler::SetEncryptionPassphrase(
    const std::string& passphrase) {
  passphrase_type_ = PassphraseType::kCustomPassphrase;
}

void FakeSyncEncryptionHandler::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::AddTrustedVaultDecryptionKeys(
    const std::vector<std::string>& encryption_keys) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::EnableEncryptEverything() {
  if (encrypt_everything_)
    return;
  encrypt_everything_ = true;
  encrypted_types_ = ModelTypeSet::All();
  for (auto& observer : observers_)
    observer.OnEncryptedTypesChanged(encrypted_types_, encrypt_everything_);
}

bool FakeSyncEncryptionHandler::IsEncryptEverythingEnabled() const {
  return encrypt_everything_;
}

PassphraseType FakeSyncEncryptionHandler::GetPassphraseType(
    const syncable::BaseTransaction* const trans) const {
  return passphrase_type_;
}

base::Time FakeSyncEncryptionHandler::GetKeystoreMigrationTime() const {
  return base::Time();
}

KeystoreKeysHandler* FakeSyncEncryptionHandler::GetKeystoreKeysHandler() {
  return this;
}

std::string FakeSyncEncryptionHandler::GetLastKeystoreKey() const {
  return std::string();
}

DirectoryCryptographer* FakeSyncEncryptionHandler::GetMutableCryptographer() {
  return &cryptographer_;
}

}  // namespace syncer
