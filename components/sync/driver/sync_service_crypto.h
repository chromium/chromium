// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_engine.h"

namespace syncer {

class CryptoSyncPrefs;
class TrustedVaultClient;

// This class functions as mostly independent component of SyncService that
// handles things related to encryption, including holding lots of state and
// encryption communications with the sync thread.
class SyncServiceCrypto : public SyncEncryptionHandler::Observer,
                          public DataTypeEncryptionHandler {
 public:
  // |sync_prefs| must not be null and must outlive this object.
  // |trusted_vault_client| may be null, but if non-null, the pointee must
  // outlive this object.
  SyncServiceCrypto(
      const base::RepeatingClosure& notify_observers,
      const base::RepeatingCallback<void(ConfigureReason)>& reconfigure,
      CryptoSyncPrefs* sync_prefs,
      TrustedVaultClient* trusted_vault_client);
  ~SyncServiceCrypto() override;

  void Reset();

  // See the SyncService header.
  base::Time GetExplicitPassphraseTime() const;
  bool IsPassphraseRequired() const;
  bool IsUsingSecondaryPassphrase() const;
  bool IsTrustedVaultKeyRequired() const;
  void EnableEncryptEverything();
  bool IsEncryptEverythingEnabled() const;
  void SetEncryptionPassphrase(const std::string& passphrase);
  bool SetDecryptionPassphrase(const std::string& passphrase);
  void AddTrustedVaultDecryptionKeys(const std::string& gaia_id,
                                     const std::vector<std::string>& keys);

  // Returns the actual passphrase type being used for encryption.
  PassphraseType GetPassphraseType() const;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;

  // DataTypeEncryptionHandler implementation.
  bool HasCryptoError() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  // Used to provide the engine when it is initialized.
  void SetSyncEngine(const CoreAccountInfo& account_info, SyncEngine* engine);

  // Creates a proxy observer object that will post calls to this thread.
  std::unique_ptr<SyncEncryptionHandler::Observer> GetEncryptionObserverProxy();

  bool encryption_pending() const { return state_.encryption_pending; }

 private:
  enum class RequiredUserAction {
    kNone,
    kPassphraseRequiredForDecryption,
    kPassphraseRequiredForEncryption,
    // Trusted vault keys are required but a silent attempt to fetch keys is in
    // progress before prompting the user.
    kFetchingTrustedVaultKeys,
    // Silent attempt is completed and user action is definitely required to
    // retrieve trusted vault keys.
    kTrustedVaultKeyRequired,
  };

  // Reads trusted vault keys from the client and feeds them to the sync engine.
  void FetchTrustedVaultKeys();

  // Called at various stages of asynchronously fetching and processing trusted
  // vault encryption keys.
  void TrustedVaultKeysFetched(const std::vector<std::string>& keys);
  void TrustedVaultKeysAdded();

  // Calls SyncServiceBase::NotifyObservers(). Never null.
  const base::RepeatingClosure notify_observers_;

  const base::RepeatingCallback<void(ConfigureReason)> reconfigure_;

  // A pointer to the crypto-relevant sync prefs. Never null and guaranteed to
  // outlive us.
  CryptoSyncPrefs* const sync_prefs_;

  // Never null and guaranteed to outlive us.
  TrustedVaultClient* const trusted_vault_client_;

  // All the mutable state is wrapped in a struct so that it can be easily
  // reset to its default values.
  struct State {
    State();
    ~State();

    State& operator=(State&& other) = default;

    // Not-null when the engine is initialized.
    SyncEngine* engine = nullptr;

    // Populated when the engine is initialized.
    CoreAccountInfo account_info;

    RequiredUserAction required_user_action = RequiredUserAction::kNone;

    // The current set of encrypted types. Always a superset of
    // Cryptographer::SensitiveTypes().
    ModelTypeSet encrypted_types = SyncEncryptionHandler::SensitiveTypes();

    // Whether we want to encrypt everything.
    bool encrypt_everything = false;

    // Whether we're waiting for an attempt to encryption all sync data to
    // complete. We track this at this layer in order to allow the user to
    // cancel if they e.g. don't remember their explicit passphrase.
    bool encryption_pending = false;

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
    PassphraseType cached_passphrase_type = PassphraseType::kImplicitPassphrase;

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

  base::WeakPtrFactory<SyncServiceCrypto> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncServiceCrypto);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_CRYPTO_H_
