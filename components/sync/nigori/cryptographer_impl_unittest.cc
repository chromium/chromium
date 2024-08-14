// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::Ne;
using testing::NotNull;

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

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);
  cryptographer->SelectDefaultCrossUserSharingKey(4);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(plaintext)),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_TRUE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldFailOnNonSetEncryptionKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(plaintext)),
          CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
              .GetRawPublicKey());

  EXPECT_FALSE(encrypted_message.has_value());
}

TEST(CryptographerImplTest, ShouldFailOnNonExistentDefaultEncryptionKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 4);
  cryptographer->SelectDefaultCrossUserSharingKey(3);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(plaintext)),
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
  original_cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);

  original_cryptographer->SelectDefaultEncryptionKey(key_name1);
  sync_pb::EncryptedData encrypted1;
  EXPECT_TRUE(original_cryptographer->EncryptString(kText1, &encrypted1));

  original_cryptographer->SelectDefaultEncryptionKey(key_name2);
  sync_pb::EncryptedData encrypted2;
  EXPECT_TRUE(original_cryptographer->EncryptString(kText2, &encrypted2));

  // Restore a new cryptographer from proto.
  std::unique_ptr<CryptographerImpl> restored_cryptographer =
      CryptographerImpl::FromProto(original_cryptographer->ToProto());
  ASSERT_THAT(restored_cryptographer, NotNull());
  EXPECT_TRUE(restored_cryptographer->CanEncrypt());
  EXPECT_TRUE(restored_cryptographer->HasKeyPair(0));

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

TEST(CryptographerImplTest, ShouldSetKeyPair) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  std::optional<CrossUserSharingPublicPrivateKeyPair> key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  ASSERT_TRUE(key_pair.has_value());
  ASSERT_FALSE(cryptographer->HasKeyPair(0));

  cryptographer->SetKeyPair(std::move(key_pair.value()), 0);

  EXPECT_TRUE(cryptographer->HasKeyPair(0));
}

TEST(CryptographerImplTest, ShouldEmplaceKeysFrom) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name_1 = key_bag.AddKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password1"));
  const std::string key_name_2 = key_bag.AddKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password2"));
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
  ASSERT_FALSE(cryptographer->HasKeyPair(0));
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  ASSERT_TRUE(cryptographer->HasKeyPair(0));

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);

  EXPECT_TRUE(cryptographer->HasKeyPair(0));
}

TEST(CryptographerImplTest, ShouldReplaceCrossUserSharingKeys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  ASSERT_THAT(cryptographer, NotNull());
  ASSERT_FALSE(cryptographer->HasKeyPair(0));
  CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  1);

  cryptographer->ReplaceCrossUserSharingKeys(std::move(keys));

  EXPECT_TRUE(cryptographer->HasKeyPair(0));
  EXPECT_TRUE(cryptographer->HasKeyPair(1));
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
  ASSERT_TRUE(cryptographer->HasKeyPair(/*key_pair_version=*/0));
  ASSERT_TRUE(cryptographer->HasKeyPair(/*key_pair_version=*/1));

  // Generate a new key pair and replace the pre-existing one with the same
  // version. The version 1 should also disappear.
  CrossUserSharingKeys new_keys = CrossUserSharingKeys::CreateEmpty();
  new_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
      /*version=*/0);
  cryptographer->ReplaceCrossUserSharingKeys(new_keys.Clone());
  ASSERT_TRUE(cryptographer->HasKeyPair(/*key_pair_version=*/0));
  ASSERT_FALSE(cryptographer->HasKeyPair(/*key_pair_version=*/1));
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

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  // Replace only one key with a new value.
  auto raw_existing_private_key0 =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/0)
          .GetRawPrivateKey();
  auto raw_existing_private_key1 =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/1)
          .GetRawPrivateKey();
  cryptographer->SetKeyPair(
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
  cryptographer_sender->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer_sender->SelectDefaultCrossUserSharingKey(0);
  std::unique_ptr<CryptographerImpl> cryptographer_recipient =
      CryptographerImpl::CreateEmpty();

  ASSERT_THAT(cryptographer_recipient, NotNull());
  cryptographer_recipient->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer_recipient->SelectDefaultCrossUserSharingKey(0);

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      cryptographer_sender->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(plaintext)),
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

  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  cryptographer->SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  cryptographer->ClearAllKeys();

  EXPECT_FALSE(cryptographer->HasKeyPair(0));
  EXPECT_FALSE(cryptographer->HasKeyPair(1));
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

}  // namespace syncer
