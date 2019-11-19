// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_crypto.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/nigori/nigori.h"

namespace syncer {

namespace {

// Used for the case where a null client is passed to SyncServiceCrypto.
class EmptyTrustedVaultClient : public TrustedVaultClient {
 public:
  EmptyTrustedVaultClient() = default;
  ~EmptyTrustedVaultClient() override = default;

  // TrustedVaultClient implementatio.
  void FetchKeys(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::vector<std::string>&)> cb) override {
    std::move(cb).Run({});
  }

  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) override {}
};

// A SyncEncryptionHandler::Observer implementation that simply posts all calls
// to another task runner.
class SyncEncryptionObserverProxy : public SyncEncryptionHandler::Observer {
 public:
  SyncEncryptionObserverProxy(
      base::WeakPtr<SyncEncryptionHandler::Observer> observer,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : observer_(observer), task_runner_(std::move(task_runner)) {}

  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncEncryptionHandler::Observer::OnPassphraseRequired,
                       observer_, reason, key_derivation_params, pending_keys));
  }

  void OnPassphraseAccepted() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncEncryptionHandler::Observer::OnPassphraseAccepted,
                       observer_));
  }

  void OnTrustedVaultKeyRequired() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnTrustedVaultKeyRequired,
            observer_));
  }

  void OnTrustedVaultKeyAccepted() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnTrustedVaultKeyAccepted,
            observer_));
  }

  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnBootstrapTokenUpdated,
            observer_, bootstrap_token, type));
  }

  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnEncryptedTypesChanged,
            observer_, encrypted_types, encrypt_everything));
  }

  void OnEncryptionComplete() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncEncryptionHandler::Observer::OnEncryptionComplete,
                       observer_));
  }

  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnCryptographerStateChanged,
            observer_, /*cryptographer=*/nullptr, has_pending_keys));
  }

  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnPassphraseTypeChanged,
            observer_, type, passphrase_time));
  }

 private:
  base::WeakPtr<SyncEncryptionHandler::Observer> observer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

TrustedVaultClient* ResoveNullClient(TrustedVaultClient* client) {
  if (client) {
    return client;
  }

  static base::NoDestructor<EmptyTrustedVaultClient> empty_client;
  return empty_client.get();
}

// Checks if |passphrase| can be used to decrypt the given pending keys. Returns
// true if decryption was successful. Returns false otherwise. Must be called
// with non-empty pending keys cache.
bool CheckPassphraseAgainstPendingKeys(
    const sync_pb::EncryptedData& pending_keys,
    const KeyDerivationParams& key_derivation_params,
    const std::string& passphrase) {
  DCHECK(pending_keys.has_blob());
  DCHECK(!passphrase.empty());
  if (key_derivation_params.method() == KeyDerivationMethod::UNSUPPORTED) {
    DLOG(ERROR) << "Cannot derive keys using an unsupported key derivation "
                   "method. Rejecting passphrase.";
    return false;
  }

  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(key_derivation_params, passphrase);
  DCHECK(nigori);
  std::string plaintext;
  bool decrypt_result = nigori->Decrypt(pending_keys.blob(), &plaintext);
  DVLOG_IF(1, !decrypt_result) << "Passphrase failed to decrypt pending keys.";
  return decrypt_result;
}

}  // namespace

SyncServiceCrypto::State::State()
    : passphrase_key_derivation_params(KeyDerivationParams::CreateForPbkdf2()) {
}

SyncServiceCrypto::State::~State() = default;

SyncServiceCrypto::SyncServiceCrypto(
    const base::RepeatingClosure& notify_observers,
    const base::RepeatingCallback<void(ConfigureReason)>& reconfigure,
    CryptoSyncPrefs* sync_prefs,
    TrustedVaultClient* trusted_vault_client)
    : notify_observers_(notify_observers),
      reconfigure_(reconfigure),
      sync_prefs_(sync_prefs),
      trusted_vault_client_(ResoveNullClient(trusted_vault_client)) {
  DCHECK(notify_observers_);
  DCHECK(reconfigure_);
  DCHECK(sync_prefs_);
  DCHECK(trusted_vault_client_);
}

