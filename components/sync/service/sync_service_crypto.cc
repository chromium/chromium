// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_service_crypto.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/trusted_vault_histograms.h"

namespace syncer {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// TrustedVaultFetchKeysAttempt in
// tools/metrics/histograms/metadata/sync/enums.xml.
// LINT.IfChange(TrustedVaultFetchKeysAttempt)
enum class TrustedVaultFetchKeysAttemptForUMA {
  kFirstAttempt = 0,
  kSecondAttempt = 1,
  kMaxValue = kSecondAttempt
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:TrustedVaultFetchKeysAttempt)

// A SyncEncryptionHandler::Observer implementation that simply posts all calls
// to another task runner.
class SyncEncryptionObserverProxy : public SyncEncryptionHandler::Observer {
 public:
  SyncEncryptionObserverProxy(
      base::WeakPtr<SyncEncryptionHandler::Observer> observer,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : observer_(observer), task_runner_(std::move(task_runner)) {}

  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncEncryptionHandler::Observer::OnPassphraseRequired,
                       observer_, key_derivation_params, pending_keys));
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

  void OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                               bool encrypt_everything) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnEncryptedTypesChanged,
            observer_, encrypted_types, encrypt_everything));
  }

  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override {
    // A null cryptographer is passed to avoid usage from another sequence.
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

// Checks if |nigori| can be used to decrypt the given pending keys. Returns
// true if decryption was successful. Returns false otherwise. Must be called
// with non-empty pending keys cache.
bool CheckNigoriAgainstPendingKeys(const Nigori& nigori,
                                   const sync_pb::EncryptedData& pending_keys) {
  DCHECK(pending_keys.has_blob());

  std::string plaintext;
  bool decrypt_result = nigori.Decrypt(pending_keys.blob(), &plaintext);
  DVLOG_IF(1, !decrypt_result) << "Passphrase failed to decrypt pending keys.";
  return decrypt_result;
}

// Reads Nigori from bootstrap token. Returns nullptr if bootstrap token empty
// or corrupted.
std::unique_ptr<Nigori> ReadNigoriFromBootstrapToken(
    const std::string& bootstrap_token) {
  if (bootstrap_token.empty()) {
    return nullptr;
  }

  std::string decoded_key;
  if (!base::Base64Decode(bootstrap_token, &decoded_key)) {
    return nullptr;
  }

  std::string decrypted_key;
  if (!OSCrypt::DecryptString(decoded_key, &decrypted_key)) {
    return nullptr;
  }

  sync_pb::NigoriKey key;
  if (!key.ParseFromString(decrypted_key)) {
    return nullptr;
  }

  return Nigori::CreateByImport(key.deprecated_user_key(), key.encryption_key(),
                                key.mac_key());
}

// Serializes |nigori| as bootstrap token. Returns empty string in case of
// crypto/serialization failures.
std::string SerializeNigoriAsBootstrapToken(const Nigori& nigori) {
  sync_pb::NigoriKey proto;
  nigori.ExportKeys(proto.mutable_deprecated_user_key(),
                    proto.mutable_encryption_key(), proto.mutable_mac_key());

  const std::string serialized_key = proto.SerializeAsString();
  if (serialized_key.empty()) {
    return std::string();
  }

  std::string encrypted_key;
  if (!OSCrypt::EncryptString(serialized_key, &encrypted_key)) {
    return std::string();
  }

  return base::Base64Encode(encrypted_key);
}

}  // namespace

SyncServiceCrypto::State::State() = default;

SyncServiceCrypto::State::~State() = default;

SyncServiceCrypto::SyncServiceCrypto(
    Delegate* delegate,
    trusted_vault::TrustedVaultClient* trusted_vault_client)
    : delegate_(delegate), trusted_vault_client_(trusted_vault_client) {
  DCHECK(delegate_);
  DCHECK(trusted_vault_client_);

  trusted_vault_client_->AddObserver(this);
}

SyncServiceCrypto::~SyncServiceCrypto() = default;

void SyncServiceCrypto::Reset() {
  state_ = State();
}

void SyncServiceCrypto::StopObservingTrustedVaultClient() {
  trusted_vault_client_->RemoveObserver(this);
}

base::Time SyncServiceCrypto::GetExplicitPassphraseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_explicit_passphrase_time;
}

