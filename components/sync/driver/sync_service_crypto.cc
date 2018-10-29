// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_crypto.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/nigori.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/clear_server_data_events.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/sync_string_conversions.h"

namespace syncer {

namespace {

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

  void OnCryptographerStateChanged(Cryptographer* cryptographer) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnCryptographerStateChanged,
            observer_, cryptographer));
  }

  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnPassphraseTypeChanged,
            observer_, type, passphrase_time));
  }

  void OnLocalSetPassphraseEncryption(
      const SyncEncryptionHandler::NigoriState& nigori_state) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncEncryptionHandler::Observer::OnLocalSetPassphraseEncryption,
            observer_, nigori_state));
  }

 private:
  base::WeakPtr<SyncEncryptionHandler::Observer> observer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

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

  Nigori nigori;
  bool derivation_result =
      nigori.InitByDerivation(key_derivation_params, passphrase);
  DCHECK(derivation_result);
  std::string plaintext;
  bool decrypt_result = nigori.Decrypt(pending_keys.blob(), &plaintext);
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
    CryptoSyncPrefs* sync_prefs)
    : notify_observers_(notify_observers),
      reconfigure_(reconfigure),
      sync_prefs_(sync_prefs),
      weak_factory_(this) {
  DCHECK(notify_observers_);
  DCHECK(reconfigure_);
  DCHECK(sync_prefs_);
}

SyncServiceCrypto::~SyncServiceCrypto() = default;

void SyncServiceCrypto::Reset() {
  state_ = State();
}

base::Time SyncServiceCrypto::GetExplicitPassphraseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_explicit_passphrase_time;
}

bool SyncServiceCrypto::IsUsingSecondaryPassphrase() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_passphrase_type ==
             PassphraseType::FROZEN_IMPLICIT_PASSPHRASE ||
         state_.cached_passphrase_type == PassphraseType::CUSTOM_PASSPHRASE;
}

void SyncServiceCrypto::EnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsEncryptEverythingAllowed());
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
  DCHECK(state_.passphrase_required_reason != REASON_DECRYPTION)
      << "Can not set explicit passphrase when decryption is needed.";

  DVLOG(1) << "Setting explicit passphrase for encryption.";
  if (state_.passphrase_required_reason == REASON_ENCRYPTION) {
    // REASON_ENCRYPTION implies that the cryptographer does not have pending
    // keys. Hence, as long as we're not trying to do an invalid passphrase
    // change (e.g. explicit -> explicit or explicit -> implicit), we know this
    // will succeed. If for some reason a new encryption key arrives via
    // sync later, the SBH will trigger another OnPassphraseRequired().
    state_.passphrase_required_reason = REASON_PASSPHRASE_NOT_REQUIRED;
    notify_observers_.Run();
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // SetEncryptionPassphrase() should never be called if we are currently
  // encrypted with an explicit passphrase.
  DCHECK(state_.cached_passphrase_type == PassphraseType::KEYSTORE_PASSPHRASE ||
         state_.cached_passphrase_type == PassphraseType::IMPLICIT_PASSPHRASE);

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
  if (state_.cached_passphrase_type != PassphraseType::CUSTOM_PASSPHRASE) {
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

PassphraseType SyncServiceCrypto::GetPassphraseType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.cached_passphrase_type;
}

bool SyncServiceCrypto::IsEncryptEverythingAllowed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.encrypt_everything_allowed;
}

void SyncServiceCrypto::SetEncryptEverythingAllowed(bool allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(allowed || !state_.engine || !IsEncryptEverythingEnabled());
  state_.encrypt_everything_allowed = allowed;
}

ModelTypeSet SyncServiceCrypto::GetEncryptedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.encrypted_types.Has(PASSWORDS));
  // We may be called during the setup process before we're
  // initialized. In this case, we default to the sensitive types.
  return state_.encrypted_types;
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
  state_.passphrase_required_reason = reason;

  // Reconfigure without the encrypted types (excluded implicitly via the
  // failed datatypes handler).
  reconfigure_.Run(CONFIGURE_REASON_CRYPTO);
}

void SyncServiceCrypto::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear our cache of the cryptographer's pending keys.
  state_.cached_pending_keys.clear_blob();

  // Reset |passphrase_required_reason| since we know we no longer require the
  // passphrase.
  state_.passphrase_required_reason = REASON_PASSPHRASE_NOT_REQUIRED;

  // Make sure the data types that depend on the passphrase are started at
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
  DCHECK(state_.encrypt_everything_allowed || !state_.encrypt_everything);
  DVLOG(1) << "Encrypted types changed to "
           << ModelTypeSetToString(state_.encrypted_types)
           << " (encrypt everything is set to "
           << (state_.encrypt_everything ? "true" : "false") << ")";
  DCHECK(state_.encrypted_types.Has(PASSWORDS));

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
    Cryptographer* cryptographer) {
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

void SyncServiceCrypto::OnLocalSetPassphraseEncryption(
    const SyncEncryptionHandler::NigoriState& nigori_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          switches::kSyncClearDataOnPassphraseEncryption)) {
    return;
  }

  // At this point the user has set a custom passphrase and we have received the
  // updated nigori state. Time to cache the nigori state, and catch up the
  // active data types.
  UMA_HISTOGRAM_ENUMERATION("Sync.ClearServerDataEvents",
                            CLEAR_SERVER_DATA_STARTED, CLEAR_SERVER_DATA_MAX);
  sync_prefs_->SetNigoriSpecificsForPassphraseTransition(
      nigori_state.nigori_specifics);
  sync_prefs_->SetPassphraseEncryptionTransitionInProgress(true);
  BeginConfigureCatchUpBeforeClear();
}

void SyncServiceCrypto::BeginConfigureCatchUpBeforeClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!state_.saved_nigori_state);
  state_.saved_nigori_state =
      std::make_unique<SyncEncryptionHandler::NigoriState>();
  sync_prefs_->GetNigoriSpecificsForPassphraseTransition(
      &state_.saved_nigori_state->nigori_specifics);
  reconfigure_.Run(CONFIGURE_REASON_CATCH_UP);
}

std::unique_ptr<SyncEncryptionHandler::Observer>
SyncServiceCrypto::GetEncryptionObserverProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<SyncEncryptionObserverProxy>(
      weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());
}

std::unique_ptr<SyncEncryptionHandler::NigoriState>
SyncServiceCrypto::TakeSavedNigoriState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(state_.saved_nigori_state);
}

}  // namespace syncer
