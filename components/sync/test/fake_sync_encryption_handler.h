// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/test/fake_cryptographer.h"

namespace syncer {

// A fake sync encryption handler capable of keeping track of the encryption
// state without opening any transactions or interacting with the nigori node.
// Note that this only performs basic interactions with the cryptographer
// (setting pending keys, installing keys).
// Note: NOT thread safe. If threads attempt to check encryption state
// while another thread is modifying it, races can occur.
class FakeSyncEncryptionHandler : public KeystoreKeysHandler,
                                  public SyncEncryptionHandler {
 public:
  FakeSyncEncryptionHandler();
  ~FakeSyncEncryptionHandler() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyInitialStateToObservers() override;
  DataTypeSet GetEncryptedTypes() override;
  Cryptographer* GetCryptographer() override;
  PassphraseType GetPassphraseType() override;
  void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) override;
  void SetExplicitPassphraseDecryptionKey(std::unique_ptr<Nigori> key) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys) override;
  base::Time GetKeystoreMigrationTime() override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;
  const sync_pb::NigoriSpecifics_TrustedVaultDebugInfo&
  GetTrustedVaultDebugInfo() override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::vector<uint8_t>>& keys) override;

 private:
  base::ObserverList<SyncEncryptionHandler::Observer> observers_;
  std::vector<uint8_t> keystore_key_;
  FakeCryptographer fake_cryptographer_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_
