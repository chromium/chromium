// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_util.h"

#include <string>

#include "base/optional.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::ChallengeResponseKey;

namespace cryptohome {

constexpr char kKeyLabel[] = "key_label";

constexpr int64_t kKeyRevision = 123;
constexpr char kProviderData1Name[] = "data_1";
constexpr int64_t kProviderData1Number = 12345;
constexpr char kProviderData2Name[] = "data_2";
constexpr char kProviderData2Bytes[] = "data_2 bytes";

TEST(CryptohomeUtilTest, CreateAuthorizationRequestEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(std::string(), kExpectedSecret);

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest, CreateAuthorizationRequestWithLabel) {
  const std::string kExpectedLabel = "some_label";
  const std::string kExpectedSecret = "some_secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(kExpectedLabel, kExpectedSecret);

  EXPECT_EQ(auth_request.key().data().label(), kExpectedLabel);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
     CreateAuthorizationRequestFromKeyDefPasswordEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, std::string() /* label */, PRIV_DEFAULT));

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
     CreateAuthorizationRequestFromKeyDefPasswordWithLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, kKeyLabel, PRIV_DEFAULT));

  EXPECT_EQ(auth_request.key().data().label(), kKeyLabel);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
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
  EXPECT_EQ(auth_request.key().data().label(), kKeyLabel);
  EXPECT_TRUE(auth_request.key().data().privileges().mount());
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

TEST(CryptohomeUtilTest, BaseReplyToMountErrorNullOptional) {
  const base::Optional<BaseReply> reply = base::nullopt;

  MountError actual_error = BaseReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, BaseReplyToMountErrorEmptyReply) {
  const base::Optional<BaseReply> reply(base::in_place);

  MountError actual_error = BaseReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_NONE);
}

TEST(CryptohomeUtilTest, BaseReplyToMountErrorNoError) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = BaseReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_NONE);
}

TEST(CryptohomeUtilTest, BaseReplyToMountErrorAuthFailure) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = BaseReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_KEY_FAILURE);
}

TEST(CryptohomeUtilTest, MountExReplyToMountErrorNullOptional) {
  const base::Optional<BaseReply> reply = base::nullopt;

  MountError actual_error = MountExReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, MountExReplyToMountErrorEmptyReply) {
  BaseReply result;
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = MountExReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, MountExReplyToMountErrorNoExtension) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = MountExReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, MountExReplyToMountErrorNoError) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  result.MutableExtension(MountReply::reply);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = MountExReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_NONE);
}

TEST(CryptohomeUtilTest, MountExReplyToMountErrorAuthFailure) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  result.MutableExtension(MountReply::reply);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = MountExReplyToMountError(reply);

  EXPECT_EQ(actual_error, MOUNT_ERROR_KEY_FAILURE);
}

TEST(CryptohomeUtilTest, MountExReplyToMountHash) {
  const std::string expected_hash = "username";
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  MountReply* mount = result.MutableExtension(MountReply::reply);
  mount->set_sanitized_username(expected_hash);

  const std::string actual_hash = MountExReplyToMountHash(result);

  EXPECT_EQ(actual_hash, expected_hash);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyType) {
  Key key;

  KeyDefinitionToKey(KeyDefinition(), &key);

  EXPECT_EQ(key.data().type(), KeyData::KEY_TYPE_PASSWORD);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeySecret) {
  const std::string kExpectedSecret = "my_dog_ate_my_homework";
  KeyDefinition key_def;
  key_def.secret = kExpectedSecret;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyLabel) {
  const std::string kExpectedLabel = "millenials hate labels";
  KeyDefinition key_def;
  key_def.label = kExpectedLabel;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().label(), kExpectedLabel);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyNonpositiveRevision) {
  KeyDefinition key_def;
  key_def.revision = -1;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), 0);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyPositiveRevision) {
  constexpr int kExpectedRevision = 10;
  KeyDefinition key_def;
  key_def.revision = kExpectedRevision;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), kExpectedRevision);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyDefaultPrivileges) {
  KeyDefinition key_def;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.mount());
  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
  EXPECT_FALSE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyMountPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_MOUNT;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.mount());
  EXPECT_FALSE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_FALSE(privileges.update());
  EXPECT_FALSE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyAddPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_ADD;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.mount());
  EXPECT_TRUE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_FALSE(privileges.update());
  EXPECT_FALSE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyRemovePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_REMOVE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.mount());
  EXPECT_FALSE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_FALSE(privileges.update());
  EXPECT_FALSE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyUpdatePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_MIGRATE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.mount());
  EXPECT_FALSE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_TRUE(privileges.update());
  EXPECT_FALSE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyAuthorizedUpdatePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_AUTHORIZED_UPDATE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.mount());
  EXPECT_FALSE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_FALSE(privileges.update());
  EXPECT_TRUE(privileges.authorized_update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyAllPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_DEFAULT | PRIV_AUTHORIZED_UPDATE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.mount());
  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
  EXPECT_TRUE(privileges.authorized_update());
}

