// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "components/sync/base/custom_passphrase_bootstrap_token.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/time.h"
#include "components/sync/nigori/cross_user_sharing_public_key.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/key_derivation_params.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori_storage.h"
#include "components/sync/nigori/pending_local_nigori_commit.h"
#include "components/sync/nigori/required_passphrase_verifier_impl.h"
#include "components/sync/nigori/sync_encryption_handler_observer_list.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;

const char kNigoriNonUniqueName[] = "Nigori";

// Enumeration of possible values for a key derivation method (including a
// special value of "not set"). Used in UMA metrics. Do not re-order or delete
// these entries; they are used in a UMA histogram.  Please edit
// SyncCustomPassphraseKeyDerivationMethodState in enums.xml if a value is
// added.
// LINT.IfChange(SyncCustomPassphraseKeyDerivationMethodState)
enum class KeyDerivationMethodStateForMetrics {
  NOT_SET = 0,
  DEPRECATED_UNSUPPORTED = 1,
  PBKDF2_HMAC_SHA1_1003 = 2,
  SCRYPT_8192_8_11 = 3,
  kMaxValue = SCRYPT_8192_8_11
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncCustomPassphraseKeyDerivationMethodState)

KeyDerivationMethodStateForMetrics GetKeyDerivationMethodStateForMetrics(
    const std::optional<KeyDerivationParams>& key_derivation_params) {
  if (!key_derivation_params.has_value()) {
    return KeyDerivationMethodStateForMetrics::NOT_SET;
  }
  switch (key_derivation_params.value().method()) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationMethodStateForMetrics::PBKDF2_HMAC_SHA1_1003;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationMethodStateForMetrics::SCRYPT_8192_8_11;
  }

  NOTREACHED();
}

std::string GetScryptSaltFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  DCHECK_EQ(specifics.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  std::string decoded_salt;
  bool result = base::Base64Decode(
      specifics.custom_passphrase_key_derivation_salt(), &decoded_salt);
  DCHECK(result);
  return decoded_salt;
}

KeyDerivationParams GetKeyDerivationParamsFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  std::optional<KeyDerivationMethod> key_derivation_method =
      ProtoKeyDerivationMethodToEnum(
          specifics.custom_passphrase_key_derivation_method());
  // Guaranteed by validations (e.g. SpecificsHasValidKeyDerivationParams()).
  DCHECK(key_derivation_method);

  switch (*key_derivation_method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          GetScryptSaltFromSpecifics(specifics));
  }

  NOTREACHED();
}

// We need to apply base64 encoding before deriving Nigori keys because the
// underlying crypto libraries (in particular the Java counterparts in JDK's
// implementation for PBKDF2) assume the keys are utf8.
std::vector<std::string> Base64EncodeKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  std::vector<std::string> encoded_keystore_keys;
  for (const std::vector<uint8_t>& key : keys) {
    encoded_keystore_keys.push_back(base::Base64Encode(key));
  }
  return encoded_keystore_keys;
}

bool SpecificsHasValidKeyDerivationParams(const NigoriSpecifics& specifics) {
  std::optional<KeyDerivationMethod> key_derivation_method =
      ProtoKeyDerivationMethodToEnum(
          specifics.custom_passphrase_key_derivation_method());
  if (!key_derivation_method) {
    DLOG(ERROR) << "Unsupported key derivation method encountered: "
                << specifics.custom_passphrase_key_derivation_method();
    return false;
  }
  switch (*key_derivation_method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return true;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      if (!specifics.has_custom_passphrase_key_derivation_salt()) {
        DLOG(ERROR) << "Missed key derivation salt while key derivation "
                    << "method is SCRYPT_8192_8_11.";
        return false;
      }
      std::string temp;
      if (!base::Base64Decode(specifics.custom_passphrase_key_derivation_salt(),
                              &temp)) {
        DLOG(ERROR) << "Key derivation salt is not a valid base64 encoded "
                       "string.";
        return false;
      }
      return true;
  }
}

// Validates given `specifics` assuming it's not specifics received from the
// server during first-time sync for current user (i.e. it's not a default
// specifics).
bool IsValidNigoriSpecifics(const NigoriSpecifics& specifics) {
  if (specifics.encryption_keybag().blob().empty()) {
    DLOG(ERROR) << "Specifics contains empty encryption_keybag.";
    return false;
  }
  switch (ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type())) {
    case NigoriSpecifics::UNKNOWN:
      DLOG(ERROR) << "Received unknown passphrase type with value: "
                  << specifics.passphrase_type();
      return false;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return true;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      if (specifics.keystore_decryptor_token().blob().empty()) {
        DLOG(ERROR) << "Keystore Nigori should have filled "
                    << "keystore_decryptor_token.";
        return false;
      }
      break;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      if (!SpecificsHasValidKeyDerivationParams(specifics)) {
        return false;
      }
      [[fallthrough]];
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      if (!specifics.encrypt_everything()) {
        DLOG(ERROR) << "Nigori with explicit passphrase type should have "
                       "enabled encrypt_everything.";
        return false;
      }
      break;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return true;
  }
  return true;
}

