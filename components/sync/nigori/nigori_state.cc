// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_state.h"

#include <cstdint>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

// When enabled, if the local state does not contain the private key for the
// current version, the key pair will be removed and re-generated.
BASE_FEATURE(kSyncDropCrossUserKeyPairIfPrivateKeyDoesNotExist,
             "SyncDropCrossUserKeyPairIfPrivateKeyDoesNotExist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossUserSharingKeyPairState)
enum class CrossUserSharingKeyPairState {
  kValidKeyPair = 0,
  kPublicKeyNotInitialized = 1,
  kPublicKeyVersionInvalid = 2,
  kCorruptedKeyPair = 3,

  // The key pair can't be checked while pending keys are not decrypted.
  kPendingKeysNotEmpty = 4,

  kMaxValue = kPendingKeysNotEmpty,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:CrossUserSharingKeyPairState)

sync_pb::CustomPassphraseKeyDerivationParams
CustomPassphraseKeyDerivationParamsToProto(const KeyDerivationParams& params) {
  sync_pb::CustomPassphraseKeyDerivationParams output;
  output.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    output.set_custom_passphrase_key_derivation_salt(params.scrypt_salt());
  }
  return output;
}

KeyDerivationParams CustomPassphraseKeyDerivationParamsFromProto(
    const sync_pb::CustomPassphraseKeyDerivationParams& proto) {
  switch (proto.custom_passphrase_key_derivation_method()) {
    case sync_pb::NigoriSpecifics::UNSPECIFIED:
      [[fallthrough]];
    case sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case sync_pb::NigoriSpecifics::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          proto.custom_passphrase_key_derivation_salt());
  }

  NOTREACHED_IN_MIGRATION();
  return KeyDerivationParams::CreateForPbkdf2();
}

// |encrypted| must not be null.
bool EncryptEncryptionKeys(const CryptographerImpl& cryptographer,
                           sync_pb::EncryptedData* encrypted) {
  DCHECK(encrypted);
  DCHECK(cryptographer.CanEncrypt());

  sync_pb::CryptographerData proto = cryptographer.ToProto();
  DCHECK(!proto.key_bag().key().empty());

  sync_pb::EncryptionKeys keys_for_encryption;

  keys_for_encryption.mutable_key()->CopyFrom(proto.key_bag().key());
  keys_for_encryption.mutable_cross_user_sharing_private_key()->CopyFrom(
      proto.cross_user_sharing_keys().private_key());

  // Encrypt the bag with the default Nigori.
  return cryptographer.Encrypt(keys_for_encryption, encrypted);
}

void UpdateSpecificsFromKeyDerivationParams(
    const KeyDerivationParams& params,
    sync_pb::NigoriSpecifics* specifics) {
  DCHECK_EQ(specifics->passphrase_type(),
            sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  specifics->set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    // Persist the salt used for key derivation in Nigori if we're using scrypt.
    std::string encoded_salt = base::Base64Encode(params.scrypt_salt());
    specifics->set_custom_passphrase_key_derivation_salt(encoded_salt);
  }
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

sync_pb::CrossUserSharingPublicKey PublicKeyToProto(
    const CrossUserSharingPublicKey& public_key,
    uint32_t key_pair_version) {
  sync_pb::CrossUserSharingPublicKey output;
  const auto key = public_key.GetRawPublicKey();
  output.set_x25519_public_key(std::string(key.begin(), key.end()));
  output.set_version(key_pair_version);
  return output;
}

CrossUserSharingKeyPairState GetCrossUserSharingPublicKeyState(
    const NigoriState& state) {
  if (state.pending_keys) {
    return CrossUserSharingKeyPairState::kPendingKeysNotEmpty;
  }

  if (!state.cross_user_sharing_public_key.has_value()) {
    return CrossUserSharingKeyPairState::kPublicKeyNotInitialized;
  }

  // Key version existence is guaranteed by NigoriState::CreateFromLocalProto().
  CHECK(state.cross_user_sharing_key_pair_version);

  if (!state.cryptographer->HasKeyPair(
          state.cross_user_sharing_key_pair_version.value())) {
    return CrossUserSharingKeyPairState::kPublicKeyVersionInvalid;
  }

  const CrossUserSharingPublicPrivateKeyPair& key_pair =
      state.cryptographer->GetCrossUserSharingKeyPair(
          state.cross_user_sharing_key_pair_version.value());
  if (key_pair.GetRawPublicKey() !=
      state.cross_user_sharing_public_key->GetRawPublicKey()) {
    return CrossUserSharingKeyPairState::kCorruptedKeyPair;
  }
  return CrossUserSharingKeyPairState::kValidKeyPair;
}

}  // namespace