bool SyncServiceCrypto::IsPassphraseRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kNone:
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      return false;
    case RequiredUserAction::kPassphraseRequired:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool SyncServiceCrypto::IsTrustedVaultKeyRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.required_user_action ==
             RequiredUserAction::kTrustedVaultKeyRequired ||
         state_.required_user_action ==
             RequiredUserAction::kTrustedVaultKeyRequiredButFetching;
}

bool SyncServiceCrypto::IsTrustedVaultRecoverabilityDegraded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.required_user_action ==
         RequiredUserAction::kTrustedVaultRecoverabilityDegraded;
}

bool SyncServiceCrypto::IsEncryptEverythingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.engine);
  return state_.encrypt_everything;
}

void SyncServiceCrypto::SetEncryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should only be called when the engine has been initialized.
  DCHECK(state_.engine);
  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kNone:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      break;
    case RequiredUserAction::kPassphraseRequired:
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
      // Cryptographer has pending keys.
      // TODO(crbug.com/40904402): this is currently reachable on iOS due to
      // discrepancy in UI code. Fix iOS implementation and avoid using more
      // strict checks here until this is done.
      DUMP_WILL_BE_NOTREACHED()
          << "Can not set explicit passphrase when decryption is needed.";
      return;
  }

  DVLOG(1) << "Setting explicit passphrase for encryption.";

  // SetEncryptionPassphrase() should never be called if we are currently
  // encrypted with an explicit passphrase.
  DCHECK(!IsExplicitPassphrase(
      GetPassphraseType().value_or(PassphraseType::kKeystorePassphrase)));

  const auto key_derivation_params =
      KeyDerivationParams::CreateForScrypt(Nigori::GenerateScryptSalt());
  state_.engine->SetEncryptionPassphrase(passphrase, key_derivation_params);

  // Immediately store new bootstrap token.
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(key_derivation_params, passphrase);
  DCHECK(nigori);
  delegate_->SetEncryptionBootstrapToken(
      SerializeNigoriAsBootstrapToken(*nigori));
}

bool SyncServiceCrypto::SetDecryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should only be called when the engine has been initialized.
  DCHECK(state_.engine);

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called when we have cached pending keys.
  DCHECK(state_.cached_pending_keys.has_blob());

  // For types other than CUSTOM_PASSPHRASE, we should be using the old PBKDF2
  // key derivation method.
  if (GetPassphraseType() != PassphraseType::kCustomPassphrase) {
    DCHECK_EQ(state_.passphrase_key_derivation_params.method(),
              KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003);
  }

  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      state_.passphrase_key_derivation_params, passphrase);
  DCHECK(nigori);

  // Update the bootstrap token immediately, this is harmless as bootstrap token
  // is ignored if it doesn't contain the right key.
  delegate_->SetEncryptionBootstrapToken(
      SerializeNigoriAsBootstrapToken(*nigori));

  return SetDecryptionKeyWithoutUpdatingBootstrapToken(std::move(nigori));
}

void SyncServiceCrypto::SetExplicitPassphraseDecryptionNigoriKey(
    std::unique_ptr<Nigori> nigori) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(nigori);
  if (state_.required_user_action != RequiredUserAction::kPassphraseRequired) {
    // Passphrase not required, ignore the call.
    return;
  }

  // Update the bootstrap token immediately, this is harmless as bootstrap token
  // is ignored if it doesn't contain the right key.
  delegate_->SetEncryptionBootstrapToken(
      SerializeNigoriAsBootstrapToken(*nigori));

  if (state_.engine) {
    // Engine being initialized isn't a precondition of this method. In case
    // it's not initialized, decryption passphrase will be set later, upon
    // initialization.
    SetDecryptionKeyWithoutUpdatingBootstrapToken(std::move(nigori));
  }
}

std::unique_ptr<Nigori>
SyncServiceCrypto::GetExplicitPassphraseDecryptionNigoriKey() const {
  return ReadNigoriFromBootstrapToken(delegate_->GetEncryptionBootstrapToken());
}

bool SyncServiceCrypto::IsTrustedVaultKeyRequiredStateKnown() const {
  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kFetchingTrustedVaultKeys:
      return false;
    case RequiredUserAction::kNone:
    case RequiredUserAction::kPassphraseRequired:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::optional<PassphraseType> SyncServiceCrypto::GetPassphraseType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetPassphraseType();
}