bool IsValidPassphraseTransition(
    NigoriSpecifics::PassphraseType old_passphrase_type,
    NigoriSpecifics::PassphraseType new_passphrase_type) {
  // We assume that `new_passphrase_type` is valid.
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);

  if (old_passphrase_type == new_passphrase_type) {
    return true;
  }
  switch (old_passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      // This can happen iff we have not synced local state yet or synced with
      // default NigoriSpecifics, so we accept any valid passphrase type
      // (invalid filtered before).
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return true;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE ||
             new_passphrase_type == NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      // There is no client side code which can cause such transition, but
      // technically it's a valid one and can be implemented in the future.
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return false;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE ||
             new_passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE;
  }
  NOTREACHED();
}

// Updates `*current_type` if needed. Returns true if its value was changed.
bool UpdatePassphraseType(NigoriSpecifics::PassphraseType new_type,
                          NigoriSpecifics::PassphraseType* current_type) {
  DCHECK(current_type);
  DCHECK(IsValidPassphraseTransition(*current_type, new_type));
  if (*current_type == new_type) {
    return false;
  }
  *current_type = new_type;
  return true;
}

bool IsValidEncryptedTypesTransition(bool old_encrypt_everything,
                                     const NigoriSpecifics& specifics) {
  // We don't support relaxing the encryption requirements.
  return specifics.encrypt_everything() || !old_encrypt_everything;
}

bool IsValidLocalData(const sync_pb::NigoriLocalData& local_data) {
  if (local_data.data_type_state().initial_sync_state() !=
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE) {
    // `local_data` should not be stored before initial sync is done.
    return false;
  }

  const sync_pb::NigoriModel& nigori_model = local_data.nigori_model();

  if (!CryptographerImpl::IsLocalProtoValid(
          nigori_model.cryptographer_data())) {
    return false;
  }

  switch (nigori_model.passphrase_type()) {
    case NigoriSpecifics::UNKNOWN:
      // The only legit way to persist UNKNOWN passphrase type is to not
      // complete keystore initialization upon initial sync, the keystore keys
      // are supposed to be available - otherwise bridge issues ModelError.
      return nigori_model.keystore_key_size() > 0;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      if (nigori_model.custom_passphrase_key_derivation_params()
              .custom_passphrase_key_derivation_method() ==
          NigoriSpecifics::UNSPECIFIED) {
        // Custom passphrase Nigori should have specified key derivation method.
        return false;
      }
      [[fallthrough]];
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      // With real passphrase type encryption keys must always be present
      // (either decrypted or pending decryption).
      return nigori_model.cryptographer_data().key_bag().key_size() > 0 ||
             nigori_model.has_pending_keys();
  }

  // All new validation logic should be added either before or into the switch
  // above.
  NOTREACHED();
}

std::optional<CrossUserSharingPublicKey> PublicKeyFromProto(
    const sync_pb::CrossUserSharingPublicKey& public_key) {
  if (!public_key.has_version()) {
    return std::nullopt;
  }
  std::vector<uint8_t> key(public_key.x25519_public_key().begin(),
                           public_key.x25519_public_key().end());
  return CrossUserSharingPublicKey::CreateByImport(key);
}

}  // namespace

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl(
    std::unique_ptr<NigoriLocalChangeProcessor> processor,
    std::unique_ptr<NigoriStorage> storage)
    : processor_(std::move(processor)), storage_(std::move(storage)) {
  std::optional<sync_pb::NigoriLocalData> deserialized_data =
      storage_->RestoreData();
  if (!deserialized_data || !IsValidLocalData(*deserialized_data)) {
    // We either have no Nigori node stored locally or it was corrupted.
    processor_->ModelReadyToSync(this, NigoriMetadataBatch());
    return;
  }

  // Restore metadata.
  NigoriMetadataBatch metadata_batch;
  metadata_batch.data_type_state = deserialized_data->data_type_state();
  metadata_batch.entity_metadata = deserialized_data->entity_metadata();
  processor_->ModelReadyToSync(this, std::move(metadata_batch));

  if (!processor_->IsTrackingMetadata()) {
    // The processor dropped the metadata (e.g. due to validation failure), so
    // don't restore the data.
    return;
  }

  // Restore data.
  state_ = syncer::NigoriState::CreateFromLocalProto(
      deserialized_data->nigori_model());

  if (state_.passphrase_type == NigoriSpecifics::UNKNOWN) {
    // Commit with keystore initialization wasn't successfully completed before
    // the restart, so trigger it again here.
    DCHECK(!state_.keystore_keys_cryptographer->IsEmpty());
    QueuePendingLocalCommit(
        PendingLocalNigoriCommit::ForKeystoreInitialization());
  }

  if (state_.NeedsGenerateCrossUserSharingKeyPair()) {
    QueuePendingLocalCommit(
        PendingLocalNigoriCommit::
            ForCrossUserSharingPublicPrivateKeyInitializer());
  }
}