// static
NigoriState NigoriState::CreateFromLocalProto(
    const sync_pb::NigoriModel& proto) {
  NigoriState state;

  state.cryptographer =
      CryptographerImpl::FromProto(proto.cryptographer_data());

  if (proto.has_pending_keys()) {
    state.pending_keys = proto.pending_keys();
  }

  state.passphrase_type = proto.passphrase_type();
  state.keystore_migration_time =
      ProtoTimeToTime(proto.keystore_migration_time());
  state.custom_passphrase_time =
      ProtoTimeToTime(proto.custom_passphrase_time());
  if (proto.has_custom_passphrase_key_derivation_params()) {
    state.custom_passphrase_key_derivation_params =
        CustomPassphraseKeyDerivationParamsFromProto(
            proto.custom_passphrase_key_derivation_params());
  }
  state.encrypt_everything = proto.encrypt_everything();

  std::vector<std::string> keystore_keys;
  for (const std::string& keystore_key : proto.keystore_key()) {
    keystore_keys.push_back(keystore_key);
  }
  state.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(keystore_keys);
  if (!state.keystore_keys_cryptographer) {
    // Crypto error occurs, create empty |keystore_keys_cryptographer|.
    // Effectively it resets keystore keys.
    state.keystore_keys_cryptographer =
        KeystoreKeysCryptographer::CreateEmpty();
  }

  if (proto.has_pending_keystore_decryptor_token()) {
    state.pending_keystore_decryptor_token =
        proto.pending_keystore_decryptor_token();
  }

  if (proto.has_last_default_trusted_vault_key_name()) {
    state.last_default_trusted_vault_key_name =
        proto.last_default_trusted_vault_key_name();
  }

  state.trusted_vault_debug_info = proto.trusted_vault_debug_info();

  if (proto.has_cross_user_sharing_public_key()) {
    state.cross_user_sharing_public_key =
        PublicKeyFromProto(proto.cross_user_sharing_public_key());
    if (state.cross_user_sharing_public_key) {
      state.cross_user_sharing_key_pair_version =
          proto.cross_user_sharing_public_key().version();
      state.cryptographer->SelectDefaultCrossUserSharingKey(
          proto.cross_user_sharing_public_key().version());
    }
  }

  const CrossUserSharingKeyPairState key_pair_state =
      GetCrossUserSharingPublicKeyState(state);
  base::UmaHistogramEnumeration("Sync.CrossUserSharingKeyPairState",
                                key_pair_state);
  if (key_pair_state ==
      CrossUserSharingKeyPairState::kPublicKeyVersionInvalid) {
    // Currently, only version 0 is supported and hence expected.
    base::UmaHistogramBoolean(
        "Sync.CrossUserSharingInvalidKeyVersion.ExpectedVersion",
        state.cross_user_sharing_key_pair_version == 0u);
    base::UmaHistogramBoolean(
        "Sync.CrossUserSharingInvalidKeyVersion.EmptyKeyPair",
        state.cryptographer->KeyPairSizeForMetrics() == 0u);
  }

  return state;
}

NigoriState::NigoriState()
    : cryptographer(CryptographerImpl::CreateEmpty()),
      passphrase_type(kInitialPassphraseType),
      encrypt_everything(kInitialEncryptEverything),
      keystore_keys_cryptographer(KeystoreKeysCryptographer::CreateEmpty()) {}

NigoriState::NigoriState(NigoriState&& other) = default;

NigoriState::~NigoriState() = default;

NigoriState& NigoriState::operator=(NigoriState&& other) = default;

