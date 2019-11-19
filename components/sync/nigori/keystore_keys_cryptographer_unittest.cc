// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/keystore_keys_cryptographer.h"

#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::NotNull;

std::string ComputeKeystoreKeyName(const std::string& keystore_key) {
  std::string key_name;
  Nigori::CreateByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                             keystore_key)
      ->Permute(Nigori::Password, kNigoriKeyName, &key_name);
  return key_name;
}

TEST(KeystoreKeysCryptographerTest, ShouldCreateEmpty) {
  std::unique_ptr<KeystoreKeysCryptographer> keystore_keys_cryptographer =
      KeystoreKeysCryptographer::CreateEmpty();

  EXPECT_TRUE(keystore_keys_cryptographer->IsEmpty());
  EXPECT_TRUE(keystore_keys_cryptographer->keystore_keys().empty());
  EXPECT_TRUE(keystore_keys_cryptographer->GetLastKeystoreKeyName().empty());

  std::unique_ptr<CryptographerImpl> underlying_cryptographer =
      keystore_keys_cryptographer->ToCryptographerImpl();
  ASSERT_THAT(underlying_cryptographer, NotNull());
  EXPECT_FALSE(underlying_cryptographer->CanEncrypt());
}

TEST(KeystoreKeysCryptographerTest, ShouldCreateNonEmpty) {
  const std::vector<std::string> kKeystoreKeys = {"key1", "key2"};
  const std::string keystore_key_name1 =
      ComputeKeystoreKeyName(kKeystoreKeys[0]);
  const std::string keystore_key_name2 =
      ComputeKeystoreKeyName(kKeystoreKeys[1]);
  std::unique_ptr<KeystoreKeysCryptographer> keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(kKeystoreKeys);

  EXPECT_FALSE(keystore_keys_cryptographer->IsEmpty());
  EXPECT_THAT(keystore_keys_cryptographer->keystore_keys(), Eq(kKeystoreKeys));
  EXPECT_THAT(keystore_keys_cryptographer->GetLastKeystoreKeyName(),
              Eq(keystore_key_name2));

  std::unique_ptr<CryptographerImpl> underlying_cryptographer =
      keystore_keys_cryptographer->ToCryptographerImpl();
  ASSERT_THAT(underlying_cryptographer, NotNull());
  EXPECT_TRUE(underlying_cryptographer->CanEncrypt());
  EXPECT_TRUE(underlying_cryptographer->HasKey(keystore_key_name1));
  EXPECT_THAT(underlying_cryptographer->GetDefaultEncryptionKeyName(),
              Eq(keystore_key_name2));
}

}  // namespace

}  // namespace syncer