NigoriSyncBridgeImpl::~NigoriSyncBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriSyncBridgeImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void NigoriSyncBridgeImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void NigoriSyncBridgeImpl::NotifyInitialStateToObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We need to expose whole bridge state through notifications, because it
  // can be different from default due to restoring from the file or
  // completeness of first sync cycle (which happens before Init() call).
  observer_list_.NotifyEncryptedTypesChanged(state_.GetEncryptedTypes(),
                                             state_.encrypt_everything);
  observer_list_.NotifyCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());

  MaybeNotifyOfPendingKeys();

  if (state_.passphrase_type != NigoriSpecifics::UNKNOWN) {
    // if `passphrase_type` is unknown, it is not yet initialized and we
    // shouldn't expose it.
    PassphraseType enum_passphrase_type =
        *ProtoPassphraseInt32ToEnum(state_.passphrase_type);
    observer_list_.NotifyPassphraseTypeChanged(enum_passphrase_type,
                                               GetExplicitPassphraseTime());
    UMA_HISTOGRAM_ENUMERATION("Sync.PassphraseType", enum_passphrase_type);
  }
  if (state_.passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE) {
    UMA_HISTOGRAM_ENUMERATION(
        "Sync.Crypto.CustomPassphraseKeyDerivationMethodStateOnStartup",
        GetKeyDerivationMethodStateForMetrics(
            state_.custom_passphrase_key_derivation_params));
  }
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerReady",
                        state_.cryptographer->CanEncrypt());
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerPendingKeys",
                        state_.pending_keys.has_value());
  if (state_.pending_keys.has_value() &&
      state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    // If this is happening, it means the keystore decryptor is either
    // undecryptable with the available keystore keys or does not match the
    // nigori keybag's encryption key. Otherwise we're simply missing the
    // keystore key.
    UMA_HISTOGRAM_BOOLEAN("Sync.KeystoreDecryptionFailed",
                          !state_.keystore_keys_cryptographer->IsEmpty());
  }
}

DataTypeSet NigoriSyncBridgeImpl::GetEncryptedTypes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.GetEncryptedTypes();
}

Cryptographer* NigoriSyncBridgeImpl::GetCryptographer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_.cryptographer);
  return state_.cryptographer.get();
}

PassphraseType NigoriSyncBridgeImpl::GetPassphraseType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ProtoPassphraseInt32ToEnum(state_.passphrase_type)
      .value_or(PassphraseType::kImplicitPassphrase);
}

void NigoriSyncBridgeImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QueuePendingLocalCommit(
      PendingLocalNigoriCommit::ForSetCustomPassphrase(passphrase));
}

void NigoriSyncBridgeImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (passphrase.empty()) {
    return;
  }
  NigoriKeyBag tmp_key_bag = NigoriKeyBag::CreateEmpty();
  tmp_key_bag.AddKey(GetKeyDerivationParamsForPendingKeys(), passphrase);
  SetExplicitPassphraseDecryptionKeyBag(tmp_key_bag);
}

void NigoriSyncBridgeImpl::SetDecryptionBootstrapToken(
    const CustomPassphraseBootstrapToken& bootstrap_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bootstrap_token.IsEmpty()) {
    return;
  }
  NigoriKeyBag tmp_key_bag = NigoriKeyBag::CreateEmpty();
  tmp_key_bag.AddKeyFromProto(bootstrap_token.ToProto());
  SetExplicitPassphraseDecryptionKeyBag(tmp_key_bag);
}