sync_pb::NigoriModel NigoriState::ToLocalProto() const {
  sync_pb::NigoriModel proto;
  *proto.mutable_cryptographer_data() = cryptographer->ToProto();
  if (pending_keys.has_value()) {
    *proto.mutable_pending_keys() = *pending_keys;
  }
  if (!keystore_keys_cryptographer->IsEmpty()) {
    proto.set_current_keystore_key_name(
        keystore_keys_cryptographer->GetLastKeystoreKeyName());
  }
  proto.set_passphrase_type(passphrase_type);
  if (!keystore_migration_time.is_null()) {
    proto.set_keystore_migration_time(TimeToProtoTime(keystore_migration_time));
  }
  if (!custom_passphrase_time.is_null()) {
    proto.set_custom_passphrase_time(TimeToProtoTime(custom_passphrase_time));
  }
  if (custom_passphrase_key_derivation_params) {
    *proto.mutable_custom_passphrase_key_derivation_params() =
        CustomPassphraseKeyDerivationParamsToProto(
            *custom_passphrase_key_derivation_params);
  }
  proto.set_encrypt_everything(encrypt_everything);
  DataTypeSet encrypted_types = AlwaysEncryptedUserTypes();
  if (encrypt_everything) {
    encrypted_types = EncryptableUserTypes();
  }
  for (DataType data_type : encrypted_types) {
    proto.add_encrypted_types_specifics_field_number(
        GetSpecificsFieldNumberFromDataType(data_type));
  }
  // TODO(crbug.com/41462727): we currently store keystore keys in proto only to
  // allow rollback of USS Nigori. Having keybag with all keystore keys and
  // |current_keystore_key_name| is enough to support all logic. We should
  // remove them few milestones after USS migration completed.
  for (const std::string& keystore_key :
       keystore_keys_cryptographer->keystore_keys()) {
    proto.add_keystore_key(keystore_key);
  }
  if (pending_keystore_decryptor_token.has_value()) {
    *proto.mutable_pending_keystore_decryptor_token() =
        *pending_keystore_decryptor_token;
  }
  if (last_default_trusted_vault_key_name.has_value()) {
    proto.set_last_default_trusted_vault_key_name(
        *last_default_trusted_vault_key_name);
  }
  *proto.mutable_trusted_vault_debug_info() = trusted_vault_debug_info;
  if (cross_user_sharing_public_key.has_value() &&
      cross_user_sharing_key_pair_version.has_value()) {
    *proto.mutable_cross_user_sharing_public_key() =
        PublicKeyToProto(cross_user_sharing_public_key.value(),
                         cross_user_sharing_key_pair_version.value());
  }
  return proto;
}

sync_pb::NigoriSpecifics NigoriState::ToSpecificsProto() const {
  sync_pb::NigoriSpecifics specifics;
  if (cryptographer->CanEncrypt()) {
    EncryptEncryptionKeys(*cryptographer,
                          specifics.mutable_encryption_keybag());
  } else if (pending_keys.has_value()) {
    // This case is reachable only from bridge's GetDataForDebugging(),
    // since currently commit is never issued while bridge has |pending_keys_|.
    // Note: with complete support of TRUSTED_VAULT mode, commit might be
    // issued in this case as well.
    *specifics.mutable_encryption_keybag() = *pending_keys;
  } else {
    // This case is reachable only from bridge's GetDataForDebugging(), and
    // indicates that the client received empty NigoriSpecifics and unable to
    // initialize them (e.g. there are no keystore keys).
    return specifics;
  }

  specifics.set_keybag_is_frozen(true);
  specifics.set_encrypt_everything(encrypt_everything);
  specifics.set_passphrase_type(passphrase_type);
  if (passphrase_type == sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE) {
    DCHECK(custom_passphrase_key_derivation_params);
    UpdateSpecificsFromKeyDerivationParams(
        *custom_passphrase_key_derivation_params, &specifics);
  }
  if (passphrase_type == sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    if (pending_keystore_decryptor_token.has_value()) {
      DCHECK(pending_keys.has_value());
      *specifics.mutable_keystore_decryptor_token() =
          *pending_keystore_decryptor_token;
    } else {
      // TODO(crbug.com/40868132): ensure correct error handling, e.g. in case
      // of empty |keystore_keys_cryptographer| or crypto errors (should be
      // impossible, but code doesn't yet guarantee that).
      keystore_keys_cryptographer->EncryptKeystoreDecryptorToken(
          cryptographer->ExportDefaultKey(),
          specifics.mutable_keystore_decryptor_token());
    }
  }
  if (!keystore_migration_time.is_null()) {
    specifics.set_keystore_migration_time(
        TimeToProtoTime(keystore_migration_time));
  }
  if (!custom_passphrase_time.is_null()) {
    specifics.set_custom_passphrase_time(
        TimeToProtoTime(custom_passphrase_time));
  }
  *specifics.mutable_trusted_vault_debug_info() = trusted_vault_debug_info;

  if (cross_user_sharing_public_key.has_value() &&
      cross_user_sharing_key_pair_version.has_value()) {
    *specifics.mutable_cross_user_sharing_public_key() =
        PublicKeyToProto(cross_user_sharing_public_key.value(),
                         cross_user_sharing_key_pair_version.value());
  }

  return specifics;
}

