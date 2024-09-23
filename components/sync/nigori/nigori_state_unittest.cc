// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_state.h"

#include <utility>
#include <vector>

#include "components/sync/base/time.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;
using testing::Eq;
using testing::Ne;

TEST(NigoriStateTest, ShouldConvertCustomPassphraseStateToSpecifics) {
  const base::Time now = base::Time::Now();
  const std::string kKey = "key1";

  NigoriState state;
  state.passphrase_type = NigoriSpecifics::CUSTOM_PASSPHRASE;
  state.encrypt_everything = true;
  state.custom_passphrase_time = now;
  state.custom_passphrase_key_derivation_params =
      KeyDerivationParams::CreateForPbkdf2();

  const std::string key_name = state.cryptographer->EmplaceKey(
      kKey, KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(""));

  state.cryptographer->SelectDefaultEncryptionKey(key_name);

  NigoriSpecifics specifics = state.ToSpecificsProto();
  EXPECT_THAT(specifics.passphrase_type(),
              Eq(NigoriSpecifics::CUSTOM_PASSPHRASE));
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_THAT(specifics.encryption_keybag().key_name(), Eq(key_name));
  EXPECT_THAT(specifics.custom_passphrase_key_derivation_method(),
              Eq(NigoriSpecifics::PBKDF2_HMAC_SHA1_1003));
  EXPECT_FALSE(specifics.has_keystore_decryptor_token());
  EXPECT_FALSE(specifics.has_keystore_migration_time());
  EXPECT_THAT(specifics.custom_passphrase_time(), Eq(TimeToProtoTime(now)));
}

TEST(NigoriStateTest, ShouldConvertKeystoreStateToSpecifics) {
  // Note that in practice having a NigoriState with two keystore keys and yet
  // a default encryption key that is neither of them is not realistic. However,
  // it serves this test well to verify that a) which key is used to encrypt the
  // keybag and b) which key is used to encrypt the keystore decryptor token.
  const base::Time now = base::Time::Now();
  const std::string kKeystoreKey1 = "keystorekey1";
  const std::string kKeystoreKey2 = "keystorekey2";
  const std::string kDefaultEncryptionKey = "defaultkey";

  NigoriState state;
  state.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(
          {kKeystoreKey1, kKeystoreKey2});
  state.passphrase_type = NigoriSpecifics::KEYSTORE_PASSPHRASE;
  state.keystore_migration_time = now;
  state.cryptographer = CryptographerImpl::CreateEmpty();
  state.cryptographer->EmplaceKey(kKeystoreKey1,
                                  KeyDerivationParams::CreateForPbkdf2());
  const std::string last_keystore_key_name = state.cryptographer->EmplaceKey(
      kKeystoreKey2, KeyDerivationParams::CreateForPbkdf2());
  const std::string default_encryption_key_name =
      state.cryptographer->EmplaceKey(kDefaultEncryptionKey,
                                      KeyDerivationParams::CreateForPbkdf2());
  state.cryptographer->SelectDefaultEncryptionKey(default_encryption_key_name);

  ASSERT_THAT(last_keystore_key_name, Ne(""));
  ASSERT_THAT(default_encryption_key_name, Ne(""));
  ASSERT_THAT(default_encryption_key_name, Ne(last_keystore_key_name));

  NigoriSpecifics specifics = state.ToSpecificsProto();
  EXPECT_THAT(specifics.passphrase_type(),
              NigoriSpecifics::KEYSTORE_PASSPHRASE);
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_THAT(specifics.encryption_keybag().key_name(),
              Eq(default_encryption_key_name));
  EXPECT_TRUE(specifics.has_keystore_decryptor_token());
  EXPECT_THAT(specifics.keystore_decryptor_token().key_name(),
              Eq(last_keystore_key_name));
  EXPECT_FALSE(specifics.has_custom_passphrase_time());
  EXPECT_FALSE(specifics.has_custom_passphrase_key_derivation_method());
  EXPECT_THAT(specifics.keystore_migration_time(), Eq(TimeToProtoTime(now)));
}

TEST(NigoriStateTest, ShouldConvertPublicKeyStateToSpecifics) {
  const std::string kDefaultEncryptionKey = "defaultkey";
  NigoriState state;
  const std::string default_encryption_key_name =
      state.cryptographer->EmplaceKey(kDefaultEncryptionKey,
                                      KeyDerivationParams::CreateForPbkdf2());
  state.cryptographer->SelectDefaultEncryptionKey(default_encryption_key_name);
  const std::vector<uint8_t> key(32, 0xDE);
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(key);
  state.cross_user_sharing_key_pair_version = 1;

  NigoriSpecifics specifics = state.ToSpecificsProto();

  EXPECT_THAT(specifics.cross_user_sharing_public_key().x25519_public_key(),
              testing::ElementsAreArray(key));
  EXPECT_THAT(specifics.cross_user_sharing_public_key().version(), Eq(1));
}

