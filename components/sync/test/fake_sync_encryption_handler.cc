// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_encryption_handler.h"

#include "base/base64.h"
#include "base/logging.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler() = default;

FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() = default;

bool FakeSyncEncryptionHandler::Init() {
  return true;
}

bool FakeSyncEncryptionHandler::NeedKeystoreKey() const {
  return keystore_key_.empty();
}

bool FakeSyncEncryptionHandler::SetKeystoreKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty())
    return false;
  std::vector<uint8_t> new_key = keys.back();
  if (new_key.empty())
    return false;
  keystore_key_ = new_key;

  DVLOG(1) << "Keystore bootstrap token updated.";
  for (auto& observer : observers_)
    observer.OnBootstrapTokenUpdated(base::Base64Encode(keystore_key_),
                                     KEYSTORE_BOOTSTRAP_TOKEN);

  return true;
}

void FakeSyncEncryptionHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncEncryptionHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncEncryptionHandler::SetEncryptionPassphrase(
    const std::string& passphrase) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& encryption_keys) {
  // Do nothing.
}

base::Time FakeSyncEncryptionHandler::GetKeystoreMigrationTime() const {
  return base::Time();
}

KeystoreKeysHandler* FakeSyncEncryptionHandler::GetKeystoreKeysHandler() {
  return this;
}

}  // namespace syncer
