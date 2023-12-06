// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash {

namespace device_sync {

namespace {

const char kSessionPublicKey[] = "session_public_key";
const char kPayloadId1[] = "payload_id_1";
const char kPayloadId2[] = "payload_id_2";
const char kUnencryptedPayload1[] = "unencrypted_payload_1";
const char kUnencryptedPayload2[] = "unencrypted_payload_2";
const char kPublicKey1[] = "public_key_1";
const char kPublicKey2[] = "public_key_2";
const char kPayloadNotSet[] = "[Payload not set]";

constexpr securemessage::EncScheme kSecureMessageEncryptionScheme =
    securemessage::AES_256_CBC;
constexpr securemessage::SigScheme kSecureMessageSignatureScheme =
    securemessage::HMAC_SHA256;

// Verifies that the encrypted payload is a valid SecureMessage with the
// expected Header parameters.
void VerifyEncryptedPayload(
    const std::string& expected_session_public_key,
    const std::optional<std::string>& encrypted_payload) {
  ASSERT_TRUE(encrypted_payload);
  EXPECT_FALSE(encrypted_payload->empty());
  EXPECT_NE(kPayloadNotSet, encrypted_payload);

  securemessage::SecureMessage secure_message;
  EXPECT_TRUE(secure_message.ParseFromString(*encrypted_payload));

  securemessage::HeaderAndBody header_and_body;
  EXPECT_TRUE(
      header_and_body.ParseFromString(secure_message.header_and_body()));

  const securemessage::Header& header = header_and_body.header();
  EXPECT_EQ(kSecureMessageEncryptionScheme, header.encryption_scheme());
  EXPECT_EQ(kSecureMessageSignatureScheme, header.signature_scheme());
  EXPECT_EQ(expected_session_public_key, header.decryption_key_id());
}

}  // namespace

class DeviceSyncCryptAuthEciesEncryptorImplTest : public testing::Test {
 public:
  DeviceSyncCryptAuthEciesEncryptorImplTest(
      const DeviceSyncCryptAuthEciesEncryptorImplTest&) = delete;
  DeviceSyncCryptAuthEciesEncryptorImplTest& operator=(
      const DeviceSyncCryptAuthEciesEncryptorImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthEciesEncryptorImplTest()
      : fake_secure_message_delegate_factory_(
            std::make_unique<multidevice::FakeSecureMessageDelegateFactory>()) {
  }

  ~DeviceSyncCryptAuthEciesEncryptorImplTest() override = default;

  void SetUp() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
        fake_secure_message_delegate_factory_.get());
  }

