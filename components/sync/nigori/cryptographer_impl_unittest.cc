// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

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

}  // namespace syncer