void SyncServiceCrypto::SetSyncEngine(const CoreAccountInfo& account_info,
                                      SyncEngine* engine) {
  CHECK(engine);
  CHECK(!state_.engine);
  state_.account_info = account_info;
  state_.engine = engine;

  switch (state_.required_user_action) {
    case RequiredUserAction::kNone:
      // It was already established during initialization that there's nothing
      // to do, which is possible for some passphrase types, but not others
      // (including |kTrustedVaultPassphrase|.
      DCHECK(GetPassphraseType() != PassphraseType::kTrustedVaultPassphrase);
      break;
    case RequiredUserAction::kUnknownDuringInitialization:
      // Since there was no state changes during engine initialization, now the
      // state is known and no user action required.
      UpdateRequiredUserActionAndNotify(RequiredUserAction::kNone);
      RefreshIsRecoverabilityDegraded();
      break;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
      // This indicates OnTrustedVaultKeyRequired() was called as part of the
      // engine's initialization.
      FetchTrustedVaultKeys(/*is_second_fetch_attempt=*/false);
      break;
    case RequiredUserAction::kPassphraseRequired:
      // Attempt decryption with bootstrap token if necessary.
      MaybeSetDecryptionKeyFromBootstrapToken();
      break;
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      // Neither keys nor the recoverability state are fetched during engine
      // initialization.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

std::unique_ptr<SyncEncryptionHandler::Observer>
SyncServiceCrypto::GetEncryptionObserverProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<SyncEncryptionObserverProxy>(
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

DataTypeSet SyncServiceCrypto::GetAllEncryptedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.encrypted_types.HasAll(AlwaysEncryptedUserTypes()));
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
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kNone:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      return false;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
    case RequiredUserAction::kPassphraseRequired:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

void SyncServiceCrypto::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update our cache of the cryptographer's pending keys.
  state_.cached_pending_keys = pending_keys;

  // Update the key derivation params to be used.
  state_.passphrase_key_derivation_params = key_derivation_params;

  DVLOG(1) << "Passphrase required.";

  UpdateRequiredUserActionAndNotify(RequiredUserAction::kPassphraseRequired);

  // Reconfigure without the encrypted types (excluded implicitly via the
  // failed datatypes handler).
  delegate_->ReconfigureDataTypesDueToCrypto();

  // Attempt decryption with bootstrap token, so the user doesn't need to enter
  // the passphrase if successful.
  MaybeSetDecryptionKeyFromBootstrapToken();
}

void SyncServiceCrypto::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear our cache of the cryptographer's pending keys.
  state_.cached_pending_keys.clear_blob();

  // Reset |required_user_action| since we know we no longer require the
  // passphrase.
  UpdateRequiredUserActionAndNotify(RequiredUserAction::kNone);

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  delegate_->ReconfigureDataTypesDueToCrypto();
}

void SyncServiceCrypto::OnTrustedVaultKeyRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // To be on the safe since, if a passphrase is required, we avoid overriding
  // |state_.required_user_action|.
  if (state_.required_user_action != RequiredUserAction::kNone &&
      state_.required_user_action !=
          RequiredUserAction::kUnknownDuringInitialization) {
    return;
  }

  UpdateRequiredUserActionAndNotify(
      RequiredUserAction::kFetchingTrustedVaultKeys);

  if (!state_.engine) {
    // If SetSyncEngine() hasn't been called yet, it means
    // OnTrustedVaultKeyRequired() was called as part of the engine's
    // initialization. Fetching the keys is not useful right now because there
    // is no engine to feed the keys to, so let's defer fetching until
    // SetSyncEngine() is called.
    return;
  }

  FetchTrustedVaultKeys(/*is_second_fetch_attempt=*/false);
}

void SyncServiceCrypto::OnTrustedVaultKeyAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kNone:
    case RequiredUserAction::kPassphraseRequired:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      return;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
      break;
  }

  DCHECK(state_.engine);
  UpdateRequiredUserActionAndNotify(RequiredUserAction::kNone);
  RefreshIsRecoverabilityDegraded();

  // Make sure the data types that depend on the decryption key are started at
  // this time.
  delegate_->ReconfigureDataTypesDueToCrypto();
}