NigoriState NigoriState::Clone() const {
  NigoriState result;
  result.cryptographer = cryptographer->Clone();
  result.pending_keys = pending_keys;
  result.passphrase_type = passphrase_type;
  result.keystore_migration_time = keystore_migration_time;
  result.custom_passphrase_time = custom_passphrase_time;
  result.custom_passphrase_key_derivation_params =
      custom_passphrase_key_derivation_params;
  result.encrypt_everything = encrypt_everything;
  result.keystore_keys_cryptographer = keystore_keys_cryptographer->Clone();
  result.pending_keystore_decryptor_token = pending_keystore_decryptor_token;
  result.last_default_trusted_vault_key_name =
      last_default_trusted_vault_key_name;
  result.trusted_vault_debug_info = trusted_vault_debug_info;
  if (cross_user_sharing_public_key.has_value()) {
    result.cross_user_sharing_public_key =
        cross_user_sharing_public_key->Clone();
  }
  result.cross_user_sharing_key_pair_version =
      cross_user_sharing_key_pair_version;
  return result;
}

bool NigoriState::NeedsKeystoreReencryption() const {
  if (keystore_keys_cryptographer->IsEmpty() ||
      passphrase_type != sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE ||
      pending_keys.has_value() ||
      cryptographer->GetDefaultEncryptionKeyName() ==
          keystore_keys_cryptographer->GetLastKeystoreKeyName()) {
    return false;
  }
  // Either keystore key rotation or full keystore migration should be
  // triggered, since default encryption key is not the last keystore key, while
  // it should be.
  return true;
}

DataTypeSet NigoriState::GetEncryptedTypes() const {
  if (!encrypt_everything) {
    return AlwaysEncryptedUserTypes();
  }

  return EncryptableUserTypes();
}

bool NigoriState::NeedsGenerateCrossUserSharingKeyPair() const {
  if (pending_keys || !cryptographer->CanEncrypt()) {
    // There are pending keys so the current state of the key pair is unknown,
    // or cryptographer is not ready yet (this should not happen but not using
    // CHECK because it's difficult to guarantee here).
    return false;
  }

  // Generate a new key pair if there is no public key in the local state. Note
  // that this can trigger a key pair generation if the current client has been
  // just upgraded from the older version (so it wasn't aware of key pairs).
  // Other clients are expected to apply the newly generated key pair.
  if (!cross_user_sharing_public_key.has_value()) {
    return true;
  }

  CrossUserSharingKeyPairState key_pair_state =
      GetCrossUserSharingPublicKeyState(*this);
  switch (key_pair_state) {
    case CrossUserSharingKeyPairState::kValidKeyPair:
    case CrossUserSharingKeyPairState::kPendingKeysNotEmpty:
      return false;
    case CrossUserSharingKeyPairState::kPublicKeyNotInitialized:
    case CrossUserSharingKeyPairState::kCorruptedKeyPair:
      // The public key doesn't match the private key. Generate a new key pair
      // and commit it to the server. Other clients are expected to apply the
      // new state. This behavior is similar to a client has just been upgraded.
      // This code also covers the case when the key pair is corrupted on the
      // server. In this case after browser restart the current client will
      // generate a new key pair.
      return true;
    case CrossUserSharingKeyPairState::kPublicKeyVersionInvalid:
      // Similar to `kCorruptedKeyPair` but when the private key does not exist
      // for the current public key version.
      return base::FeatureList::IsEnabled(
          kSyncDropCrossUserKeyPairIfPrivateKeyDoesNotExist);
  }
}

}  // namespace syncer