void NigoriSyncBridgeImpl::SetExplicitPassphraseDecryptionKeyBag(
    const NigoriKeyBag& key_bag) {
  if (!state_.pending_keys) {
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);
    return;
  }

  std::optional<ModelError> error = TryDecryptPendingKeysWith(key_bag);
  if (error.has_value()) {
    processor_->ReportError(*error);
    return;
  }

  if (state_.pending_keys.has_value()) {
    // `pending_keys` could be changed in between of OnPassphraseRequired()
    // and SetExplicitPassphraseDecryptionKey() calls (remote update with
    // different keystore Nigori or with transition from keystore to custom
    // passphrase Nigori).
    MaybeNotifyOfPendingKeys();
    return;
  }

  if (state_.passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE) {
    DCHECK(state_.custom_passphrase_key_derivation_params.has_value());
    UMA_HISTOGRAM_ENUMERATION(
        "Sync.Crypto."
        "CustomPassphraseKeyDerivationMethodOnSuccessfulDecryption",
        GetKeyDerivationMethodStateForMetrics(
            state_.custom_passphrase_key_derivation_params));
  }

  storage_->StoreData(SerializeAsNigoriLocalData());
  observer_list_.NotifyCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());
  CustomPassphraseBootstrapToken token =
      CustomPassphraseBootstrapToken::FromProto(
          state_.cryptographer->ExportDefaultKey());
  observer_list_.NotifyPassphraseAccepted(token);
}

void NigoriSyncBridgeImpl::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  // This API gets plumbed and ultimately exposed to layers outside the sync
  // codebase and even outside the browser, so there are no preconditions and
  // instead we ignore invalid or partially invalid input.
  if (state_.passphrase_type != NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE ||
      !state_.pending_keys || keys.empty()) {
    return;
  }

  const std::vector<std::string> encoded_keys = Base64EncodeKeys(keys);
  NigoriKeyBag tmp_key_bag = NigoriKeyBag::CreateEmpty();
  for (const std::string& encoded_key : encoded_keys) {
    tmp_key_bag.AddKey(GetKeyDerivationParamsForPendingKeys(), encoded_key);
  }

  std::optional<ModelError> error = TryDecryptPendingKeysWith(tmp_key_bag);
  if (error.has_value()) {
    processor_->ReportError(*error);
    return;
  }

  if (state_.pending_keys.has_value()) {
    return;
  }

  state_.last_default_trusted_vault_key_name =
      state_.cryptographer->GetDefaultEncryptionKeyName();

  storage_->StoreData(SerializeAsNigoriLocalData());
  observer_list_.NotifyCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());
  observer_list_.NotifyTrustedVaultKeyAccepted();
}

base::Time NigoriSyncBridgeImpl::GetKeystoreMigrationTime() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.keystore_migration_time;
}

KeystoreKeysHandler* NigoriSyncBridgeImpl::GetKeystoreKeysHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

const sync_pb::NigoriSpecifics::TrustedVaultDebugInfo&
NigoriSyncBridgeImpl::GetTrustedVaultDebugInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.trusted_vault_debug_info;
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Explicitly asks the server for keystore keys if it's first-time sync, i.e.
  // if there is no keystore keys yet or remote keybag wasn't decryptable due
  // to absence of some keystore key. In case of key rotation, it's a server
  // responsibility to send updated keystore keys. `keystore_keys_` is expected
  // to be non-empty before MergeFullSyncData() call, regardless of passphrase
  // type.
  return state_.keystore_keys_cryptographer->IsEmpty() ||
         (state_.pending_keystore_decryptor_token.has_value() &&
          state_.pending_keys.has_value());
}

bool NigoriSyncBridgeImpl::SetKeystoreKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (keys.empty() || keys.back().empty()) {
    return false;
  }

  state_.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(Base64EncodeKeys(keys));
  if (!state_.keystore_keys_cryptographer) {
    state_.keystore_keys_cryptographer =
        KeystoreKeysCryptographer::CreateEmpty();
    return false;
  }

  if (state_.pending_keystore_decryptor_token.has_value() &&
      state_.pending_keys.has_value()) {
    // Newly arrived keystore keys could resolve pending encryption state in
    // keystore mode.
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);

    std::optional<ModelError> error =
        TryDecryptPendingKeysWith(BuildDecryptionKeyBagForRemoteKeybag());
    if (error.has_value()) {
      processor_->ReportError(*error);
      return false;
    }

    if (!state_.pending_keys.has_value()) {
      observer_list_.NotifyCryptographerStateChanged(
          state_.cryptographer.get(), state_.pending_keys.has_value());
      observer_list_.NotifyKeystoreKeysAccepted();
    }
  }

  // Note: we don't need to persist keystore keys here, because we will receive
  // Nigori node right after this method and persist all the data during
  // UpdateLocalState().
  return true;
}

