// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_engine.h"

namespace syncer {

class CryptoSyncPrefs;

// This class functions as mostly independent component of SyncService that
// handles things related to encryption, including holding lots of state and
// encryption communications with the sync thread.
class SyncServiceCrypto : public SyncEncryptionHandler::Observer {
 public:
  SyncServiceCrypto(
      const base::RepeatingClosure& notify_observers,
      const base::RepeatingCallback<void(ConfigureReason)>& reconfigure,
      CryptoSyncPrefs* sync_prefs);
  ~SyncServiceCrypto() override;

  void Reset();

  // See the SyncService header.
  base::Time GetExplicitPassphraseTime() const;
  bool IsUsingSecondaryPassphrase() const;
  void EnableEncryptEverything();
  bool IsEncryptEverythingEnabled() const;
  void SetEncryptionPassphrase(const std::string& passphrase);
  bool SetDecryptionPassphrase(const std::string& passphrase);

  // Returns the actual passphrase type being used for encryption.
  PassphraseType GetPassphraseType() const;

  // Returns true if encrypting all the sync data is allowed. If this method
  // returns false, EnableEncryptEverything() should not be called.
  bool IsEncryptEverythingAllowed() const;

  // Sets whether encrypting all the sync data is allowed or not.
  void SetEncryptEverythingAllowed(bool allowed);

  // Returns the current set of encrypted data types.
  ModelTypeSet GetEncryptedDataTypes() const;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;
  void OnLocalSetPassphraseEncryption(
      const SyncEncryptionHandler::NigoriState& nigori_state) override;

  // Calls data type manager to start catch up configure.
  void BeginConfigureCatchUpBeforeClear();

  // Used to provide the engine when it is initialized.
  void SetSyncEngine(SyncEngine* engine) { state_.engine = engine; }

  // Creates a proxy observer object that will post calls to this thread.
  std::unique_ptr<SyncEncryptionHandler::Observer> GetEncryptionObserverProxy();

  // Takes the previously saved nigori state; null if there isn't any.
  std::unique_ptr<SyncEncryptionHandler::NigoriState> TakeSavedNigoriState();

  PassphraseRequiredReason passphrase_required_reason() const {
    return state_.passphrase_required_reason;
  }
  bool encryption_pending() const { return state_.encryption_pending; }

 private:
  // Calls SyncServiceBase::NotifyObservers(). Never null.
  const base::RepeatingClosure notify_observers_;

  const base::RepeatingCallback<void(ConfigureReason)> reconfigure_;

  // A pointer to the crypto-relevant sync prefs. Never null and guaranteed to
  // outlive us.
  CryptoSyncPrefs* const sync_prefs_;

  // All the mutable state is wrapped in a struct so that it can be easily
  // reset to its default values.
  struct State {
    State();
    ~State();

    State& operator=(State&& other) = default;

    // Not-null when the engine is initialized.
    SyncEngine* engine = nullptr;

    // Was the last SYNC_PASSPHRASE_REQUIRED notification sent because it
    // was required for encryption, decryption with a cached passphrase, or
    // because a new passphrase is required?
    PassphraseRequiredReason passphrase_required_reason =
        REASON_PASSPHRASE_NOT_REQUIRED;

    // The current set of encrypted types. Always a superset of
    // Cryptographer::SensitiveTypes().
    ModelTypeSet encrypted_types = SyncEncryptionHandler::SensitiveTypes();

    // Whether encrypting everything is allowed.
    bool encrypt_everything_allowed = true;

    // Whether we want to encrypt everything.
    bool encrypt_everything = false;

    // Whether we're waiting for an attempt to encryption all sync data to
    // complete. We track this at this layer in order to allow the user to
    // cancel if they e.g. don't remember their explicit passphrase.
    bool encryption_pending = false;

    // Nigori state after user switching to custom passphrase, saved until
    // transition steps complete. It will be injected into new engine after sync
    // restart.
    std::unique_ptr<SyncEncryptionHandler::NigoriState> saved_nigori_state;

    // We cache the cryptographer's pending keys whenever
    // NotifyPassphraseRequired is called. This way, before the UI calls
    // SetDecryptionPassphrase on the syncer, it can avoid the overhead of an
    // asynchronous decryption call and give the user immediate feedback about
    // the passphrase entered by first trying to decrypt the cached pending keys
    // on the UI thread. Note that SetDecryptionPassphrase can still fail after
    // the cached pending keys are successfully decrypted if the pending keys
    // have changed since the time they were cached.
    sync_pb::EncryptedData cached_pending_keys;

    // The state of the passphrase required to decrypt the bag of encryption
    // keys in the nigori node. Updated whenever a new nigori node arrives or
    // the user manually changes their passphrase state. Cached so we can
    // synchronously check it from the UI thread.
    PassphraseType cached_passphrase_type = PassphraseType::IMPLICIT_PASSPHRASE;

    // The key derivation params for the passphrase. We save them when we
    // receive a passphrase required event, as they are a necessary piece of
    // information to be able to properly perform a decryption attempt, and we
    // want to be able to synchronously do that from the UI thread. For
    // passphrase types other than CUSTOM_PASSPHRASE, their key derivation
    // method will always be PBKDF2.
    KeyDerivationParams passphrase_key_derivation_params;

    // If an explicit passphrase is in use, the time at which the passphrase was
    // first set (if available).
    base::Time cached_explicit_passphrase_time;
  } state_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncServiceCrypto> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncServiceCrypto);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_
