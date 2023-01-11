// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/easy_unlock/fake_easy_unlock_client.h"

#include <string>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Callback for |GenerateEcP256KeyPair| method. Saves keys returned by the
// method in |private_key_target| and |public_key_target|.
void RecordKeyPair(std::string* private_key_target,
                   std::string* public_key_target,
                   const std::string& private_key_source,
                   const std::string& public_key_source) {
  *private_key_target = private_key_source;
  *public_key_target = public_key_source;
}

// Callback for |EasyUnlockClient| methods that return a single piece of data.
// It saves the returned data in |data_target|.
void RecordData(std::string* data_target, const std::string& data_source) {
  *data_target = data_source;
}

TEST(FakeEasyUnlockClientTest, GenerateEcP256KeyPair) {
  ash::FakeEasyUnlockClient client;

  std::string private_key_1;
  std::string public_key_1;
  client.GenerateEcP256KeyPair(
      base::BindOnce(&RecordKeyPair, &private_key_1, &public_key_1));
  ASSERT_EQ("{\"ec_p256_private_key\": 1}", private_key_1);
  ASSERT_EQ("{\"ec_p256_public_key\": 1}", public_key_1);

  std::string private_key_2;
  std::string public_key_2;
  client.GenerateEcP256KeyPair(
      base::BindOnce(&RecordKeyPair, &private_key_2, &public_key_2));
  ASSERT_EQ("{\"ec_p256_private_key\": 2}", private_key_2);
  ASSERT_EQ("{\"ec_p256_public_key\": 2}", public_key_2);

  EXPECT_NE(private_key_1, private_key_2);
  EXPECT_NE(public_key_1, public_key_2);
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair) {
  ASSERT_TRUE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_private_key\": 12}", "{\"ec_p256_public_key\": 12}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_KeysFromDiffrentPairs) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_private_key\": 12}", "{\"ec_p256_public_key\": 34}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_KeyOrderSwitched) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_public_key\": 34}", "{\"ec_p256_private_key\": 34}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_PrivateKeyInvalidFormat) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "\"ec_p256_private_key\": 12", "{\"ec_p256_public_key\": 12}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_PublicKeyInvalidFormat) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_private_key\": 12}", "\"ec_p256_public_key\": 12"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_PrivateKeyInvalidDictKey) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"invalid\": 12}", "{\"ec_p256_public_key\": 12}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_PublicKeyInvalidDictKey) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_private_key\": 12}", "{\"invalid\": 12}"));
}

TEST(FakeEasyUnlockClientTest, IsEcP256KeyPair_InvalidDictValues) {
  ASSERT_FALSE(ash::FakeEasyUnlockClient::IsEcP256KeyPair(
      "{\"ec_p256_private_key\": \"12\"}", "{\"ec_p256_public_key\": \"12\"}"));
}

// Verifies the fake |PerformECDHKeyAgreement| method is symetric in respect to
// key pairs from which private and public key used in the key agreement
// originate.
TEST(FakeEasyUnlockClientTest, ECDHKeyAgreementSuccess) {
  ash::FakeEasyUnlockClient client;

  // (Fake) key pairs used in the test to generate fake shared keys.
  const std::string private_key_1 = "{\"ec_p256_private_key\": 32}";
  const std::string public_key_1 = "{\"ec_p256_public_key\": 32}";

  const std::string private_key_2 = "{\"ec_p256_private_key\": 352}";
  const std::string public_key_2 = "{\"ec_p256_public_key\": 352}";

  const std::string private_key_3 = "{\"ec_p256_private_key\": 432}";
  const std::string public_key_3 = "{\"ec_p256_public_key\": 432}";

  // Generate shared key for key pairs 1 and 2, using private key from the
  // second key pair and public key from the first key pair.
  std::string shared_key_1;
  client.PerformECDHKeyAgreement(private_key_2, public_key_1,
                                 base::BindOnce(&RecordData, &shared_key_1));
  EXPECT_FALSE(shared_key_1.empty());

  // Generate shared key for key pairs 1 and 2, using private key from the
  // first key pair and public key from the second key pair.
  std::string shared_key_2;
  client.PerformECDHKeyAgreement(private_key_1, public_key_2,
                                 base::BindOnce(&RecordData, &shared_key_2));
  EXPECT_FALSE(shared_key_2.empty());

  // The generated keys should be equal. They were generated using keys from
  // the same key pairs, even though key pairs from which private and public key
  // originate were switched.
  EXPECT_EQ(shared_key_1, shared_key_2);

  // Generate a key using key pairs 1 and 3.
  std::string shared_key_3;
  client.PerformECDHKeyAgreement(private_key_1, public_key_3,
                                 base::BindOnce(&RecordData, &shared_key_3));
  EXPECT_FALSE(shared_key_3.empty());

  // The new key should be different from the previously generated ones, since
  // the used key pairs are different.
  EXPECT_NE(shared_key_3, shared_key_1);
  EXPECT_NE(shared_key_3, shared_key_1);
}