std::optional<ModelError> NigoriSyncBridgeImpl::MergeFullSyncData(
    std::optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    return ModelError(
        FROM_HERE, ModelError::Type::kNigoriEmptyEntityDataDuringInitialSync);
  }
  DCHECK(data->specifics.has_nigori());

  const NigoriSpecifics& specifics = data->specifics.nigori();
  if (specifics.passphrase_type() != NigoriSpecifics::IMPLICIT_PASSPHRASE ||
      !specifics.encryption_keybag().blob().empty()) {
    // We received regular Nigori.
    // TODO(crbug.com/40267990): consider generating a new public-private key
    // pair after the initial sync.
    return UpdateLocalState(data->specifics.nigori());
  }
  // Ensure we have `keystore_keys` during the initial download, requested to
  // the server as per NeedKeystoreKey(), and required for initializing the
  // default keystore Nigori.
  DCHECK(state_.keystore_keys_cryptographer);
  if (state_.keystore_keys_cryptographer->IsEmpty()) {
    // TODO(crbug.com/40253261): try to relax this requirement for Nigori
    // initialization as well. Keystore keys might not arrive, for example, due
    // to throttling. It seems easier after complete deprecation of
    // IMPLICIT_PASSPHRASE, where not initialized state will be well
    // distinguished.
    return ModelError(
        FROM_HERE,
        ModelError::Type::kNigoriMissingKeystoreKeysDuringInitialSync);
  }
  // We received uninitialized Nigori and need to initialize it as default
  // keystore Nigori.
  QueuePendingLocalCommit(
      PendingLocalNigoriCommit::ForKeystoreInitialization());
  return std::nullopt;
}

std::optional<ModelError> NigoriSyncBridgeImpl::ApplyIncrementalSyncChanges(
    std::optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data) {
    DCHECK(data->specifics.has_nigori());
    return UpdateLocalState(data->specifics.nigori());
  }

  if (!pending_local_commit_queue_.empty() && !processor_->IsEntityUnsynced()) {
    // Successfully committed first element in queue.
    bool success = pending_local_commit_queue_.front()->TryApply(state_);
    DCHECK(success);
    pending_local_commit_queue_.front()->OnSuccess(state_, observer_list_);
    pending_local_commit_queue_.pop_front();

    // Advance until the next applicable local change if any and call Put().
    PutNextApplicablePendingLocalCommit();
  }

  // Receiving empty `data` means metadata-only change (e.g. no remote updates,
  // or local commit completion), so we need to persist its state.
  storage_->StoreData(SerializeAsNigoriLocalData());
  return std::nullopt;
}