// Test the KeyDefinitionToKey() function against the KeyDefinition struct of
// the |TYPE_CHALLENGE_RESPONSE| type.
TEST(CryptohomeUtilTest, KeyDefinitionToKey_ChallengeResponse) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  const int kPrivileges = PRIV_MOUNT;
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
  EXPECT_EQ(key.data().label(), kKeyLabel);
  EXPECT_TRUE(key.data().privileges().mount());
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

TEST(CryptohomeUtilTest, KeyAuthorizationDataToAuthorizationDataHmacSha256) {
  KeyAuthorizationData auth_data_proto;
  auth_data_proto.set_type(
      KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256);
  KeyDefinition::AuthorizationData auth_data;

  KeyAuthorizationDataToAuthorizationData(auth_data_proto, &auth_data);

  EXPECT_EQ(auth_data.type, KeyDefinition::AuthorizationData::TYPE_HMACSHA256);
}

TEST(CryptohomeUtilTest, KeyAuthorizationDataToAuthorizationDataAes256Cbc) {
  KeyAuthorizationData auth_data_proto;
  auth_data_proto.set_type(
      KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_AES256CBC_HMACSHA256);
  KeyDefinition::AuthorizationData auth_data;

  KeyAuthorizationDataToAuthorizationData(auth_data_proto, &auth_data);

  EXPECT_EQ(auth_data.type,
            KeyDefinition::AuthorizationData::TYPE_AES256CBC_HMACSHA256);
}

TEST(CryptohomeUtilTest, KeyAuthorizationDataToAuthorizationDataSecret) {
  constexpr bool kEncrypt = true;
  constexpr bool kSign = false;
  constexpr bool kWrapped = true;
  const std::string kSymmetricKey = "symmetric_key";
  const std::string kPublicKey = "public_key";
  KeyAuthorizationData auth_data_proto;
  KeyAuthorizationSecret* secret = auth_data_proto.add_secrets();
  KeyAuthorizationSecretUsage* usage = secret->mutable_usage();
  usage->set_encrypt(kEncrypt);
  usage->set_sign(kSign);
  secret->set_wrapped(kWrapped);
  secret->set_symmetric_key(kSymmetricKey);
  secret->set_public_key(kPublicKey);
  KeyDefinition::AuthorizationData::Secret expected_secret(
      kEncrypt, kSign, kSymmetricKey, kPublicKey, kWrapped);
  KeyDefinition::AuthorizationData auth_data;

  KeyAuthorizationDataToAuthorizationData(auth_data_proto, &auth_data);

  EXPECT_EQ(auth_data.secrets.back(), expected_secret);
}

TEST(CryptohomeUtilTest, AccountDiskUsageReplyToUsageSizeNullOptional) {
  const base::Optional<BaseReply> reply = base::nullopt;

  int64_t size = AccountDiskUsageReplyToUsageSize(reply);

  ASSERT_EQ(size, -1);
}

TEST(CryptohomeUtilTest, AccountDiskUsageReplyToUsageSizeEmptyReply) {
  const base::Optional<BaseReply> reply(base::in_place);

  int64_t size = AccountDiskUsageReplyToUsageSize(reply);

  ASSERT_EQ(size, -1);
}

TEST(CryptohomeUtilTest, AccountDiskUsageReplyToUsageSizeNoExtension) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const base::Optional<BaseReply> reply = std::move(result);

  int64_t size = AccountDiskUsageReplyToUsageSize(reply);

  ASSERT_EQ(size, -1);
}

TEST(CryptohomeUtilTest, AccountDiskUsageReplyToUsageSizeErrorInReply) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
  result.MutableExtension(GetAccountDiskUsageReply::reply);
  const base::Optional<BaseReply> reply = std::move(result);

  int64_t size = AccountDiskUsageReplyToUsageSize(reply);

  ASSERT_EQ(size, -1);
}

TEST(CryptohomeUtilTest, AccountDiskUsageReplyToUsageSizeValidReply) {
  constexpr int64_t expected_size = 100;
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  GetAccountDiskUsageReply* usage_reply =
      result.MutableExtension(GetAccountDiskUsageReply::reply);
  usage_reply->set_size(expected_size);
  const base::Optional<BaseReply> reply = std::move(result);

  int64_t size = AccountDiskUsageReplyToUsageSize(reply);

  ASSERT_EQ(size, expected_size);
}