SyncServiceCrypto::~SyncServiceCrypto() = default;

void SyncServiceCrypto::Reset() {
  state_ = State();
}

base::Time SyncServiceCrypto::GetExplicitPassphraseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_explicit_passphrase_time;
}

bool SyncServiceCrypto::IsPassphraseRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_.required_user_action) {
    case RequiredUserAction::kNone:
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
      return false;
    case RequiredUserAction::kPassphraseRequiredForDecryption:
    case RequiredUserAction::kPassphraseRequiredForEncryption:
      return true;
  }

  NOTREACHED();
  return false;
}

bool SyncServiceCrypto::IsUsingSecondaryPassphrase() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsExplicitPassphrase(state_.cached_passphrase_type);
}

bool SyncServiceCrypto::IsTrustedVaultKeyRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.required_user_action ==
         RequiredUserAction::kTrustedVaultKeyRequired;
}

void SyncServiceCrypto::EnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.engine);

  // TODO(atwilson): Persist the encryption_pending flag to address the various
  // problems around cancelling encryption in the background (crbug.com/119649).
  if (!state_.encrypt_everything)
    state_.encryption_pending = true;
}

bool SyncServiceCrypto::IsEncryptEverythingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.engine);
  return state_.encrypt_everything || state_.encryption_pending;
}

void SyncServiceCrypto::SetEncryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should only be called when the engine has been initialized.
  DCHECK(state_.engine);
  DCHECK_NE(state_.required_user_action,
            RequiredUserAction::kPassphraseRequiredForDecryption)
      << "Can not set explicit passphrase when decryption is needed.";

  DVLOG(1) << "Setting explicit passphrase for encryption.";
  if (state_.required_user_action ==
      RequiredUserAction::kPassphraseRequiredForEncryption) {
    // |kPassphraseRequiredForEncryption| implies that the cryptographer does
    // not have pending keys. Hence, as long as we're not trying to do an
    // invalid passphrase change (e.g. explicit -> explicit or explicit ->
    // implicit), we know this will succeed. If for some reason a new
    // encryption key arrives via sync later, the SyncEncryptionHandler will
    // trigger another OnPassphraseRequired().
    state_.required_user_action = RequiredUserAction::kNone;
    notify_observers_.Run();
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // SetEncryptionPassphrase() should never be called if we are currently
  // encrypted with an explicit passphrase.
  DCHECK(!IsExplicitPassphrase(state_.cached_passphrase_type));

  state_.engine->SetEncryptionPassphrase(passphrase);
}

bool SyncServiceCrypto::SetDecryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called when we have cached pending keys.
  DCHECK(state_.cached_pending_keys.has_blob());

  // For types other than CUSTOM_PASSPHRASE, we should be using the old PBKDF2
  // key derivation method.
  if (state_.cached_passphrase_type != PassphraseType::kCustomPassphrase) {
    DCHECK_EQ(state_.passphrase_key_derivation_params.method(),
              KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003);
  }

  // Check the passphrase that was provided against our local cache of the
  // cryptographer's pending keys (which we cached during a previous
  // OnPassphraseRequired() event). If this was unsuccessful, the UI layer can
  // immediately call OnPassphraseRequired() again without showing the user a
  // spinner.
  if (!CheckPassphraseAgainstPendingKeys(
          state_.cached_pending_keys, state_.passphrase_key_derivation_params,
          passphrase)) {
    return false;
  }

  state_.engine->SetDecryptionPassphrase(passphrase);

  // Since we were able to decrypt the cached pending keys with the passphrase
  // provided, we immediately alert the UI layer that the passphrase was
  // accepted. This will avoid the situation where a user enters a passphrase,
  // clicks OK, immediately reopens the advanced settings dialog, and gets an
  // unnecessary prompt for a passphrase.
  // Note: It is not guaranteed that the passphrase will be accepted by the
  // syncer thread, since we could receive a new nigori node while the task is
  // pending. This scenario is a valid race, and SetDecryptionPassphrase() can
  // trigger a new OnPassphraseRequired() if it needs to.
  OnPassphraseAccepted();
  return true;
}