std::optional<ModelError> NigoriSyncBridgeImpl::UpdateLocalState(
    const NigoriSpecifics& specifics) {
  if (!IsValidNigoriSpecifics(specifics)) {
    return ModelError(FROM_HERE, ModelError::Type::kNigoriInvalidSpecifics);
  }

  const sync_pb::NigoriSpecifics::PassphraseType new_passphrase_type =
      ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type());
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);

  if (!IsValidPassphraseTransition(
          /*old_passphrase_type=*/state_.passphrase_type,
          new_passphrase_type)) {
    return ModelError(FROM_HERE,
                      ModelError::Type::kNigoriInvalidPassphraseTransition);
  }
  if (!IsValidEncryptedTypesTransition(state_.encrypt_everything, specifics)) {
    return ModelError(FROM_HERE,
                      ModelError::Type::kNigoriInvalidEncryptedTypesTransition);
  }

  const bool had_pending_keys_before_update = state_.pending_keys.has_value();
  const DataTypeSet encrypted_types_before_update = state_.GetEncryptedTypes();

  state_.encrypt_everything = specifics.encrypt_everything();

  const bool passphrase_type_changed =
      UpdatePassphraseType(new_passphrase_type, &state_.passphrase_type);
  DCHECK_NE(state_.passphrase_type, NigoriSpecifics::UNKNOWN);

  if (specifics.has_custom_passphrase_time()) {
    state_.custom_passphrase_time =
        ProtoTimeToTime(specifics.custom_passphrase_time());
  }
  if (specifics.has_keystore_migration_time()) {
    state_.keystore_migration_time =
        ProtoTimeToTime(specifics.keystore_migration_time());
  }

  state_.trusted_vault_debug_info = specifics.trusted_vault_debug_info();

  if (state_.passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE) {
    state_.custom_passphrase_key_derivation_params =
        GetKeyDerivationParamsFromSpecifics(specifics);
  }

  std::optional<sync_pb::NigoriKey> keystore_decryptor_key;
  if (state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    state_.pending_keystore_decryptor_token =
        specifics.keystore_decryptor_token();
  } else {
    state_.pending_keystore_decryptor_token.reset();
  }

  const NigoriKeyBag decryption_key_bag_for_remote_update =
      BuildDecryptionKeyBagForRemoteKeybag();

  // Set incoming encrypted keys as pending, so they are processed in
  // TryDecryptPendingKeysWith(). If the keybag is not immediately decryptable,
  // it will be kept in `state_.pending_keys` until decryption is possible, e.g.
  // upon SetExplicitPassphraseDecryptionKey() or equivalent depending on the
  // passphrase type.
  state_.pending_keys = specifics.encryption_keybag();
  state_.cryptographer->ClearDefaultEncryptionKey();

  if (specifics.has_cross_user_sharing_public_key()) {
    // Remote update wins over local state.
    state_.cross_user_sharing_key_pair_version.reset();
    state_.cross_user_sharing_public_key =
        PublicKeyFromProto(specifics.cross_user_sharing_public_key());
    if (state_.cross_user_sharing_public_key) {
      state_.cross_user_sharing_key_pair_version =
          specifics.cross_user_sharing_public_key().version();
    }
  }

  std::optional<ModelError> error =
      TryDecryptPendingKeysWith(decryption_key_bag_for_remote_update);
  if (error.has_value()) {
    return error;
  }

  if (passphrase_type_changed) {
    observer_list_.NotifyPassphraseTypeChanged(
        *ProtoPassphraseInt32ToEnum(state_.passphrase_type),
        GetExplicitPassphraseTime());
  }

  if (encrypted_types_before_update != state_.GetEncryptedTypes()) {
    observer_list_.NotifyEncryptedTypesChanged(state_.GetEncryptedTypes(),
                                               state_.encrypt_everything);
  }

  observer_list_.NotifyCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());

  if (!state_.pending_keys.has_value() && had_pending_keys_before_update) {
    // Guaranteed by BuildDecryptionKeyBagForRemoteKeybag() logic.
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);
    // Note that, for users in half-migrated state, OnPassphraseRequired() may
    // have been notified instead of OnKeystoreKeysRequired(). Instead of trying
    // to distinguish the two cases here too, this codepath issues
    // OnKeystoreKeysAccepted() in all cases for simplicity.
    observer_list_.NotifyKeystoreKeysAccepted();
  }

  MaybeNotifyOfPendingKeys();

  // There might be pending local commits, so make attempt to apply them on top
  // of new `state_`.
  PutNextApplicablePendingLocalCommit();

  storage_->StoreData(SerializeAsNigoriLocalData());

  return std::nullopt;
}

NigoriKeyBag NigoriSyncBridgeImpl::BuildDecryptionKeyBagForRemoteKeybag()
    const {
  NigoriKeyBag decryption_key_bag = NigoriKeyBag::CreateEmpty();

  if (state_.pending_keystore_decryptor_token.has_value()) {
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);
    sync_pb::NigoriKey keystore_decryptor_key;
    if (state_.keystore_keys_cryptographer->DecryptKeystoreDecryptorToken(
            *state_.pending_keystore_decryptor_token,
            &keystore_decryptor_key)) {
      // Note: `pending_keystore_decryptor_token` will be cleared upon
      // successful decryption of `pending_keys`.
      decryption_key_bag.AddKeyFromProto(keystore_decryptor_key);
    }
  }

  if (state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    // Allow decryption using keystore keys directly: while using
    // `keystore_decryptor_token` should be sufficient, this supports future
    // case when `keystore_decryptor_token` is not passed.
    decryption_key_bag.AddAllUnknownKeysFrom(
        state_.keystore_keys_cryptographer->GetKeystoreKeybag());
  }

  if (state_.cryptographer->CanEncrypt()) {
    decryption_key_bag.AddKeyFromProto(
        state_.cryptographer->ExportDefaultKey());
  }

  return decryption_key_bag;
}

