// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/nigori_test_utils.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cross_user_sharing_keys.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

sync_pb::CrossUserSharingPublicKey PublicKeyToProto(
    const syncer::CrossUserSharingPublicKey& public_key,
    uint32_t key_pair_version) {
  sync_pb::CrossUserSharingPublicKey output;
  const auto key = public_key.GetRawPublicKey();
  output.set_x25519_public_key(std::string(key.begin(), key.end()));
  output.set_version(key_pair_version);
  return output;
}

}  // namespace
namespace syncer {

KeyParamsForTesting KeystoreKeyParamsForTesting(
    const std::vector<uint8_t>& raw_key) {
  return Pbkdf2PassphraseKeyParamsForTesting(base::Base64Encode(raw_key));
}

KeyParamsForTesting TrustedVaultKeyParamsForTesting(
    const std::vector<uint8_t>& raw_key) {
  return Pbkdf2PassphraseKeyParamsForTesting(base::Base64Encode(raw_key));
}

KeyParamsForTesting Pbkdf2PassphraseKeyParamsForTesting(
    const std::string& passphrase) {
  return {KeyDerivationParams::CreateForPbkdf2(), passphrase};
}

KeyParamsForTesting ScryptPassphraseKeyParamsForTesting(
    const std::string& passphrase) {
  return {KeyDerivationParams::CreateForScrypt(passphrase), passphrase};
}

sync_pb::CrossUserSharingPublicKey CrossUserSharingKeysToPublicKeyProto(
    const CrossUserSharingKeys& cross_user_sharing_keys,
    size_t key_version) {
  sync_pb::CrossUserSharingPublicKey public_key;
  auto raw_public_key =
      cross_user_sharing_keys.GetKeyPair(key_version).GetRawPublicKey();
  public_key.set_x25519_public_key(
      std::string(raw_public_key.begin(), raw_public_key.end()));
  public_key.set_version(key_version);
  return public_key;
}

sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params,
    const CrossUserSharingKeys& cross_user_sharing_keys) {
  DCHECK(!keybag_keys_params.empty());

  sync_pb::NigoriSpecifics specifics;

  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          keystore_decryptor_params.password,
          keystore_decryptor_params.derivation_params);

  NigoriKeyBag encryption_keybag = NigoriKeyBag::CreateEmpty();
  for (const KeyParamsForTesting& key_params : keybag_keys_params) {
    encryption_keybag.AddKey(Nigori::CreateByDerivation(
        key_params.derivation_params, key_params.password));
  }

  sync_pb::EncryptionKeys keys_for_encryption;

  keys_for_encryption.mutable_key()->CopyFrom(
      encryption_keybag.ToProto().key());
  keys_for_encryption.mutable_cross_user_sharing_private_key()->CopyFrom(
      cross_user_sharing_keys.ToProto().private_key());

  EXPECT_TRUE(cryptographer->Encrypt(keys_for_encryption,
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<CryptographerImpl> keystore_cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          keystore_key_params.password, keystore_key_params.derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  if (cross_user_sharing_keys.HasKeyPair(/*key_version=*/0)) {
    specifics.mutable_cross_user_sharing_public_key()->CopyFrom(
        CrossUserSharingKeysToPublicKeyProto(cross_user_sharing_keys,
                                             /*key_version=*/0));
  }

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(TimeToProtoTime(base::Time::Now()));
  return specifics;
}

sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecificsWithCrossUserSharingKeys(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params,
    const CrossUserSharingKeys& cross_user_sharing_keys,
    const CrossUserSharingPublicKey& cross_user_sharing_public_key,
    const uint32_t cross_user_sharing_public_key_version) {
  sync_pb::NigoriSpecifics specifics = BuildKeystoreNigoriSpecifics(
      keybag_keys_params, keystore_decryptor_params, keystore_key_params,
      cross_user_sharing_keys);
  *specifics.mutable_cross_user_sharing_public_key() = PublicKeyToProto(
      cross_user_sharing_public_key, cross_user_sharing_public_key_version);
  return specifics;
}

sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    base::Time migration_time) {
  sync_pb::NigoriSpecifics specifics;
  specifics.set_passphrase_type(
      sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE);
  specifics.set_keybag_is_frozen(true);

  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::CreateEmpty();
  for (const std::vector<uint8_t>& trusted_vault_key : trusted_vault_keys) {
    const std::string key_name = cryptographer->EmplaceKey(
        base::Base64Encode(trusted_vault_key),
        syncer::KeyDerivationParams::CreateForPbkdf2());
    cryptographer->SelectDefaultEncryptionKey(key_name);
  }

  specifics.mutable_trusted_vault_debug_info()->set_migration_time(
      TimeToProtoTime(migration_time));
  specifics.mutable_trusted_vault_debug_info()->set_key_version(2);

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  return specifics;
}