TEST(FakeEasyUnlockClientTest, ECDHKeyAgreementFailsIfKeyOrderSwitched) {
  ash::FakeEasyUnlockClient client;

  const std::string private_key = "{\"ec_p256_private_key\": 415}";
  const std::string public_key = "{\"ec_p256_public_key\": 345}";

  std::string shared_key;
  client.PerformECDHKeyAgreement(public_key, private_key,
                                 base::BindOnce(&RecordData, &shared_key));
  EXPECT_TRUE(shared_key.empty());
}

TEST(FakeEasyUnlockClientTest, ECDHKeyAgreementFailsIfKeyDictKeyInvalid) {
  ash::FakeEasyUnlockClient client;

  const std::string private_key = "{\"ec_p256_private_key_invalid\": 415}";
  const std::string public_key = "{\"ec_p256_public_key_invalid\": 345}";

  std::string shared_key;
  client.PerformECDHKeyAgreement(private_key, public_key,
                                 base::BindOnce(&RecordData, &shared_key));
  EXPECT_TRUE(shared_key.empty());
}

TEST(FakeEasyUnlockClientTest, ECDHKeyAgreementFailsIfKeyDictValueInvalid) {
  ash::FakeEasyUnlockClient client;

  const std::string private_key = "{\"ec_p256_private_key\": 415}";
  const std::string public_key = "{\"ec_p256_public_key\": \"345__\"}";

  std::string shared_key;
  client.PerformECDHKeyAgreement(private_key, public_key,
                                 base::BindOnce(&RecordData, &shared_key));
  EXPECT_TRUE(shared_key.empty());
}

TEST(FakeEasyUnlockClientTest, ECDHKeyAgreementFailsIfKeyFormatInvalid) {
  ash::FakeEasyUnlockClient client;

  const std::string private_key = "invalid";
  const std::string public_key = "{\"ec_p256_public_key\": 345}";

  std::string shared_key;
  client.PerformECDHKeyAgreement(private_key, public_key,
                                 base::BindOnce(&RecordData, &shared_key));
  EXPECT_TRUE(shared_key.empty());
}

TEST(FakeEasyUnlockClientTest, CreateSecureMessage) {
  ash::FakeEasyUnlockClient client;

  std::string message;

  ash::EasyUnlockClient::CreateSecureMessageOptions options;
  options.key = "KEY";
  options.associated_data = "ASSOCIATED_DATA";
  options.public_metadata = "PUBLIC_METADATA";
  options.verification_key_id = "VERIFICATION_KEY_ID";
  options.decryption_key_id = "DECRYPTION_KEY_ID";
  options.encryption_type = "ENCRYPTION_TYPE";
  options.signature_type = "SIGNATURE_TYPE";

  client.CreateSecureMessage("PAYLOAD", options,
                             base::BindOnce(&RecordData, &message));

  const std::string expected_message(
      "{\"securemessage\": {"
      "\"payload\": \"PAYLOAD\","
      "\"key\": \"KEY\","
      "\"associated_data\": \"ASSOCIATED_DATA\","
      "\"public_metadata\": \"PUBLIC_METADATA\","
      "\"verification_key_id\": \"VERIFICATION_KEY_ID\","
      "\"decryption_key_id\": \"DECRYPTION_KEY_ID\","
      "\"encryption_type\": \"ENCRYPTION_TYPE\","
      "\"signature_type\": \"SIGNATURE_TYPE\"}"
      "}");
  ASSERT_EQ(expected_message, message);
}

TEST(FakeEasyUnlockClientTest, UnwrapSecureMessage) {
  ash::FakeEasyUnlockClient client;

  std::string message;

  ash::EasyUnlockClient::UnwrapSecureMessageOptions options;
  options.key = "KEY";
  options.associated_data = "ASSOCIATED_DATA";
  options.encryption_type = "ENCRYPTION_TYPE";
  options.signature_type = "SIGNATURE_TYPE";

  client.UnwrapSecureMessage("MESSAGE", options,
                             base::BindOnce(&RecordData, &message));

  const std::string expected_message(
      "{\"unwrapped_securemessage\": {"
      "\"message\": \"MESSAGE\","
      "\"key\": \"KEY\","
      "\"associated_data\": \"ASSOCIATED_DATA\","
      "\"encryption_type\": \"ENCRYPTION_TYPE\","
      "\"signature_type\": \"SIGNATURE_TYPE\"}"
      "}");
  ASSERT_EQ(expected_message, message);
}

}  // namespace