std::optional<ModelError> NigoriSyncBridgeImpl::TryDecryptPendingKeysWith(
    const NigoriKeyBag& key_bag) {
  DCHECK(state_.pending_keys.has_value());
  DCHECK(state_.cryptographer->GetDefaultEncryptionKeyName().empty());

  std::string decrypted_pending_keys_str;
  if (!key_bag.Decrypt(*state_.pending_keys, &decrypted_pending_keys_str)) {
    return std::nullopt;
  }

  sync_pb::EncryptionKeys decrypted_pending_keys;
  if (!decrypted_pending_keys.ParseFromString(decrypted_pending_keys_str)) {
    return std::nullopt;
  }

  const std::string new_default_key_name = state_.pending_keys->key_name();
  DCHECK(key_bag.HasKey(new_default_key_name));

  NigoriKeyBag new_key_bag = NigoriKeyBag::CreateEmpty();
  for (const sync_pb::NigoriKey& key : decrypted_pending_keys.key()) {
    new_key_bag.AddKeyFromProto(key);
  }

  if (!new_key_bag.HasKey(new_default_key_name)) {
    // Protocol violation.
    return ModelError(FROM_HERE,
                      ModelError::Type::kNigoriMissingNewDefaultKeyInKeybag);
  }

  if (state_.last_default_trusted_vault_key_name.has_value() &&
      !new_key_bag.HasKey(*state_.last_default_trusted_vault_key_name)) {
    // Protocol violation.
    return ModelError(
        FROM_HERE, ModelError::Type::kNigoriMissingLastTrustedVaultKeyInKeybag);
  }

  CrossUserSharingKeys new_cross_user_sharing_keys =
      CrossUserSharingKeys::CreateEmpty();
  for (const sync_pb::CrossUserSharingPrivateKey& key_pair :
       decrypted_pending_keys.cross_user_sharing_private_key()) {
    new_cross_user_sharing_keys.AddKeyPairFromProto(key_pair);
  }

  if (state_.cross_user_sharing_key_pair_version.has_value() &&
      !new_cross_user_sharing_keys.HasKeyPair(
          state_.cross_user_sharing_key_pair_version.value())) {
    DLOG(ERROR) << "Received keybag is missing the last "
                << "cross-user-sharing private key.";
    // Reset keys so that on next startup they would be recreated and
    // committed to the server.
    // TODO(crbug.com/40070237): Clear obsolete key-pairs from cryptographer.
    state_.cross_user_sharing_key_pair_version = std::nullopt;
    state_.cross_user_sharing_public_key = std::nullopt;
  } else if (state_.cross_user_sharing_key_pair_version.has_value()) {
    // Use the keys from the server and replace any pre-existing ones (so in
    // case of conflict the server wins). One of cases when this can happen is
    // when one of older clients is upgraded to a newer version and generated
    // a new key pair because it wasn't aware of the previous key pair.
    state_.cryptographer->ReplaceCrossUserSharingKeys(
        std::move(new_cross_user_sharing_keys));
    state_.cryptographer->SelectDefaultCrossUserSharingKey(
        state_.cross_user_sharing_key_pair_version.value());
  }

  // Reset `last_default_trusted_vault_key_name` as `state_` might go out of
  // TRUSTED_VAULT passphrase type. The callers are responsible to set it again
  // if needed.
  state_.last_default_trusted_vault_key_name = std::nullopt;
  state_.cryptographer->EmplaceKeysFrom(new_key_bag);
  state_.cryptographer->SelectDefaultEncryptionKey(new_default_key_name);
  state_.pending_keys.reset();
  state_.pending_keystore_decryptor_token.reset();

  return std::nullopt;
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetDataForCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<EntityData> entity_data = GetDataImpl(/*is_for_commit=*/true);
  CHECK(IsValidNigoriSpecifics(entity_data->specifics.nigori()));
  return entity_data;
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetDataImpl(/*is_for_commit=*/false);
}

void NigoriSyncBridgeImpl::ApplyDisableSyncChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_->ClearData();
  state_.keystore_keys_cryptographer = KeystoreKeysCryptographer::CreateEmpty();
  state_.cryptographer->ClearAllKeys();
  state_.pending_keys.reset();
  state_.pending_keystore_decryptor_token.reset();
  state_.passphrase_type = NigoriSpecifics::UNKNOWN;
  state_.encrypt_everything = false;
  state_.custom_passphrase_time = base::Time();
  state_.keystore_migration_time = base::Time();
  state_.custom_passphrase_key_derivation_params = std::nullopt;
  state_.last_default_trusted_vault_key_name = std::nullopt;
  state_.trusted_vault_debug_info =
      sync_pb::NigoriSpecifics::TrustedVaultDebugInfo();
  state_.cross_user_sharing_public_key = std::nullopt;
  state_.cross_user_sharing_key_pair_version = std::nullopt;

  observer_list_.NotifyCryptographerStateChanged(state_.cryptographer.get(),
                                                 /*has_pending_keys=*/false);
  observer_list_.NotifyEncryptedTypesChanged(state_.GetEncryptedTypes(), false);
}