sync_pb::NigoriSpecifics BuildCustomPassphraseNigoriSpecifics(
    const KeyParamsForTesting& passphrase_key_params,
    const std::optional<KeyParamsForTesting>& old_key_params) {
  KeyDerivationMethod method = passphrase_key_params.derivation_params.method();

  sync_pb::NigoriSpecifics nigori;
  nigori.set_keybag_is_frozen(true);
  nigori.set_keystore_migration_time(1U);
  nigori.set_custom_passphrase_time(2U);
  nigori.set_encrypt_everything(true);
  nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  nigori.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(method));

  std::string encoded_salt;
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      // Nothing to do; no further information needs to be extracted from
      // Nigori.
      break;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      encoded_salt = base::Base64Encode(
          passphrase_key_params.derivation_params.scrypt_salt());
      nigori.set_custom_passphrase_key_derivation_salt(encoded_salt);
      break;
  }

  // Create the cryptographer, which encrypts with the key derived from
  // |passphrase_key_params| and can decrypt with the key derived from
  // |old_key_params| if given. |encryption_keybag| is a serialized version
  // of this cryptographer |key_bag| encrypted with its encryption key.
  auto cryptographer = CryptographerImpl::FromSingleKeyForTesting(
      passphrase_key_params.password, passphrase_key_params.derivation_params);
  if (old_key_params) {
    cryptographer->EmplaceKey(old_key_params->password,
                              old_key_params->derivation_params);
  }
  sync_pb::CryptographerData proto = cryptographer->ToProto();
  bool encrypt_result = cryptographer->Encrypt(
      proto.key_bag(), nigori.mutable_encryption_keybag());
  DCHECK(encrypt_result);

  return nigori;
}

KeyDerivationParams InitCustomPassphraseKeyDerivationParamsFromNigori(
    const sync_pb::NigoriSpecifics& nigori) {
  std::optional<KeyDerivationMethod> key_derivation_method =
      ProtoKeyDerivationMethodToEnum(
          nigori.custom_passphrase_key_derivation_method());
  if (!key_derivation_method.has_value()) {
    // The test cannot pass since we wouldn't know how to decrypt data encrypted
    // using an unsupported method.
    ADD_FAILURE() << "Unsupported key derivation method encountered: "
                  << nigori.custom_passphrase_key_derivation_method();
    return KeyDerivationParams::CreateForPbkdf2();
  }

  switch (*key_derivation_method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      std::string decoded_salt;
      EXPECT_TRUE(base::Base64Decode(
          nigori.custom_passphrase_key_derivation_salt(), &decoded_salt));
      return KeyDerivationParams::CreateForScrypt(decoded_salt);
  }
}

std::unique_ptr<Cryptographer> InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase) {
  KeyDerivationParams key_derivation_params =
      InitCustomPassphraseKeyDerivationParamsFromNigori(nigori);
  auto cryptographer = CryptographerImpl::FromSingleKeyForTesting(
      passphrase, key_derivation_params);

  std::string decrypted_keys_str;
  EXPECT_TRUE(cryptographer->DecryptToString(nigori.encryption_keybag(),
                                             &decrypted_keys_str));

  sync_pb::EncryptionKeys decrypted_keys;
  EXPECT_TRUE(decrypted_keys.ParseFromString(decrypted_keys_str));

  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  base::ranges::for_each(
      decrypted_keys.key(),
      [&key_bag](sync_pb::NigoriKey key) { key_bag.AddKeyFromProto(key); });

  cryptographer->EmplaceKeysFrom(key_bag);
  return cryptographer;
}

sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& bookmark_specifics,
    const KeyParamsForTesting& key_params) {
  sync_pb::EntitySpecifics new_specifics;

  sync_pb::EntitySpecifics wrapped_entity_specifics;
  *wrapped_entity_specifics.mutable_bookmark() = bookmark_specifics;
  auto cryptographer = CryptographerImpl::FromSingleKeyForTesting(
      key_params.password, key_params.derivation_params);

  bool encrypt_result = cryptographer->Encrypt(
      wrapped_entity_specifics, new_specifics.mutable_encrypted());
  DCHECK(encrypt_result);

  new_specifics.mutable_bookmark()->set_legacy_canonicalized_title("encrypted");
  new_specifics.mutable_bookmark()->set_url("encrypted");

  return new_specifics;
}

}  // namespace syncer