TEST(CryptohomeUtilTest, GetKeyDataReplyToMountErrorNullOptional) {
  const base::Optional<BaseReply> reply = base::nullopt;

  MountError actual_error = GetKeyDataReplyToMountError(reply);

  ASSERT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, GetKeyDataReplyToMountErrorEmptyReply) {
  const base::Optional<BaseReply> reply(base::in_place);

  MountError actual_error = GetKeyDataReplyToMountError(reply);

  ASSERT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, GetKeyDataReplyToMountErrorNoExtension) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = GetKeyDataReplyToMountError(reply);

  ASSERT_EQ(actual_error, MOUNT_ERROR_FATAL);
}

TEST(CryptohomeUtilTest, GetKeyDataReplyToMountErrorErrorInReply) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  result.MutableExtension(GetKeyDataReply::reply);
  const base::Optional<BaseReply> reply = std::move(result);

  MountError actual_error = GetKeyDataReplyToMountError(reply);

  ASSERT_EQ(actual_error, MOUNT_ERROR_KEY_FAILURE);
}

TEST(CryptohomeUtilTest, GetKeyDataReplyToKeyDefinitionsTwoEntries) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  GetKeyDataReply* get_key_data_reply =
      result.MutableExtension(GetKeyDataReply::reply);
  KeyData* key_data = get_key_data_reply->add_key_data();
  key_data->set_type(KeyData::KEY_TYPE_PASSWORD);
  key_data->set_label(kKeyLabel);
  key_data->mutable_privileges()->set_update(false);
  key_data->set_revision(kKeyRevision);
  key_data->add_authorization_data()->set_type(
      KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256);
  KeyProviderData* data = key_data->mutable_provider_data();
  KeyProviderData::Entry* entry1 = data->add_entry();
  entry1->set_name(kProviderData1Name);
  entry1->set_number(kProviderData1Number);
  KeyProviderData::Entry* entry2 = data->add_entry();
  entry2->set_name(kProviderData2Name);
  entry2->set_bytes(kProviderData2Bytes);
  const base::Optional<BaseReply> reply = std::move(result);

  std::vector<KeyDefinition> key_definitions =
      GetKeyDataReplyToKeyDefinitions(reply);

  // Verify that the call was successful and the result was correctly parsed.
  ASSERT_EQ(1u, key_definitions.size());
  const KeyDefinition& key_definition = key_definitions.front();
  EXPECT_EQ(KeyDefinition::TYPE_PASSWORD, key_definition.type);
  EXPECT_EQ(kKeyLabel, key_definition.label);
  EXPECT_EQ(PRIV_MOUNT | PRIV_ADD | PRIV_REMOVE, key_definition.privileges);
  EXPECT_EQ(kKeyRevision, key_definition.revision);
  ASSERT_EQ(1u, key_definition.authorization_data.size());
  EXPECT_EQ(KeyDefinition::AuthorizationData::TYPE_HMACSHA256,
            key_definition.authorization_data.front().type);
  ASSERT_EQ(2u, key_definition.provider_data.size());
  const KeyDefinition::ProviderData* provider_data =
      &key_definition.provider_data[0];
  EXPECT_EQ(kProviderData1Name, provider_data->name);
  ASSERT_TRUE(provider_data->number);
  EXPECT_EQ(kProviderData1Number, *provider_data->number.get());
  EXPECT_FALSE(provider_data->bytes);
  provider_data = &key_definition.provider_data[1];
  EXPECT_EQ(kProviderData2Name, provider_data->name);
  EXPECT_FALSE(provider_data->number);
  ASSERT_TRUE(provider_data->bytes);
  EXPECT_EQ(kProviderData2Bytes, *provider_data->bytes.get());
}

// Test the GetKeyDataReplyToKeyDefinitions() function against the BaseReply
// proto containing the KeyData proto of the |KEY_TYPE_CHALLENGE_RESPONSE| type.
TEST(CryptohomeUtilTest, GetKeyDataReplyToKeyDefinitions_ChallengeResponse) {
  BaseReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  GetKeyDataReply* get_key_data_reply =
      result.MutableExtension(GetKeyDataReply::reply);
  KeyData* key_data = get_key_data_reply->add_key_data();
  key_data->set_type(KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  key_data->set_label(kKeyLabel);
  const base::Optional<BaseReply> reply = std::move(result);

  const std::vector<KeyDefinition> key_definitions =
      GetKeyDataReplyToKeyDefinitions(reply);

  ASSERT_EQ(1u, key_definitions.size());
  const KeyDefinition& key_definition = key_definitions.front();
  EXPECT_EQ(KeyDefinition::TYPE_CHALLENGE_RESPONSE, key_definition.type);
  EXPECT_EQ(kKeyLabel, key_definition.label);
}

}  // namespace cryptohome