void SyncServiceCrypto::AddTrustedVaultDecryptionKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {
  trusted_vault_client_->StoreKeys(gaia_id, keys);

  if (state_.engine && state_.account_info.gaia == gaia_id) {
    state_.engine->AddTrustedVaultDecryptionKeys(keys, base::DoNothing());
  }
}

PassphraseType SyncServiceCrypto::GetPassphraseType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_passphrase_type;
}

ModelTypeSet SyncServiceCrypto::GetEncryptedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.encrypted_types.Has(PASSWORDS));
  DCHECK(state_.encrypted_types.Has(WIFI_CONFIGURATIONS));
  // We may be called during the setup process before we're
  // initialized. In this case, we default to the sensitive types.
  return state_.encrypted_types;
}

bool SyncServiceCrypto::HasCryptoError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This determines whether DataTypeManager should issue crypto errors for
  // encrypted datatypes. This may differ from whether the UI represents the
  // error state or not.

  switch (state_.required_user_action) {
    case RequiredUserAction::kNone:
      return false;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kPassphraseRequiredForDecryption:
    case RequiredUserAction::kPassphraseRequiredForEncryption:
      return true;
  }

  NOTREACHED();
  return false;
}

void SyncServiceCrypto::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update our cache of the cryptographer's pending keys.
  state_.cached_pending_keys = pending_keys;

  // Update the key derivation params to be used.
  state_.passphrase_key_derivation_params = key_derivation_params;

  DVLOG(1) << "Passphrase required with reason: "
           << PassphraseRequiredReasonToString(reason);

  switch (reason) {
    case REASON_ENCRYPTION:
      state_.required_user_action =
          RequiredUserAction::kPassphraseRequiredForEncryption;
      break;
    case REASON_DECRYPTION:
      state_.required_user_action =
          RequiredUserAction::kPassphraseRequiredForDecryption;
      break;
  }

  // Reconfigure without the encrypted types (excluded implicitly via the
  // failed datatypes handler).
  reconfigure_.Run(CONFIGURE_REASON_CRYPTO);
}

void SyncServiceCrypto::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear our cache of the cryptographer's pending keys.
  state_.cached_pending_keys.clear_blob();

  // Reset |required_user_action| since we know we no longer require the
  // passphrase.
  state_.required_user_action = RequiredUserAction::kNone;

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  reconfigure_.Run(CONFIGURE_REASON_CRYPTO);
}

void SyncServiceCrypto::OnTrustedVaultKeyRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // To be on the safe since, if a passphrase is required, we avoid overriding
  // |state_.required_user_action|.
  if (state_.required_user_action != RequiredUserAction::kNone) {
    return;
  }

  state_.required_user_action = RequiredUserAction::kFetchingTrustedVaultKeys;

  if (!state_.engine) {
    // If SetSyncEngine() hasn't been called yet, it means
    // OnTrustedVaultKeyRequired() was called as part of the engine's
    // initialization. Fetching the keys is not useful right now because there
    // is known engine to feed the keys to, so let's defer fetching until
    // SetSyncEngine() is called.
    return;
  }

  FetchTrustedVaultKeys();
}

void SyncServiceCrypto::OnTrustedVaultKeyAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_.required_user_action) {
    case RequiredUserAction::kNone:
    case RequiredUserAction::kPassphraseRequiredForDecryption:
    case RequiredUserAction::kPassphraseRequiredForEncryption:
      return;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
      break;
  }

  state_.required_user_action = RequiredUserAction::kNone;

  // Make sure the data types that depend on the decryption key are started at
  // this time.
  reconfigure_.Run(CONFIGURE_REASON_CRYPTO);
}