void SyncServiceCrypto::OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                                                bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_.encrypted_types = encrypted_types;
  state_.encrypt_everything = encrypt_everything;
  DVLOG(1) << "Encrypted types changed to "
           << DataTypeSetToDebugString(state_.encrypted_types)
           << " (encrypt everything is set to "
           << (state_.encrypt_everything ? "true" : "false") << ")";
  DCHECK(state_.encrypted_types.HasAll(AlwaysEncryptedUserTypes()));

  delegate_->CryptoStateChanged();
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

  state_.cached_explicit_passphrase_time = passphrase_time;

  // TODO(crbug.com/40923935): Also pass along the passphrase time?
  delegate_->PassphraseTypeChanged(type);

  // Clear recoverability degraded state in case a custom passphrase was set.
  // Note that the opposite transition (into degraded recoverability) isn't
  // handled here, i.e. RefreshIsRecoverabilityDegraded() isn't invoked, as
  // it can be safely assumed that in practice either of
  // OnTrustedVaultKeyRequired() or OnTrustedVaultKeyAccepted() will eventually
  // be invoked.
  if (type != PassphraseType::kTrustedVaultPassphrase &&
      state_.required_user_action ==
          RequiredUserAction::kTrustedVaultRecoverabilityDegraded) {
    UpdateRequiredUserActionAndNotify(RequiredUserAction::kNone);
  }

  delegate_->CryptoStateChanged();
}

void SyncServiceCrypto::OnTrustedVaultKeysChanged() {
  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kNone:
    case RequiredUserAction::kPassphraseRequired:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      // If no trusted vault keys are required, there's nothing to do. If they
      // later are required, a fetch will be triggered in
      // OnTrustedVaultKeyRequired().
      return;
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
      // If there's an ongoing fetch, FetchKeys() cannot be issued immediately
      // since that violates the function precondition. However, the in-flight
      // FetchKeys() may end up returning stale keys, so let's make sure
      // FetchKeys() is invoked again once it becomes possible.
      state_.deferred_trusted_vault_fetch_keys_pending = true;
      return;
    case RequiredUserAction::kTrustedVaultKeyRequired:
      UpdateRequiredUserActionAndNotify(
          RequiredUserAction::kTrustedVaultKeyRequiredButFetching);
      break;
  }

  FetchTrustedVaultKeys(/*is_second_fetch_attempt=*/false);
}

void SyncServiceCrypto::OnTrustedVaultRecoverabilityChanged() {
  // Ignore calls during engine initialization, as decoverability will be
  // refreshed in SetSyncEngine().
  if (!state_.engine) {
    return;
  }
  RefreshIsRecoverabilityDegraded();
}

void SyncServiceCrypto::FetchTrustedVaultKeys(bool is_second_fetch_attempt) {
  DCHECK(state_.engine);
  DCHECK(state_.required_user_action ==
             RequiredUserAction::kFetchingTrustedVaultKeys ||
         state_.required_user_action ==
             RequiredUserAction::kTrustedVaultKeyRequiredButFetching);

  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultFetchKeysAttempt",
      is_second_fetch_attempt
          ? TrustedVaultFetchKeysAttemptForUMA::kSecondAttempt
          : TrustedVaultFetchKeysAttemptForUMA::kFirstAttempt);

  if (!is_second_fetch_attempt) {
    state_.deferred_trusted_vault_fetch_keys_pending = false;
  }

  trusted_vault_client_->FetchKeys(
      state_.account_info,
      base::BindOnce(&SyncServiceCrypto::TrustedVaultKeysFetchedFromClient,
                     weak_factory_.GetWeakPtr(), is_second_fetch_attempt));
}