  void Encrypt(const std::string& unencrypted_payload,
               const std::string& encrypting_public_key) {
    encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
    fake_secure_message_delegate()->set_next_public_key(kSessionPublicKey);

    base::RunLoop run_loop;
    encryptor_->Encrypt(
        unencrypted_payload, encrypting_public_key,
        base::BindOnce(
            &DeviceSyncCryptAuthEciesEncryptorImplTest::OnEncryptionFinished,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    VerifyEncryption(kSessionPublicKey);
    encryptor_.reset();
  }

  void BatchEncrypt(const CryptAuthEciesEncryptor::IdToInputMap&
                        id_to_unencrypted_payload_map) {
    encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
    fake_secure_message_delegate()->set_next_public_key(kSessionPublicKey);

    base::RunLoop run_loop;
    encryptor_->BatchEncrypt(
        id_to_unencrypted_payload_map,
        base::BindOnce(&DeviceSyncCryptAuthEciesEncryptorImplTest::
                           OnBatchEncryptionFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    VerifyBatchEncryption(kSessionPublicKey, id_to_unencrypted_payload_map);
    encryptor_.reset();
  }

  void Decrypt(const std::string& encrypted_payload,
               const std::string& decrypting_private_key) {
    encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();

    base::RunLoop run_loop;
    encryptor_->Decrypt(
        encrypted_payload, decrypting_private_key,
        base::BindOnce(
            &DeviceSyncCryptAuthEciesEncryptorImplTest::OnDecryptionFinished,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void BatchDecrypt(const CryptAuthEciesEncryptor::IdToInputMap&
                        id_to_encrypted_payload_map) {
    encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();

    base::RunLoop run_loop;
    encryptor_->BatchDecrypt(
        id_to_encrypted_payload_map,
        base::BindOnce(&DeviceSyncCryptAuthEciesEncryptorImplTest::
                           OnBatchDecryptionFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void VerifyDecryption(
      const std::optional<std::string>& expected_decrypted_payload) {
    EXPECT_EQ(expected_decrypted_payload, decrypted_payload_);
  }

  void VerifyBatchDecryption(const CryptAuthEciesEncryptor::IdToOutputMap&
                                 expected_batch_decrypted_payloads) {
    EXPECT_EQ(expected_batch_decrypted_payloads, batch_decrypted_payloads_);
  }

  // Replaces the unencrypted payloads and encrypting public keys with the
  // corresponding encrypted payloads and decrypting private keys.
  CryptAuthEciesEncryptor::IdToInputMap
  ConvertBatchEncryptInputToBatchDecryptInput(
      const CryptAuthEciesEncryptor::IdToInputMap&
          id_to_unencrypted_payload_and_public_key_map) {
    EXPECT_TRUE(batch_encrypted_payloads_);

    CryptAuthEciesEncryptor::IdToInputMap
        id_to_encrypted_payload_and_private_key_map =
            id_to_unencrypted_payload_and_public_key_map;
    for (auto& id_pk_pair : id_to_encrypted_payload_and_private_key_map) {
      const std::optional<std::string>& encrypted_payload =
          (*batch_encrypted_payloads_)[id_pk_pair.first];
      EXPECT_TRUE(encrypted_payload);

      id_pk_pair.second = CryptAuthEciesEncryptor::PayloadAndKey(
          *encrypted_payload,
          fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
              id_pk_pair.second.key));
    }

    return id_to_encrypted_payload_and_private_key_map;
  }

  const std::optional<std::string>& encrypted_payload() {
    return encrypted_payload_;
  }

  multidevice::FakeSecureMessageDelegate* fake_secure_message_delegate() {
    return fake_secure_message_delegate_factory_->instance();
  }

 private:
  void OnEncryptionFinished(
      base::OnceClosure quit_closure,
      const std::optional<std::string>& encrypted_payload) {
    encrypted_payload_ = encrypted_payload;
    std::move(quit_closure).Run();
  }

  void OnBatchEncryptionFinished(
      base::OnceClosure quit_closure,
      const CryptAuthEciesEncryptor::IdToOutputMap& batch_encrypted_payloads) {
    batch_encrypted_payloads_ = batch_encrypted_payloads;
    std::move(quit_closure).Run();
  }

  void OnDecryptionFinished(
      base::OnceClosure quit_closure,
      const std::optional<std::string>& decrypted_payload) {
    decrypted_payload_ = decrypted_payload;
    std::move(quit_closure).Run();
  }

  void OnBatchDecryptionFinished(
      base::OnceClosure quit_closure,
      const CryptAuthEciesEncryptor::IdToOutputMap& batch_decrypted_payloads) {
    batch_decrypted_payloads_ = batch_decrypted_payloads;
    std::move(quit_closure).Run();
  }

  void VerifyEncryption(const std::string& expected_session_public_key) {
    VerifyEncryptedPayload(expected_session_public_key, encrypted_payload_);
  }

  void VerifyBatchEncryption(const std::string& expected_session_public_key,
                             const CryptAuthEciesEncryptor::IdToInputMap&
                                 input_id_to_unencrypted_payload_map) {
    ASSERT_TRUE(batch_encrypted_payloads_);

    for (const auto& id_output_pair : *batch_encrypted_payloads_) {
      EXPECT_TRUE(base::Contains(input_id_to_unencrypted_payload_map,
                                 id_output_pair.first));
      VerifyEncryptedPayload(expected_session_public_key,
                             id_output_pair.second);
    }
  }

  const base::test::TaskEnvironment task_environment_;

  std::unique_ptr<CryptAuthEciesEncryptor> encryptor_;
  std::optional<std::string> encrypted_payload_ = kPayloadNotSet;
  std::optional<std::string> decrypted_payload_ = kPayloadNotSet;
  std::optional<CryptAuthEciesEncryptor::IdToOutputMap>
      batch_encrypted_payloads_;
  std::optional<CryptAuthEciesEncryptor::IdToOutputMap>
      batch_decrypted_payloads_;

  std::unique_ptr<multidevice::FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;
};

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest, EncryptAndDecrypt) {
  Encrypt(kUnencryptedPayload1, kPublicKey1);

  Decrypt(
      *encrypted_payload(),
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(kPublicKey1));
  VerifyDecryption(kUnencryptedPayload1);
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest, BatchEncryptAndDecrypt) {
  const CryptAuthEciesEncryptor::IdToInputMap unencrypted_input = {
      {kPayloadId1, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload1,
                                                           kPublicKey1)},
      {kPayloadId2, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload2,
                                                           kPublicKey2)}};
  BatchEncrypt(unencrypted_input);

  BatchDecrypt(ConvertBatchEncryptInputToBatchDecryptInput(unencrypted_input));
  VerifyBatchDecryption({{kPayloadId1, kUnencryptedPayload1},
                         {kPayloadId2, kUnencryptedPayload2}});
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest,
       DecryptionFailure_WrongDecryptionKey) {
  Encrypt(kUnencryptedPayload1, kPublicKey1);

  Decrypt(*encrypted_payload(), "Invalid private key");
  VerifyDecryption(std::nullopt /* expected_decrypted_payload */);
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest,
       DecryptionFailure_CannotParseSecureMessage) {
  const CryptAuthEciesEncryptor::IdToInputMap unencrypted_input = {
      {kPayloadId1, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload1,
                                                           kPublicKey1)},
      {kPayloadId2, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload2,
                                                           kPublicKey2)}};

  BatchEncrypt(unencrypted_input);
  CryptAuthEciesEncryptor::IdToInputMap encrypted_input =
      ConvertBatchEncryptInputToBatchDecryptInput(unencrypted_input);

  // Corrupt the second serialized SecureMessage.
  encrypted_input[kPayloadId2].payload = "Invalid SecureMessage";

  BatchDecrypt(encrypted_input);
  VerifyBatchDecryption(
      {{kPayloadId1, kUnencryptedPayload1}, {kPayloadId2, std::nullopt}});
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest,
       DecryptionFailure_CannotParseSecureMessageHeaderAndBody) {
  Encrypt(kUnencryptedPayload1, kPublicKey1);

  // Corrupt the HeaderAndBody.
  securemessage::SecureMessage secure_message_with_invalid_header_and_body;
  secure_message_with_invalid_header_and_body.ParseFromString(
      *encrypted_payload());
  secure_message_with_invalid_header_and_body.set_header_and_body(
      "Invalid HeaderAndBody");

  Decrypt(
      secure_message_with_invalid_header_and_body.SerializeAsString(),
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(kPublicKey1));
  VerifyDecryption(std::nullopt /* expected_decrypted_payload */);
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest,
       DecryptionFailure_InvalidSchemesInHeader) {
  const CryptAuthEciesEncryptor::IdToInputMap unencrypted_input = {
      {kPayloadId1, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload1,
                                                           kPublicKey1)},
      {kPayloadId2, CryptAuthEciesEncryptor::PayloadAndKey(kUnencryptedPayload2,
                                                           kPublicKey2)}};
  BatchEncrypt(unencrypted_input);
  CryptAuthEciesEncryptor::IdToInputMap encrypted_input =
      ConvertBatchEncryptInputToBatchDecryptInput(unencrypted_input);

  // Corrupt the specified encryption scheme of the first SecureMessage.
  {
    securemessage::SecureMessage secure_message;
    secure_message.ParseFromString(encrypted_input[kPayloadId1].payload);
    securemessage::HeaderAndBody header_and_body;
    header_and_body.ParseFromString(secure_message.header_and_body());
    header_and_body.mutable_header()->set_encryption_scheme(
        securemessage::EncScheme::NONE);
    secure_message.set_header_and_body(header_and_body.SerializeAsString());
    encrypted_input[kPayloadId1].payload = secure_message.SerializeAsString();
  }

  // Corrupt the specified signature scheme of the second SecureMessage.
  {
    securemessage::SecureMessage secure_message;
    secure_message.ParseFromString(encrypted_input[kPayloadId2].payload);
    securemessage::HeaderAndBody header_and_body;
    header_and_body.ParseFromString(secure_message.header_and_body());
    header_and_body.mutable_header()->set_signature_scheme(
        securemessage::SigScheme::RSA2048_SHA256);
    secure_message.set_header_and_body(header_and_body.SerializeAsString());
    encrypted_input[kPayloadId2].payload = secure_message.SerializeAsString();
  }

  BatchDecrypt(encrypted_input);
  VerifyBatchDecryption(
      {{kPayloadId1, std::nullopt}, {kPayloadId2, std::nullopt}});
}

TEST_F(DeviceSyncCryptAuthEciesEncryptorImplTest,
       DecryptionFailure_EmptyDecryptionKeyId) {
  Encrypt(kUnencryptedPayload1, kPublicKey1);

  // Remove the session public key.
  securemessage::SecureMessage secure_message;
  secure_message.ParseFromString(*encrypted_payload());
  securemessage::HeaderAndBody header_and_body;
  header_and_body.ParseFromString(secure_message.header_and_body());
  header_and_body.mutable_header()->set_decryption_key_id(std::string());
  secure_message.set_header_and_body(header_and_body.SerializeAsString());

  Decrypt(
      secure_message.SerializeAsString(),
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(kPublicKey1));
  VerifyDecryption(std::nullopt /* expected_decrypted_payload */);
}

}  // namespace device_sync

}  // namespace ash
