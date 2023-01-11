// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::multidevice {

namespace {

const char kTestPublicKey[] = "the private key is in another castle";
const char kPayload[] = "500 tons of uranium";
const char kSymmetricKey[] = "hunter2";
const char kPublicMetadata[] = "brought to you by our sponsors";
const char kAssociatedData[] = "save 20% bytes on your nonce insurance";
const char kVerificationKeyId[] = "the one with the red stripes";
const char kDecryptionKeyId[] = "it's in your pocket somewhere";

// Callback for saving the result of GenerateKeys().
void SaveKeyPair(std::string* private_key_out,
                 std::string* public_key_out,
                 const std::string& private_key,
                 const std::string& public_key) {
  *private_key_out = private_key;
  *public_key_out = public_key;
}

// Callback for saving the result of DeriveKey() and CreateSecureMessage().
void SaveString(std::string* out, const std::string& value) {
  *out = value;
}

// Callback for saving the result of UnwrapSecureMessage().
void SaveUnwrapResults(std::string* payload_out,
                       securemessage::Header* header_out,
                       bool verified,
                       const std::string& payload,
                       const securemessage::Header& header) {
  ASSERT_TRUE(verified);
  *payload_out = payload;
  *header_out = header;
}

// Returns the CreateOptions struct to create the test message.
SecureMessageDelegate::CreateOptions GetCreateOptions(
    securemessage::EncScheme encryption_scheme,
    securemessage::SigScheme signature_scheme) {
  SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = encryption_scheme;
  create_options.signature_scheme = signature_scheme;
  create_options.public_metadata = kPublicMetadata;
  create_options.associated_data = kAssociatedData;
  create_options.verification_key_id = kVerificationKeyId;
  create_options.decryption_key_id = kDecryptionKeyId;
  return create_options;
}

// Returns the UnwrapOptions struct to unwrap the test message.
SecureMessageDelegate::UnwrapOptions GetUnwrapOptions(
    securemessage::EncScheme encryption_scheme,
    securemessage::SigScheme signature_scheme) {
  SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = encryption_scheme;
  unwrap_options.signature_scheme = signature_scheme;
  unwrap_options.associated_data = kAssociatedData;
  return unwrap_options;
}

void CheckSerializedSecureMessage(
    const std::string& serialized_message,
    const SecureMessageDelegate::CreateOptions& create_options) {
  securemessage::SecureMessage secure_message;
  ASSERT_TRUE(secure_message.ParseFromString(serialized_message));
  securemessage::HeaderAndBody header_and_body;
  ASSERT_TRUE(
      header_and_body.ParseFromString(secure_message.header_and_body()));

  const securemessage::Header& header = header_and_body.header();
  EXPECT_EQ(create_options.signature_scheme, header.signature_scheme());
  EXPECT_EQ(create_options.encryption_scheme, header.encryption_scheme());
  EXPECT_EQ(create_options.verification_key_id, header.verification_key_id());
  EXPECT_EQ(create_options.decryption_key_id, header.decryption_key_id());
  EXPECT_EQ(create_options.public_metadata, header.public_metadata());
}

}  // namespace

class CryptAuthFakeSecureMessageDelegateTest : public testing::Test {
 public:
  CryptAuthFakeSecureMessageDelegateTest(
      const CryptAuthFakeSecureMessageDelegateTest&) = delete;
  CryptAuthFakeSecureMessageDelegateTest& operator=(
      const CryptAuthFakeSecureMessageDelegateTest&) = delete;

 protected:
  CryptAuthFakeSecureMessageDelegateTest() {}

  FakeSecureMessageDelegate delegate_;
};

