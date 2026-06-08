// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/nigori/key_derivation_params.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::UnorderedElementsAre;

MATCHER_P(HasKeyName, name, "") {
  return arg && arg->GetKeyName() == name;
}

}  // namespace

TEST(CryptographerImplTest, ShouldCreateEmpty) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  EXPECT_FALSE(cryptographer->CanEncrypt());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer->CanDecrypt(encrypted));

  std::string output;
  EXPECT_FALSE(cryptographer->DecryptToString(encrypted, &output));
}

TEST(CryptographerImplTest, ShouldEmplaceKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->CanEncrypt());

  const std::string key_name = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  EXPECT_THAT(key_name, Ne(std::string()));

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(key_name);
  encrypted.set_blob("fakeblob");

  EXPECT_TRUE(cryptographer->CanDecrypt(encrypted));
  EXPECT_FALSE(cryptographer->CanEncrypt());
}

TEST(CryptographerImplTest, ShouldEmplaceExistingKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  const std::string key_name = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(std::string()));
  EXPECT_THAT(cryptographer->EmplaceKey("password1",
                                        KeyDerivationParams::CreateForPbkdf2()),
              Eq(key_name));
}

TEST(CryptographerImplTest, ShouldEmplaceSecondKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  const std::string key_name1 = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  const std::string key_name2 = cryptographer->EmplaceKey(
      "password2", KeyDerivationParams::CreateForPbkdf2());

  EXPECT_THAT(key_name1, Ne(std::string()));
  EXPECT_THAT(key_name2, Ne(std::string()));
  EXPECT_THAT(key_name1, Ne(key_name2));
}

TEST(CryptographerImplTest, ShouldSelectDefaultEncryptionKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->CanEncrypt());

  const std::string key_name = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(std::string()));

  cryptographer->SelectDefaultEncryptionKey(key_name);
  ASSERT_TRUE(cryptographer->CanEncrypt());

  sync_pb::EncryptedData encrypted;
  EXPECT_TRUE(cryptographer->EncryptString("foo", &encrypted));
  EXPECT_THAT(encrypted.key_name(), Eq(key_name));
}

TEST(CryptographerImplTest, ShouldSelectDefaultCrossUserSharingKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);
  cryptographer->SelectDefaultCrossUserSharingKey(4);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_byte_span(plaintext),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_TRUE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldFailOnNonSetEncryptionKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_byte_span(plaintext),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_FALSE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldFailOnNonExistentDefaultEncryptionKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);
  cryptographer->SelectDefaultCrossUserSharingKey(3);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_byte_span(plaintext),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_FALSE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldSerializeToAndFromProto) {
  const std::string kText1 = "foo";
  const std::string kText2 = "bar";

  std::unique_ptr<CryptographerImpl> original_cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(original_cryptographer, NotNull());

  const std::string key_name1 = original_cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  const std::string key_name2 = original_cryptographer->EmplaceKey(
      "password2", KeyDerivationParams::CreateForPbkdf2());
  original_cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);

  original_cryptographer->SelectDefaultEncryptionKey(key_name1);
  sync_pb::EncryptedData encrypted1;
  EXPECT_TRUE(original_cryptographer->EncryptString(kText1, &encrypted1));

  original_cryptographer->SelectDefaultEncryptionKey(key_name2);
  sync_pb::EncryptedData encrypted2;
  EXPECT_TRUE(original_cryptographer->EncryptString(kText2, &encrypted2));

  // Restore a new cryptographer from proto.
  std::unique_ptr<CryptographerImpl> restored_cryptographer =
      CryptographerImpl::FromLocalProto(original_cryptographer->ToLocalProto());
  ASSERT_THAT(restored_cryptographer, NotNull());
  EXPECT_TRUE(restored_cryptographer->CanEncrypt());
  EXPECT_TRUE(restored_cryptographer->HasCrossUserSharingKeyPair(0));

  std::string decrypted;
  EXPECT_TRUE(restored_cryptographer->DecryptToString(encrypted1, &decrypted));
  EXPECT_THAT(decrypted, Eq(kText1));
  EXPECT_TRUE(restored_cryptographer->DecryptToString(encrypted2, &decrypted));
  EXPECT_THAT(decrypted, Eq(kText2));
}

