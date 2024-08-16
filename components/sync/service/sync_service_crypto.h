// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_CRYPTO_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_CRYPTO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/service/data_type_encryption_handler.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace syncer {

// This class functions as mostly independent component of SyncService that
// handles things related to encryption, including holding lots of state and
// encryption communications with the sync thread.
class SyncServiceCrypto : public SyncEncryptionHandler::Observer,
                          public DataTypeEncryptionHandler,
                          public trusted_vault::TrustedVaultClient::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void CryptoStateChanged() = 0;
    virtual void CryptoRequiredUserActionChanged() = 0;
    virtual void ReconfigureDataTypesDueToCrypto() = 0;
    virtual void PassphraseTypeChanged(PassphraseType passphrase_type) = 0;
    virtual std::optional<PassphraseType> GetPassphraseType() const = 0;
    virtual void SetEncryptionBootstrapToken(
        const std::string& bootstrap_token) = 0;
    virtual std::string GetEncryptionBootstrapToken() const = 0;
  };

  // |delegate| and |trusted_vault_client| must not be null and must outlive
  // this object.
  SyncServiceCrypto(Delegate* delegate,
                    trusted_vault::TrustedVaultClient* trusted_vault_client);

  SyncServiceCrypto(const SyncServiceCrypto&) = delete;
  SyncServiceCrypto& operator=(const SyncServiceCrypto&) = delete;

  ~SyncServiceCrypto() override;

  void Reset();
  void StopObservingTrustedVaultClient();

  // See the SyncUserSettings header.
  base::Time GetExplicitPassphraseTime() const;
  bool IsPassphraseRequired() const;
  bool IsTrustedVaultKeyRequired() const;
  bool IsTrustedVaultRecoverabilityDegraded() const;
  bool IsEncryptEverythingEnabled() const;
  void SetEncryptionPassphrase(const std::string& passphrase);
  bool SetDecryptionPassphrase(const std::string& passphrase);
  void SetExplicitPassphraseDecryptionNigoriKey(std::unique_ptr<Nigori> nigori);
  std::unique_ptr<Nigori> GetExplicitPassphraseDecryptionNigoriKey() const;

  // Returns whether it's already possible to determine whether trusted vault
  // key required (e.g. engine didn't start yet or silent fetch attempt is in
  // progress).
  bool IsTrustedVaultKeyRequiredStateKnown() const;

  // Returns the actual passphrase type being used for encryption.
  std::optional<PassphraseType> GetPassphraseType() const;

  // Used to provide the engine when it is initialized, |engine| must not be
  // null and must outlive the |this| or the Reset() call. Should not be called
  // second time, unless Reset() is called first.
  void SetSyncEngine(const CoreAccountInfo& account_info, SyncEngine* engine);

  // Creates a proxy observer object that will post calls to this thread.
  std::unique_ptr<SyncEncryptionHandler::Observer> GetEncryptionObserverProxy();

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;

  // DataTypeEncryptionHandler implementation.
  bool HasCryptoError() const override;
  DataTypeSet GetAllEncryptedDataTypes() const override;

  // TrustedVaultClient::Observer implementation.
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

 private:
  enum class RequiredUserAction {
    kUnknownDuringInitialization,
    kNone,
    kPassphraseRequired,
    // Trusted vault keys are required but a silent attempt to fetch keys is in
    // progress before prompting the user.
    kFetchingTrustedVaultKeys,
    // Silent attempt is completed and user action is definitely required to
    // retrieve trusted vault keys.
    kTrustedVaultKeyRequired,
    // The need for user action has already been surfaced to upper layers (UI)
    // via IsTrustedVaultKeyRequired() but there's an ongoing fetch that may
    // resolve the issue.
    kTrustedVaultKeyRequiredButFetching,
    // No keys are required locally but user action is recommended to improve
    // recoverability.
    kTrustedVaultRecoverabilityDegraded,
  };

  // Reads trusted vault keys from the client and feeds them to the sync engine.
  void FetchTrustedVaultKeys(bool is_second_fetch_attempt);

  // Called at various stages of asynchronously fetching and processing trusted
  // vault encryption keys. |is_second_fetch_attempt| is useful for the case
  // where multiple passes (up to two) are needed to fetch the keys from the
  // client.
  void TrustedVaultKeysFetchedFromClient(
      bool is_second_fetch_attempt,
      const std::vector<std::vector<uint8_t>>& keys);
  void TrustedVaultKeysAdded(bool is_second_fetch_attempt);
  void TrustedVaultKeysMarkedAsStale(bool is_second_fetch_attempt, bool result);
  void FetchTrustedVaultKeysCompletedButInsufficient();

  // Updates required user action and notifies observers via
  // |notify_required_user_action_changed_|.
  void UpdateRequiredUserActionAndNotify(
      RequiredUserAction new_required_user_action);

  // Invokes TrustedVaultClient::GetIsRecoverabilityDegraded() if needed.
  void RefreshIsRecoverabilityDegraded();

  // Completion callback function for
  // TrustedVaultClient::GetIsRecoverabilityDegraded().
  void GetIsRecoverabilityDegradedCompleted(bool is_recoverability_degraded);

  // Attempts decryption of |cached_pending_keys| with a |nigori| and, if
  // successful, resolves the kPassphraseRequired state and populates the
  // |nigori| to engine. Should never be called when there is no cached pending
  // keys. Returns true if successful. Doesn't update bootstrap token.
  bool SetDecryptionKeyWithoutUpdatingBootstrapToken(
      std::unique_ptr<Nigori> nigori);

  // Similar to SetDecryptionPassphrase(), but uses bootstrap token instead of
  // user provided passphrase. Resolves the kPassphraseRequired state on
  // successful attempt.
  void MaybeSetDecryptionKeyFromBootstrapToken();

  const raw_ptr<Delegate> delegate_;

  // Never null and guaranteed to outlive us.
  const raw_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;

  // All the mutable state is wrapped in a struct so that it can be easily
  // reset to its default values.
  struct State {
    State();
    ~State();

    State& operator=(State&& other) = default;

    // Not-null when the engine is initialized.
    raw_ptr<SyncEngine> engine = nullptr;

    // Populated when the engine is initialized.
    CoreAccountInfo account_info;

    // This field must be updated via UpdateRequiredUserAction() to ensure
    // observers are notified.
    RequiredUserAction required_user_action =
        RequiredUserAction::kUnknownDuringInitialization;

    // The current set of encrypted types. Always a superset of
    // AlwaysEncryptedUserTypes().
    DataTypeSet encrypted_types = AlwaysEncryptedUserTypes();

    // Whether we want to encrypt everything.
    bool encrypt_everything = false;

    // We cache the cryptographer's pending keys whenever
    // NotifyPassphraseRequired is called. This way, before the UI calls
    // SetDecryptionPassphrase on the syncer, it can avoid the overhead of an
    // asynchronous decryption call and give the user immediate feedback about
    // the passphrase entered by first trying to decrypt the cached pending keys
    // on the UI thread. Note that SetDecryptionPassphrase can still fail after
    // the cached pending keys are successfully decrypted if the pending keys
    // have changed since the time they were cached.
    sync_pb::EncryptedData cached_pending_keys;

    // The key derivation params for the passphrase. We save them when we
    // receive a passphrase required event, as they are a necessary piece of
    // information to be able to properly perform a decryption attempt, and we
    // want to be able to synchronously do that from the UI thread. For
    // passphrase types other than CUSTOM_PASSPHRASE, their key derivation
    // method will always be PBKDF2.
    KeyDerivationParams passphrase_key_derivation_params =
        KeyDerivationParams::CreateForPbkdf2();

    // If an explicit passphrase is in use, the time at which the passphrase was
    // first set (if available).
    base::Time cached_explicit_passphrase_time;

    // Set to true when FetchKeys() should be issued again once an ongoing
    // fetch-and-add procedure completes.
    bool deferred_trusted_vault_fetch_keys_pending = false;
  } state_;

  SEQUENCE_CHECKER(sequence_checker_);

  bool initial_trusted_vault_recoverability_logged_to_uma_ = false;

  base::WeakPtrFactory<SyncServiceCrypto> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_CRYPTO_H_
