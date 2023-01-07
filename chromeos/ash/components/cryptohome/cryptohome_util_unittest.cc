// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/cryptohome_util.h"

#include <string>

#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cryptohome {

using ::ash::ChallengeResponseKey;

constexpr char kKeyLabelStr[] = "key_label";

class CryptohomeUtilTest : public ::testing::Test {
 protected:
  const KeyLabel kKeyLabel = KeyLabel(kKeyLabelStr);
};

TEST_F(CryptohomeUtilTest, CreateAuthorizationRequestEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(KeyLabel(), kExpectedSecret);

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST_F(CryptohomeUtilTest, CreateAuthorizationRequestWithLabel) {
  const std::string kExpectedLabel = "some_label";
  const std::string kExpectedSecret = "some_secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(KeyLabel(kExpectedLabel), kExpectedSecret);

  EXPECT_EQ(auth_request.key().data().label(), kExpectedLabel);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST_F(CryptohomeUtilTest,
       CreateAuthorizationRequestFromKeyDefPasswordEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, KeyLabel(), PRIV_DEFAULT));

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST_F(CryptohomeUtilTest,
       CreateAuthorizationRequestFromKeyDefPasswordWithLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, kKeyLabel, PRIV_DEFAULT));

  EXPECT_EQ(auth_request.key().data().label(), kKeyLabelStr);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST_F(CryptohomeUtilTest,
       CreateAuthorizationRequestFromKeyDefChallengeResponse) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  const std::string kKeySpki = "spki";
  const Algorithm kKeyAlgorithm = Algorithm::kRsassaPkcs1V15Sha1;
  const ChallengeSignatureAlgorithm kKeyAlgorithmProto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;

  ChallengeResponseKey challenge_response_key;
  challenge_response_key.set_public_key_spki_der(kKeySpki);
  challenge_response_key.set_signature_algorithms({kKeyAlgorithm});
  const KeyDefinition key_def = KeyDefinition::CreateForChallengeResponse(
      {challenge_response_key}, kKeyLabel, PRIV_DEFAULT);

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(key_def);

  EXPECT_FALSE(auth_request.key().has_secret());
  EXPECT_EQ(auth_request.key().data().type(),
            KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  EXPECT_EQ(auth_request.key().data().label(), kKeyLabelStr);
  ASSERT_EQ(auth_request.key().data().challenge_response_key_size(), 1);
  EXPECT_EQ(
      auth_request.key().data().challenge_response_key(0).public_key_spki_der(),
      kKeySpki);
  ASSERT_EQ(auth_request.key()
                .data()
                .challenge_response_key(0)
                .signature_algorithm_size(),
            1);
  EXPECT_EQ(
      auth_request.key().data().challenge_response_key(0).signature_algorithm(
          0),
      kKeyAlgorithmProto);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyType) {
  Key key;

  KeyDefinitionToKey(KeyDefinition(), &key);

  EXPECT_EQ(key.data().type(), KeyData::KEY_TYPE_PASSWORD);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeySecret) {
  const std::string kExpectedSecret = "my_dog_ate_my_homework";
  KeyDefinition key_def;
  key_def.secret = kExpectedSecret;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.secret(), kExpectedSecret);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyLabel) {
  const std::string kExpectedLabel = "millenials hate labels";
  KeyDefinition key_def;
  key_def.label = KeyLabel(kExpectedLabel);
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().label(), kExpectedLabel);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyNonpositiveRevision) {
  KeyDefinition key_def;
  key_def.revision = -1;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), 0);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyPositiveRevision) {
  constexpr int kExpectedRevision = 10;
  KeyDefinition key_def;
  key_def.revision = kExpectedRevision;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), kExpectedRevision);
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyDefaultPrivileges) {
  KeyDefinition key_def;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyAddPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_ADD;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_FALSE(privileges.update());
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyRemovePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_REMOVE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_FALSE(privileges.update());
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyUpdatePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_MIGRATE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

TEST_F(CryptohomeUtilTest, KeyDefinitionToKeyAllPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_DEFAULT;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

// Test the KeyDefinitionToKey() function against the KeyDefinition struct of
// the |TYPE_CHALLENGE_RESPONSE| type.
TEST_F(CryptohomeUtilTest, KeyDefinitionToKey_ChallengeResponse) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  const int kPrivileges = 0;
  const std::string kKey1Spki = "spki1";
  const Algorithm kKey1Algorithm = Algorithm::kRsassaPkcs1V15Sha1;
  const ChallengeSignatureAlgorithm kKey1AlgorithmProto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;
  const std::string kKey2Spki = "spki2";
  const Algorithm kKey2Algorithm1 = Algorithm::kRsassaPkcs1V15Sha512;
  const ChallengeSignatureAlgorithm kKey2Algorithm1Proto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA512;
  const Algorithm kKey2Algorithm2 = Algorithm::kRsassaPkcs1V15Sha256;
  const ChallengeSignatureAlgorithm kKey2Algorithm2Proto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;

  ChallengeResponseKey challenge_response_key1;
  challenge_response_key1.set_public_key_spki_der(kKey1Spki);
  challenge_response_key1.set_signature_algorithms({kKey1Algorithm});
  ChallengeResponseKey challenge_response_key2;
  challenge_response_key2.set_public_key_spki_der(kKey2Spki);
  challenge_response_key2.set_signature_algorithms(
      {kKey2Algorithm1, kKey2Algorithm2});
  const KeyDefinition key_def = KeyDefinition::CreateForChallengeResponse(
      {challenge_response_key1, challenge_response_key2}, kKeyLabel,
      kPrivileges);
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_FALSE(key.has_secret());
  EXPECT_EQ(key.data().type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  EXPECT_EQ(key.data().label(), kKeyLabelStr);
  ASSERT_EQ(key.data().challenge_response_key_size(), 2);
  EXPECT_EQ(key.data().challenge_response_key(0).public_key_spki_der(),
            kKey1Spki);
  ASSERT_EQ(key.data().challenge_response_key(0).signature_algorithm_size(), 1);
  EXPECT_EQ(key.data().challenge_response_key(0).signature_algorithm(0),
            kKey1AlgorithmProto);
  EXPECT_EQ(key.data().challenge_response_key(1).public_key_spki_der(),
            kKey2Spki);
  ASSERT_EQ(key.data().challenge_response_key(1).signature_algorithm_size(), 2);
  EXPECT_EQ(key.data().challenge_response_key(1).signature_algorithm(0),
            kKey2Algorithm1Proto);
  EXPECT_EQ(key.data().challenge_response_key(1).signature_algorithm(1),
            kKey2Algorithm2Proto);
}

}  // namespace cryptohome
