// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/custom_passphrase_bootstrap_token.h"

#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Not;

MATCHER_P(MatchesToken, expected_token, "") {
  return arg.ToProto().SerializeAsString() ==
         expected_token.ToProto().SerializeAsString();
}

TEST(CustomPassphraseBootstrapTokenTest, ShouldBeEmptyWhenDefaultConstructed) {
  CustomPassphraseBootstrapToken token;
  EXPECT_TRUE(token.IsEmpty());
  EXPECT_EQ(token.ToEncryptedPref(os_crypt_async::GetTestEncryptorForTesting()),
            "");
}

TEST(CustomPassphraseBootstrapTokenTest,
     ShouldPreserveFieldsWhenConvertedToAndFromProto) {
  sync_pb::NigoriKey proto;
  proto.set_encryption_key("enc_key");
  proto.set_mac_key("mac_key");
  proto.set_deprecated_name("key_name");

  CustomPassphraseBootstrapToken token =
      CustomPassphraseBootstrapToken::FromProto(proto);
  EXPECT_FALSE(token.IsEmpty());

  const sync_pb::NigoriKey& exported = token.ToProto();
  EXPECT_EQ(exported.encryption_key(), "enc_key");
  EXPECT_EQ(exported.mac_key(), "mac_key");
  EXPECT_EQ(exported.deprecated_name(), "key_name");
}

TEST(CustomPassphraseBootstrapTokenTest,
     ShouldSuccessfullyEncryptAndDecryptPref) {
  sync_pb::NigoriKey proto;
  proto.set_encryption_key("enc_key");
  proto.set_mac_key("mac_key");

  CustomPassphraseBootstrapToken token =
      CustomPassphraseBootstrapToken::FromProto(proto);

  os_crypt_async::Encryptor encryptor =
      os_crypt_async::GetTestEncryptorForTesting();
  std::string encrypted_pref = token.ToEncryptedPref(encryptor);
  EXPECT_NE(encrypted_pref, "");

  CustomPassphraseBootstrapToken decrypted_token =
      CustomPassphraseBootstrapToken::FromEncryptedPref(encrypted_pref,
                                                        encryptor);
  EXPECT_THAT(decrypted_token, MatchesToken(token));
}

TEST(CustomPassphraseBootstrapTokenTest, ShouldSupportEqualityOperator) {
  sync_pb::NigoriKey proto1;
  proto1.set_encryption_key("enc_key");
  proto1.set_mac_key("mac_key");

  sync_pb::NigoriKey proto2;
  proto2.set_encryption_key("enc_key");
  proto2.set_mac_key("mac_key");

  EXPECT_THAT(CustomPassphraseBootstrapToken::FromProto(proto1),
              MatchesToken(CustomPassphraseBootstrapToken::FromProto(proto2)));

  proto2.set_encryption_key("other");
  EXPECT_THAT(
      CustomPassphraseBootstrapToken::FromProto(proto1),
      Not(MatchesToken(CustomPassphraseBootstrapToken::FromProto(proto2))));
}

}  // namespace
}  // namespace syncer