const CryptographerImpl& NigoriSyncBridgeImpl::GetCryptographerImplForTesting()
    const {
  return *state_.cryptographer;
}

bool NigoriSyncBridgeImpl::HasPendingKeysForTesting() const {
  return state_.pending_keys.has_value();
}

KeyDerivationParams
NigoriSyncBridgeImpl::GetCustomPassphraseKeyDerivationParamsForTesting() const {
  if (!state_.custom_passphrase_key_derivation_params) {
    return KeyDerivationParams::CreateForPbkdf2();
  }
  return *state_.custom_passphrase_key_derivation_params;
}

base::Time NigoriSyncBridgeImpl::GetExplicitPassphraseTime() const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return base::Time();
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return state_.keystore_migration_time;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return state_.custom_passphrase_time;
  }
  NOTREACHED();
}

KeyDerivationParams NigoriSyncBridgeImpl::GetKeyDerivationParamsForPendingKeys()
    const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      NOTREACHED();
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return KeyDerivationParams::CreateForPbkdf2();
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      DCHECK(state_.custom_passphrase_key_derivation_params);
      return *state_.custom_passphrase_key_derivation_params;
  }
}

void NigoriSyncBridgeImpl::MaybeNotifyOfPendingKeys() const {
  if (!state_.pending_keys.has_value()) {
    return;
  }

  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      return;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      observer_list_.NotifyPassphraseRequired(RequiredPassphraseVerifierImpl(
          GetKeyDerivationParamsForPendingKeys(), *state_.pending_keys));
      break;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      if (state_.pending_keystore_decryptor_token.has_value() &&
          state_.pending_keys->key_name() !=
              state_.pending_keystore_decryptor_token->key_name()) {
        // Half-migrated keystore state: the user may enter the implicit
        // passphrase.
        observer_list_.NotifyPassphraseRequired(RequiredPassphraseVerifierImpl(
            GetKeyDerivationParamsForPendingKeys(), *state_.pending_keys));
      } else {
        observer_list_.NotifyKeystoreKeysRequired();
      }
      break;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      observer_list_.NotifyTrustedVaultKeyRequired();
      break;
  }
}

sync_pb::NigoriLocalData NigoriSyncBridgeImpl::SerializeAsNigoriLocalData()
    const {
  sync_pb::NigoriLocalData output;

  // Serialize the metadata.
  const NigoriMetadataBatch metadata_batch = processor_->GetMetadata();
  *output.mutable_data_type_state() = metadata_batch.data_type_state;
  if (metadata_batch.entity_metadata) {
    *output.mutable_entity_metadata() = *metadata_batch.entity_metadata;
  }

  // Serialize the data.
  *output.mutable_nigori_model() = state_.ToLocalProto();

  return output;
}


void NigoriSyncBridgeImpl::QueuePendingLocalCommit(
    std::unique_ptr<PendingLocalNigoriCommit> local_commit) {
  CHECK(processor_->IsTrackingMetadata());

  pending_local_commit_queue_.push_back(std::move(local_commit));

  if (pending_local_commit_queue_.size() == 1) {
    // Verify that the newly-introduced commit (if first in the queue) applies
    // and if so call Put(), or otherwise issue an immediate failure.
    PutNextApplicablePendingLocalCommit();
  }
}

void NigoriSyncBridgeImpl::PutNextApplicablePendingLocalCommit() {
  while (!pending_local_commit_queue_.empty()) {
    NigoriState tmp_state = state_.Clone();
    bool success = pending_local_commit_queue_.front()->TryApply(tmp_state);
    if (success) {
      // This particular commit applies cleanly.
      processor_->Put(GetDataForCommit());
      break;
    }

    // The local change failed to apply.
    pending_local_commit_queue_.front()->OnFailure(observer_list_);
    pending_local_commit_queue_.pop_front();
  }
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetDataImpl(
    bool is_for_commit) {
  NigoriState state_to_report = state_.Clone();
  if (!pending_local_commit_queue_.empty()) {
    bool success =
        pending_local_commit_queue_.front()->TryApply(state_to_report);
    if (is_for_commit) {
      CHECK(success, base::NotFatalUntil::M149);
    } else if (!success) {
      DLOG(ERROR) << "Failed to apply pending local commit for debugging.";
    }
  }

  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_nigori() = state_to_report.ToSpecificsProto();
  entity_data->name = kNigoriNonUniqueName;
  return entity_data;
}

}  // namespace syncer
