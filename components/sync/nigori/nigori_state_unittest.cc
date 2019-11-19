// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_state.h"

#include "components/sync/base/time.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori.h"
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

}  // namespace

}  // namespace syncer