TEST(NigoriStateTest, ShouldContainPublicKeyInLocalProto) {
  const std::string kDefaultEncryptionKey = "defaultkey";
  const uint32_t kKeyPairVersion = 1;

  NigoriState state;
  const std::string default_encryption_key_name =
      state.cryptographer->EmplaceKey(kDefaultEncryptionKey,
                                      KeyDerivationParams::CreateForPbkdf2());
  state.cryptographer->SelectDefaultEncryptionKey(default_encryption_key_name);
  state.cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
      kKeyPairVersion);
  const std::vector<uint8_t> key(32, 0xDE);
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(key);
  state.cross_user_sharing_key_pair_version = kKeyPairVersion;

  sync_pb::NigoriModel nigori_model = state.ToLocalProto();

  ASSERT_THAT(nigori_model.cross_user_sharing_public_key().x25519_public_key(),
              testing::ElementsAreArray(key));
  ASSERT_THAT(nigori_model.cross_user_sharing_public_key().version(),
              Eq(kKeyPairVersion));
}

TEST(NigoriStateTest, ShouldClonePublicKey) {
  NigoriState state;
  const std::vector<uint8_t> key(32, 0xDE);
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(key);
  state.cross_user_sharing_key_pair_version = 1;

  NigoriState cloned_state = state.Clone();

  EXPECT_THAT(cloned_state.cross_user_sharing_public_key->GetRawPublicKey(),
              testing::ElementsAreArray(
                  state.cross_user_sharing_public_key->GetRawPublicKey()));
  EXPECT_EQ(cloned_state.cross_user_sharing_key_pair_version,
            state.cross_user_sharing_key_pair_version);
}

TEST(
    NigoriStateTest,
    ShouldSetCrossUserPublicKeyVersionInCreateFromLocalProtoAndBeAbleToEncrypt) {
  sync_pb::NigoriModel nigori_model;
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 5);
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 6);
  ASSERT_TRUE(cryptographer->HasKeyPair(5));
  ASSERT_TRUE(cryptographer->HasKeyPair(6));
  *nigori_model.mutable_cryptographer_data() = cryptographer->ToProto();

  const auto raw_public_key =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/5)
          .GetRawPublicKey();
  nigori_model.mutable_cross_user_sharing_public_key()->set_x25519_public_key(
      std::string(raw_public_key.begin(), raw_public_key.end()));
  nigori_model.mutable_cross_user_sharing_public_key()->set_version(5);

  NigoriState state = NigoriState::CreateFromLocalProto(nigori_model);

  std::optional<std::vector<uint8_t>> encrypted_message =
      state.cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span("should encrypt this message")),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_TRUE(encrypted_message.has_value());
}

TEST(NigoriStateTest,
     ShouldReturnEmptyOnEncryptIfCrossUserPublicKeyVersionIsNotSet) {
  sync_pb::NigoriModel nigori_model;
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 5);
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 6);
  ASSERT_TRUE(cryptographer->HasKeyPair(5));
  ASSERT_TRUE(cryptographer->HasKeyPair(6));
  *nigori_model.mutable_cryptographer_data() = cryptographer->ToProto();

  NigoriState state = NigoriState::CreateFromLocalProto(nigori_model);

  std::optional<std::vector<uint8_t>> encrypted_message =
      state.cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span("should encrypt this message")),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_FALSE(encrypted_message.has_value());
}

TEST(NigoriStateTest, ShouldReturnNeedsGenerateCrossUserSharingKeyPair) {
  NigoriState state;
  const std::string key_name = state.cryptographer->EmplaceKey(
      "key1", KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(""));
  state.cryptographer->SelectDefaultEncryptionKey(key_name);

  // There is no public key, so the key pair needs to be generated.
  EXPECT_TRUE(state.NeedsGenerateCrossUserSharingKeyPair());

  // Initialize the correct key pair.
  CrossUserSharingPublicPrivateKeyPair key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(key_pair.GetRawPublicKey());
  state.cross_user_sharing_key_pair_version = 0;
  state.cryptographer->SetKeyPair(std::move(key_pair), 0);
  EXPECT_FALSE(state.NeedsGenerateCrossUserSharingKeyPair());

  // Corrupt the key pair.
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());
  EXPECT_TRUE(state.NeedsGenerateCrossUserSharingKeyPair());

  // Set pending keys which should prevent generating key pair.
  state.pending_keys = sync_pb::EncryptedData();
  EXPECT_FALSE(state.NeedsGenerateCrossUserSharingKeyPair());
}

TEST(NigoriStateTest,
     ShouldReturnNeedsGenerateCrossUserSharingKeyPairWhenInvalid) {
  NigoriState state;
  const std::string key_name = state.cryptographer->EmplaceKey(
      "key1", KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(""));
  state.cryptographer->SelectDefaultEncryptionKey(key_name);

  // There is no public key, so the key pair needs to be generated.
  ASSERT_TRUE(state.NeedsGenerateCrossUserSharingKeyPair());

  // Initialize only public key and keep the private key missing.
  CrossUserSharingPublicPrivateKeyPair key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  state.cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(key_pair.GetRawPublicKey());
  state.cross_user_sharing_key_pair_version = 0;
  EXPECT_TRUE(state.NeedsGenerateCrossUserSharingKeyPair());
}

}  // namespace

}  // namespace syncer