TEST(CryptographerImplTest, ShouldExportDefaultKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  const std::string key_name = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  ASSERT_THAT(key_name, Ne(std::string()));

  cryptographer->SelectDefaultEncryptionKey(key_name);
  ASSERT_TRUE(cryptographer->CanEncrypt());

  sync_pb::NigoriKey exported_key = cryptographer->ExportDefaultKey();
  EXPECT_FALSE(exported_key.has_deprecated_name());

  // The exported key, even without name, should be importable, and the
  // resulting key name should match the original.
  EXPECT_THAT(NigoriKeyBag::CreateEmpty().AddKeyFromProto(exported_key),
              Eq(key_name));
}

TEST(CryptographerImplTest, ShouldSetCrossUserSharingKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  std::optional<CrossUserSharingPublicPrivateKeyPair> key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  ASSERT_TRUE(key_pair.has_value());
  ASSERT_FALSE(cryptographer->HasCrossUserSharingKeyPair(0));

  cryptographer->SetCrossUserSharingKeyPair(std::move(key_pair.value()), 0);

  EXPECT_TRUE(cryptographer->HasCrossUserSharingKeyPair(0));
}

TEST(CryptographerImplTest, ShouldEmplaceKeysFrom) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name_1 =
      key_bag.AddKey(KeyDerivationParams::CreateForPbkdf2(), "password1");
  const std::string key_name_2 =
      key_bag.AddKey(KeyDerivationParams::CreateForPbkdf2(), "password2");
  ASSERT_FALSE(cryptographer->HasKey(key_name_1));
  ASSERT_FALSE(cryptographer->HasKey(key_name_2));

  cryptographer->EmplaceKeysFrom(key_bag);

  EXPECT_TRUE(cryptographer->HasKey(key_name_1));
  EXPECT_TRUE(cryptographer->HasKey(key_name_2));
}

TEST(CryptographerImplTest, ShouldEmplaceExistingKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->HasCrossUserSharingKeyPair(0));
  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  ASSERT_TRUE(cryptographer->HasCrossUserSharingKeyPair(0));

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);

  EXPECT_TRUE(cryptographer->HasCrossUserSharingKeyPair(0));
}

TEST(CryptographerImplTest, ShouldReplaceCrossUserSharingKeys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->HasCrossUserSharingKeyPair(0));
  CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  1);

  cryptographer->ReplaceCrossUserSharingKeys(std::move(keys));

  EXPECT_TRUE(cryptographer->HasCrossUserSharingKeyPair(0));
  EXPECT_TRUE(cryptographer->HasCrossUserSharingKeyPair(1));
}

TEST(CryptographerImplTest, ShouldOverwritePreexistingKeys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  CrossUserSharingKeys old_keys = CrossUserSharingKeys::CreateEmpty();
  old_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
      /*version=*/0);
  old_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
      /*version=*/1);
  cryptographer->ReplaceCrossUserSharingKeys(old_keys.Clone());
  ASSERT_TRUE(
      cryptographer->HasCrossUserSharingKeyPair(/*key_pair_version=*/0));
  ASSERT_TRUE(
      cryptographer->HasCrossUserSharingKeyPair(/*key_pair_version=*/1));

  // Generate a new key pair and replace the pre-existing one with the same
  // version. The version 1 should also disappear.
  CrossUserSharingKeys new_keys = CrossUserSharingKeys::CreateEmpty();
  new_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
      /*version=*/0);
  cryptographer->ReplaceCrossUserSharingKeys(new_keys.Clone());
  ASSERT_TRUE(
      cryptographer->HasCrossUserSharingKeyPair(/*key_pair_version=*/0));
  ASSERT_FALSE(
      cryptographer->HasCrossUserSharingKeyPair(/*key_pair_version=*/1));
  EXPECT_EQ(cryptographer->GetCrossUserSharingKeyPair(/*version=*/0)
                .GetRawPrivateKey(),
            new_keys.GetKeyPair(/*version=*/0).GetRawPrivateKey());
  EXPECT_NE(cryptographer->GetCrossUserSharingKeyPair(/*version=*/0)
                .GetRawPrivateKey(),
            old_keys.GetKeyPair(/*version=*/0).GetRawPrivateKey());
}