void SyncServiceCrypto::TrustedVaultKeysFetchedFromClient(
    bool is_second_fetch_attempt,
    const std::vector<std::vector<uint8_t>>& keys) {
  if (state_.required_user_action !=
          RequiredUserAction::kFetchingTrustedVaultKeys &&
      state_.required_user_action !=
          RequiredUserAction::kTrustedVaultKeyRequiredButFetching) {
    return;
  }

  DCHECK(state_.engine);

  base::UmaHistogramCounts100("Sync.TrustedVaultFetchedKeysCount", keys.size());

  if (keys.empty()) {
    // Nothing to do if no keys have been fetched from the client (e.g. user
    // action is required for fetching additional keys). Let's avoid unnecessary
    // steps like marking keys as stale.
    FetchTrustedVaultKeysCompletedButInsufficient();
    return;
  }

  state_.engine->AddTrustedVaultDecryptionKeys(
      keys,
      base::BindOnce(&SyncServiceCrypto::TrustedVaultKeysAdded,
                     weak_factory_.GetWeakPtr(), is_second_fetch_attempt));
}

void SyncServiceCrypto::TrustedVaultKeysAdded(bool is_second_fetch_attempt) {
  // Having kFetchingTrustedVaultKeys or kTrustedVaultKeyRequiredButFetching
  // indicates OnTrustedVaultKeyAccepted() was not triggered, so the fetched
  // trusted vault keys were insufficient.
  bool success = state_.required_user_action !=
                     RequiredUserAction::kFetchingTrustedVaultKeys &&
                 state_.required_user_action !=
                     RequiredUserAction::kTrustedVaultKeyRequiredButFetching;

  base::UmaHistogramBoolean("Sync.TrustedVaultAddKeysAttemptIsSuccessful",
                            success);

  if (success) {
    return;
  }

  // Let trusted vault client know, that fetched keys were insufficient.
  trusted_vault_client_->MarkLocalKeysAsStale(
      state_.account_info,
      base::BindOnce(&SyncServiceCrypto::TrustedVaultKeysMarkedAsStale,
                     weak_factory_.GetWeakPtr(), is_second_fetch_attempt));
}

void SyncServiceCrypto::TrustedVaultKeysMarkedAsStale(
    bool is_second_fetch_attempt,
    bool result) {
  if (state_.required_user_action !=
          RequiredUserAction::kFetchingTrustedVaultKeys &&
      state_.required_user_action !=
          RequiredUserAction::kTrustedVaultKeyRequiredButFetching) {
    return;
  }

  // If nothing has changed (determined by |!result| since false negatives are
  // disallowed by the API) or this is already a second attempt, the fetching
  // procedure can be considered completed.
  if (!result || is_second_fetch_attempt) {
    FetchTrustedVaultKeysCompletedButInsufficient();
    return;
  }

  FetchTrustedVaultKeys(/*is_second_fetch_attempt=*/true);
}

void SyncServiceCrypto::FetchTrustedVaultKeysCompletedButInsufficient() {
  DCHECK(state_.required_user_action ==
             RequiredUserAction::kFetchingTrustedVaultKeys ||
         state_.required_user_action ==
             RequiredUserAction::kTrustedVaultKeyRequiredButFetching);

  // If FetchKeys() was intended to be called during an already existing ongoing
  // FetchKeys(), it needs to be invoked now that it's possible.
  if (state_.deferred_trusted_vault_fetch_keys_pending) {
    FetchTrustedVaultKeys(/*is_second_fetch_attempt=*/false);
    return;
  }

  // Reaching this codepath indicates OnTrustedVaultKeyAccepted() was not
  // triggered, so the fetched trusted vault keys were insufficient.
  UpdateRequiredUserActionAndNotify(
      RequiredUserAction::kTrustedVaultKeyRequired);

  // Reconfigure without the encrypted types (excluded implicitly via the failed
  // datatypes handler).
  delegate_->ReconfigureDataTypesDueToCrypto();
}

void SyncServiceCrypto::UpdateRequiredUserActionAndNotify(
    RequiredUserAction new_required_user_action) {
  DCHECK_NE(new_required_user_action,
            RequiredUserAction::kUnknownDuringInitialization);

  if (state_.required_user_action == new_required_user_action) {
    return;
  }

  state_.required_user_action = new_required_user_action;
  delegate_->CryptoRequiredUserActionChanged();
}

