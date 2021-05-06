// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_test_utils.h"

#include "base/base64.h"
#include "components/sync/base/time.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

KeyParamsForTesting Pbkdf2KeyParamsForTesting(
    const std::vector<uint8_t>& raw_key) {
  return {KeyDerivationParams::CreateForPbkdf2(), base::Base64Encode(raw_key)};
}

sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params) {
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

  EXPECT_TRUE(cryptographer->Encrypt(encryption_keybag.ToProto(),
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<CryptographerImpl> keystore_cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          keystore_key_params.password, keystore_key_params.derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(TimeToProtoTime(base::Time::Now()));
  return specifics;
}

sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys) {
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

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  return specifics;
}

sync_pb::NigoriSpecifics CreateCustomPassphraseNigori(
    const KeyParamsForTesting& passphrase_key_params,
    const base::Optional<KeyParamsForTesting>& old_key_params) {
  KeyDerivationMethod method = passphrase_key_params.derivation_params.method();

  sync_pb::NigoriSpecifics nigori;
  nigori.set_keybag_is_frozen(true);
  nigori.set_keystore_migration_time(1U);
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
      base::Base64Encode(passphrase_key_params.derivation_params.scrypt_salt(),
                         &encoded_salt);
      nigori.set_custom_passphrase_key_derivation_salt(encoded_salt);
      break;
    case KeyDerivationMethod::UNSUPPORTED:
      ADD_FAILURE() << "Unsupported method in KeyParamsForTesting, cannot "
                       "construct Nigori.";
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

std::unique_ptr<Cryptographer> InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase) {
  std::unique_ptr<CryptographerImpl> cryptographer;
  sync_pb::EncryptedData keybag = nigori.encryption_keybag();

  std::string decoded_salt;
  switch (ProtoKeyDerivationMethodToEnum(
      nigori.custom_passphrase_key_derivation_method())) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      cryptographer = CryptographerImpl::FromSingleKeyForTesting(passphrase);
      break;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      EXPECT_TRUE(base::Base64Decode(
          nigori.custom_passphrase_key_derivation_salt(), &decoded_salt));
      cryptographer = CryptographerImpl::FromSingleKeyForTesting(
          passphrase, KeyDerivationParams::CreateForScrypt(decoded_salt));
      break;
    case KeyDerivationMethod::UNSUPPORTED:
      // This test cannot pass since we wouldn't know how to decrypt data
      // encrypted using an unsupported method.
      ADD_FAILURE() << "Unsupported key derivation method encountered: "
                    << nigori.custom_passphrase_key_derivation_method();
      return CryptographerImpl::CreateEmpty();
  }

  std::string decrypted_keys_str;
  EXPECT_TRUE(cryptographer->DecryptToString(nigori.encryption_keybag(),
                                             &decrypted_keys_str));

  sync_pb::NigoriKeyBag decrypted_keys;
  EXPECT_TRUE(decrypted_keys.ParseFromString(decrypted_keys_str));

  NigoriKeyBag key_bag = NigoriKeyBag::CreateFromProto(decrypted_keys);

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