TEST(CryptographerImplTest, ShouldOverwriteOnlyOneKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  // Replace only one key with a new value.
  auto raw_existing_private_key0 =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/0)
          .GetRawPrivateKey();
  auto raw_existing_private_key1 =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/1)
          .GetRawPrivateKey();
  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);

  EXPECT_NE(raw_existing_private_key0,
            cryptographer->GetCrossUserSharingKeyPair(/*version=*/0)
                .GetRawPrivateKey());
  EXPECT_EQ(raw_existing_private_key1,
            cryptographer->GetCrossUserSharingKeyPair(/*version=*/1)
                .GetRawPrivateKey());
}

TEST(CryptographerImplTest, ShouldEncryptAndDecryptForCrossUserSharing) {
  std::unique_ptr<CryptographerImpl> cryptographer_sender =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer_sender, NotNull());
  cryptographer_sender->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer_sender->SelectDefaultCrossUserSharingKey(0);
  std::unique_ptr<CryptographerImpl> cryptographer_recipient =
      CryptographerImpl::CreateEmpty();

  ASSERT_THAT(cryptographer_recipient, NotNull());
  cryptographer_recipient->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer_recipient->SelectDefaultCrossUserSharingKey(0);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer_sender->AuthEncryptForCrossUserSharing(
          base::as_byte_span(plaintext),
          cryptographer_recipient->GetCrossUserSharingKeyPair(0)
              .GetRawPublicKey());

  EXPECT_TRUE(encrypted_message.has_value());

  std::optional<std::vector<uint8_t>> decrypted_message =
      cryptographer_recipient->AuthDecryptForCrossUserSharing(
          encrypted_message.value(),
          cryptographer_sender->GetCrossUserSharingKeyPair(0).GetRawPublicKey(),
          0);

  EXPECT_TRUE(decrypted_message.has_value());
  EXPECT_THAT(decrypted_message.value(), testing::ElementsAreArray(plaintext));
}

TEST(CryptographerImplTest, ShouldClearCrossUserSharingKeys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  cryptographer->ClearAllKeys();

  EXPECT_FALSE(cryptographer->HasCrossUserSharingKeyPair(0));
  EXPECT_FALSE(cryptographer->HasCrossUserSharingKeyPair(1));
}

TEST(CryptographerImplTest, ShouldEmplaceAllNigoriKeysFrom) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->CanEncrypt());

  const std::string key_name = cryptographer->EmplaceKey(
      "password_local", KeyDerivationParams::CreateForPbkdf2());

  ASSERT_THAT(key_name, Ne(std::string()));
  cryptographer->SelectDefaultEncryptionKey(key_name);
  ASSERT_TRUE(cryptographer->CanEncrypt());

  std::unique_ptr<CryptographerImpl> other_cryptographer =
      CryptographerImpl::CreateEmpty();

  ASSERT_THAT(other_cryptographer, NotNull());
  ASSERT_FALSE(other_cryptographer->CanEncrypt());

  const std::string key_name_other = other_cryptographer->EmplaceKey(
      "password_other", KeyDerivationParams::CreateForPbkdf2());

  ASSERT_THAT(key_name_other, Ne(std::string()));
  other_cryptographer->SelectDefaultEncryptionKey(key_name_other);
  ASSERT_TRUE(other_cryptographer->CanEncrypt());

  cryptographer->EmplaceAllNigoriKeysFrom(*other_cryptographer);

  EXPECT_TRUE(cryptographer->HasKey(key_name));
  EXPECT_TRUE(cryptographer->HasKey(key_name_other));
}