void SyncServiceCrypto::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_prefs_);
  if (type == PASSPHRASE_BOOTSTRAP_TOKEN) {
    sync_prefs_->SetEncryptionBootstrapToken(bootstrap_token);
  } else {
    sync_prefs_->SetKeystoreEncryptionBootstrapToken(bootstrap_token);
  }
}

void SyncServiceCrypto::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_.encrypted_types = encrypted_types;
  state_.encrypt_everything = encrypt_everything;
  DVLOG(1) << "Encrypted types changed to "
           << ModelTypeSetToString(state_.encrypted_types)
           << " (encrypt everything is set to "
           << (state_.encrypt_everything ? "true" : "false") << ")";
  DCHECK(state_.encrypted_types.Has(PASSWORDS));
  DCHECK(state_.encrypted_types.Has(WIFI_CONFIGURATIONS));

  notify_observers_.Run();
}

void SyncServiceCrypto::OnEncryptionComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Encryption complete";
  if (state_.encryption_pending && state_.encrypt_everything) {
    state_.encryption_pending = false;
    // This is to nudge the integration tests when encryption is
    // finished.
    notify_observers_.Run();
  }
}

void SyncServiceCrypto::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do nothing.
}

void SyncServiceCrypto::OnPassphraseTypeChanged(PassphraseType type,
                                                base::Time passphrase_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Passphrase type changed to " << PassphraseTypeToString(type);
  state_.cached_passphrase_type = type;
  state_.cached_explicit_passphrase_time = passphrase_time;
  notify_observers_.Run();
}

void SyncServiceCrypto::SetSyncEngine(const CoreAccountInfo& account_info,
                                      SyncEngine* engine) {
  DCHECK(engine);
  state_.account_info = account_info;
  state_.engine = engine;

  // This indicates OnTrustedVaultKeyRequired() was called as part of the
  // engine's initialization.
  if (state_.required_user_action ==
      RequiredUserAction::kFetchingTrustedVaultKeys) {
    FetchTrustedVaultKeys();
  }
}

std::unique_ptr<SyncEncryptionHandler::Observer>
SyncServiceCrypto::GetEncryptionObserverProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<SyncEncryptionObserverProxy>(
      weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());
}

void SyncServiceCrypto::FetchTrustedVaultKeys() {
  DCHECK(state_.engine);
  DCHECK_EQ(state_.required_user_action,
            RequiredUserAction::kFetchingTrustedVaultKeys);

  trusted_vault_client_->FetchKeys(
      state_.account_info.gaia,
      base::BindOnce(&SyncServiceCrypto::TrustedVaultKeysFetched,
                     weak_factory_.GetWeakPtr()));
}

void SyncServiceCrypto::TrustedVaultKeysFetched(
    const std::vector<std::string>& keys) {
  // The engine could have been shut down while keys were being fetched.
  if (!state_.engine) {
    return;
  }

  state_.engine->AddTrustedVaultDecryptionKeys(
      keys, base::BindOnce(&SyncServiceCrypto::TrustedVaultKeysAdded,
                           weak_factory_.GetWeakPtr()));
}

void SyncServiceCrypto::TrustedVaultKeysAdded() {
  if (state_.required_user_action !=
      RequiredUserAction::kFetchingTrustedVaultKeys) {
    return;
  }

  // Reaching this codepath indicates OnTrustedVaultKeyAccepted() was not
  // triggered, so reconfigure without the encrypted types (excluded implicitly
  // via the failed datatypes handler).
  state_.required_user_action = RequiredUserAction::kTrustedVaultKeyRequired;
  reconfigure_.Run(CONFIGURE_REASON_CRYPTO);
}

}  // namespace syncer