TEST_F(CryptAuthFakeSecureMessageDelegateTest, GenerateKeyPair) {
  std::string public_key1, private_key1;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key1, &private_key1));
  EXPECT_NE(private_key1, public_key1);

  std::string public_key2, private_key2;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key2, &private_key2));
  EXPECT_NE(private_key2, public_key2);

  EXPECT_NE(public_key1, public_key2);
  EXPECT_NE(private_key1, private_key2);

  delegate_.set_next_public_key(kTestPublicKey);
  std::string public_key3, private_key3;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key3, &private_key3));
  EXPECT_EQ(kTestPublicKey, public_key3);
  EXPECT_NE(private_key3, public_key3);

  EXPECT_NE(public_key1, public_key3);
  EXPECT_NE(private_key1, private_key3);
}

TEST_F(CryptAuthFakeSecureMessageDelegateTest, DeriveKey) {
  delegate_.set_next_public_key("key_pair_1");
  std::string public_key1, private_key1;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key1, &private_key1));

  delegate_.set_next_public_key("key_pair_2");
  std::string public_key2, private_key2;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key2, &private_key2));

  std::string symmetric_key1, symmetric_key2;
  delegate_.DeriveKey(private_key1, public_key2,
                      base::BindOnce(&SaveString, &symmetric_key1));
  delegate_.DeriveKey(private_key2, public_key1,
                      base::BindOnce(&SaveString, &symmetric_key2));

  EXPECT_EQ(symmetric_key1, symmetric_key2);
}

TEST_F(CryptAuthFakeSecureMessageDelegateTest,
       CreateAndUnwrapWithSymmetricKey) {
  // Create SecureMessage using symmetric key.
  SecureMessageDelegate::CreateOptions create_options =
      GetCreateOptions(securemessage::AES_256_CBC, securemessage::HMAC_SHA256);
  std::string serialized_message;
  delegate_.CreateSecureMessage(
      kPayload, kSymmetricKey, create_options,
      base::BindOnce(&SaveString, &serialized_message));

  CheckSerializedSecureMessage(serialized_message, create_options);

  // Unwrap SecureMessage using symmetric key.
  SecureMessageDelegate::UnwrapOptions unwrap_options =
      GetUnwrapOptions(securemessage::AES_256_CBC, securemessage::HMAC_SHA256);
  std::string payload;
  securemessage::Header header;
  delegate_.UnwrapSecureMessage(
      serialized_message, kSymmetricKey, unwrap_options,
      base::BindOnce(&SaveUnwrapResults, &payload, &header));

  EXPECT_EQ(kPayload, payload);
}

TEST_F(CryptAuthFakeSecureMessageDelegateTest,
       CreateAndUnwrapWithAsymmetricKey) {
  delegate_.set_next_public_key(kTestPublicKey);
  std::string public_key, private_key;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key, &private_key));

  // Create SecureMessage using asymmetric key.
  SecureMessageDelegate::CreateOptions create_options =
      GetCreateOptions(securemessage::NONE, securemessage::ECDSA_P256_SHA256);
  std::string serialized_message;
  delegate_.CreateSecureMessage(
      kPayload, private_key, create_options,
      base::BindOnce(&SaveString, &serialized_message));

  CheckSerializedSecureMessage(serialized_message, create_options);

  // Unwrap SecureMessage using symmetric key.
  SecureMessageDelegate::UnwrapOptions unwrap_options =
      GetUnwrapOptions(securemessage::NONE, securemessage::ECDSA_P256_SHA256);
  std::string payload;
  securemessage::Header header;
  delegate_.UnwrapSecureMessage(
      serialized_message, public_key, unwrap_options,
      base::BindOnce(&SaveUnwrapResults, &payload, &header));

  EXPECT_EQ(kPayload, payload);
}

TEST_F(CryptAuthFakeSecureMessageDelegateTest, GetPrivateKeyForPublicKey) {
  delegate_.set_next_public_key(kTestPublicKey);
  std::string public_key, private_key;
  delegate_.GenerateKeyPair(
      base::BindOnce(&SaveKeyPair, &public_key, &private_key));
  EXPECT_EQ(kTestPublicKey, public_key);
  EXPECT_EQ(private_key, delegate_.GetPrivateKeyForPublicKey(kTestPublicKey));
}

}  // namespace ash::multidevice