TEST(CryptographerImplTest, ShouldCloneDefaultCrossUserSharingKeyVersion) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  cryptographer->SetCrossUserSharingKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);
  cryptographer->SelectDefaultCrossUserSharingKey(4);

  std::unique_ptr<CryptographerImpl> cloned = cryptographer->Clone();
  ASSERT_THAT(cloned, NotNull());

  const std::string plaintext = "Sharing is caring";
  std::optional<std::vector<uint8_t>> encrypted_message =
      cloned->AuthEncryptForCrossUserSharing(
          base::as_byte_span(plaintext),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_TRUE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldSerializeToLocalProto) {
  std::unique_ptr<CryptographerImpl> original =
      CryptographerImpl::CreateEmpty();
  const std::string key_name =
      original->EmplaceKey("password", KeyDerivationParams::CreateForPbkdf2());
  original->SelectDefaultEncryptionKey(key_name);

  sync_pb::CryptographerData proto = original->ToLocalProto();
  EXPECT_THAT(proto.default_key_name(), Eq(key_name));
  EXPECT_TRUE(proto.key_bag().key_size() > 0);

  std::unique_ptr<CryptographerImpl> restored =
      CryptographerImpl::FromLocalProto(proto);
  ASSERT_THAT(restored, NotNull());
  EXPECT_THAT(restored->GetDefaultEncryptionKeyName(), Eq(key_name));
  EXPECT_TRUE(restored->CanEncrypt());
}

TEST(CryptographerImplTest, ShouldReturnNullOnInvalidLocalProto) {
  sync_pb::CryptographerData proto;
  proto.set_default_key_name("non_existent_key");
  // The key bag is empty, so "non_existent_key" is missing.

  std::unique_ptr<CryptographerImpl> restored =
      CryptographerImpl::FromLocalProto(proto);
  EXPECT_THAT(restored, IsNull());
}

TEST(CryptographerImplTest, ShouldExportEncryptedKeyBagWithOneKey) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  const std::string key_name = cryptographer->EmplaceKey(
      "password", KeyDerivationParams::CreateForPbkdf2());
  cryptographer->SelectDefaultEncryptionKey(key_name);

  sync_pb::EncryptedData exported = cryptographer->ExportEncryptedKeyBag();
  EXPECT_TRUE(exported.has_blob());
  EXPECT_THAT(exported.key_name(), Eq(key_name));

  sync_pb::EncryptionKeys decrypted_keys_proto;
  ASSERT_TRUE(cryptographer->Decrypt(exported, &decrypted_keys_proto));

  ASSERT_EQ(decrypted_keys_proto.key_size(), 1);
  std::unique_ptr<Nigori> decrypted_key =
      Nigori::CreateByImport(NigoriPassKey::ForTesting(),
                             decrypted_keys_proto.key(0).deprecated_user_key(),
                             decrypted_keys_proto.key(0).encryption_key(),
                             decrypted_keys_proto.key(0).mac_key());
  EXPECT_THAT(decrypted_key, HasKeyName(key_name));
}

TEST(CryptographerImplTest, ShouldExportEncryptedKeyBagWithMultipleKeys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  const std::string key_name1 = cryptographer->EmplaceKey(
      "password1", KeyDerivationParams::CreateForPbkdf2());
  const std::string key_name2 = cryptographer->EmplaceKey(
      "password2", KeyDerivationParams::CreateForPbkdf2());
  cryptographer->SelectDefaultEncryptionKey(key_name2);

  sync_pb::EncryptedData exported = cryptographer->ExportEncryptedKeyBag();
  EXPECT_TRUE(exported.has_blob());
  EXPECT_THAT(exported.key_name(), Eq(key_name2));

  sync_pb::EncryptionKeys decrypted_keys_proto;
  ASSERT_TRUE(cryptographer->Decrypt(exported, &decrypted_keys_proto));

  std::vector<std::unique_ptr<Nigori>> decrypted_keys;
  for (const sync_pb::NigoriKey& decrypted_key : decrypted_keys_proto.key()) {
    decrypted_keys.push_back(Nigori::CreateByImport(
        NigoriPassKey::ForTesting(), decrypted_key.deprecated_user_key(),
        decrypted_key.encryption_key(), decrypted_key.mac_key()));
  }

  EXPECT_THAT(decrypted_keys, UnorderedElementsAre(HasKeyName(key_name1),
                                                   HasKeyName(key_name2)));
}

}  // namespace syncer