void SyncServiceCrypto::RefreshIsRecoverabilityDegraded() {
  DCHECK(state_.engine);

  if (GetPassphraseType() != PassphraseType::kTrustedVaultPassphrase) {
    return;
  }

  switch (state_.required_user_action) {
    case RequiredUserAction::kUnknownDuringInitialization:
    case RequiredUserAction::kFetchingTrustedVaultKeys:
    case RequiredUserAction::kTrustedVaultKeyRequired:
    case RequiredUserAction::kTrustedVaultKeyRequiredButFetching:
    case RequiredUserAction::kPassphraseRequired:
      return;
    case RequiredUserAction::kNone:
    case RequiredUserAction::kTrustedVaultRecoverabilityDegraded:
      break;
  }

  trusted_vault_client_->GetIsRecoverabilityDegraded(
      state_.account_info,
      base::BindOnce(&SyncServiceCrypto::GetIsRecoverabilityDegradedCompleted,
                     weak_factory_.GetWeakPtr()));
}

void SyncServiceCrypto::GetIsRecoverabilityDegradedCompleted(
    bool is_recoverability_degraded) {
  // |engine| could have been reset.
  if (!state_.engine) {
    DCHECK_EQ(state_.required_user_action,
              RequiredUserAction::kUnknownDuringInitialization);
    return;
  }

  // The passphrase type could have changed.
  if (GetPassphraseType() != PassphraseType::kTrustedVaultPassphrase) {
    DCHECK_NE(state_.required_user_action,
              RequiredUserAction::kTrustedVaultRecoverabilityDegraded);
    return;
  }

  // Transition from non-degraded to degraded recoverability.
  if (is_recoverability_degraded &&
      state_.required_user_action == RequiredUserAction::kNone) {
    UpdateRequiredUserActionAndNotify(
        RequiredUserAction::kTrustedVaultRecoverabilityDegraded);
    delegate_->CryptoStateChanged();
  }

  // Transition from degraded to non-degraded recoverability.
  if (!is_recoverability_degraded &&
      state_.required_user_action ==
          RequiredUserAction::kTrustedVaultRecoverabilityDegraded) {
    UpdateRequiredUserActionAndNotify(RequiredUserAction::kNone);
    delegate_->CryptoStateChanged();
  }

  if (!initial_trusted_vault_recoverability_logged_to_uma_) {
    DCHECK(state_.engine);

    initial_trusted_vault_recoverability_logged_to_uma_ = true;
    RecordTrustedVaultHistogramBooleanWithMigrationSuffix(
        "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
        is_recoverability_degraded, state_.engine->GetDetailedStatus());
  }
}

bool SyncServiceCrypto::SetDecryptionKeyWithoutUpdatingBootstrapToken(
    std::unique_ptr<Nigori> nigori) {
  DCHECK(nigori);
  // This should only be called when we have cached pending keys.
  DCHECK(state_.cached_pending_keys.has_blob());

  // Check the passphrase that was provided against our local cache of the
  // cryptographer's pending keys (which we cached during a previous
  // OnPassphraseRequired() event). If this was unsuccessful, the UI layer can
  // immediately call OnPassphraseRequired() again without showing the user a
  // spinner.
  if (!CheckNigoriAgainstPendingKeys(*nigori, state_.cached_pending_keys)) {
    return false;
  }

  state_.engine->SetExplicitPassphraseDecryptionKey(std::move(nigori));

  // Since we were able to decrypt the cached pending keys with the passphrase
  // provided, we immediately alert the UI layer that the passphrase was
  // accepted. This will avoid the situation where a user enters a passphrase,
  // clicks OK, immediately reopens the advanced settings dialog, and gets an
  // unnecessary prompt for a passphrase.
  // Note: It is not guaranteed that the passphrase will be accepted by the
  // syncer thread, since we could receive a new nigori node while the task is
  // pending. This scenario is a valid race, and
  // SetExplicitPassphraseDecryptionKey() can trigger a new
  // OnPassphraseRequired() if it needs to.
  OnPassphraseAccepted();
  return true;
}

void SyncServiceCrypto::MaybeSetDecryptionKeyFromBootstrapToken() {
  if (!state_.engine) {
    // Engine initialization isn't complete yet, attempt decryption upon
    // initialization.
    return;
  }
  std::unique_ptr<Nigori> nigori =
      ReadNigoriFromBootstrapToken(delegate_->GetEncryptionBootstrapToken());
  if (!nigori) {
    return;
  }

  SetDecryptionKeyWithoutUpdatingBootstrapToken(std::move(nigori));
}

}  // namespace syncer
