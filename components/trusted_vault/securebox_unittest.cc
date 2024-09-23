// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/trusted_vault/securebox.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;
using testing::IsEmpty;
using testing::Ne;
using testing::NotNull;
using testing::SizeIs;

std::vector<uint8_t> StringToBytes(std::string_view str) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>(raw_data, raw_data + str.length());
}

class SecureBoxTest : public testing::Test {
 public:
  const size_t kPublicKeyLengthInBytes = 65;
  const size_t kPrivateKeyLengthInBytes = 32;
  const std::vector<uint8_t> kTestSharedSecret =
      StringToBytes("TEST_SHARED_SECRET");
  const std::vector<uint8_t> kTestHeader = StringToBytes("TEST_HEADER");
  const std::vector<uint8_t> kTestPayload = StringToBytes("TEST_PAYLOAD");
};

TEST_F(SecureBoxTest, ShouldExportAndImportPublicKey) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> exported_public_key =
      key_pair->public_key().ExportToBytes();
  EXPECT_THAT(exported_public_key, SizeIs(kPublicKeyLengthInBytes));

  std::unique_ptr<SecureBoxPublicKey> imported_public_key =
      SecureBoxPublicKey::CreateByImport(exported_public_key);
  EXPECT_THAT(imported_public_key, NotNull());
  EXPECT_THAT(imported_public_key->ExportToBytes(), Eq(exported_public_key));
}

TEST_F(SecureBoxTest, ShouldExportAndImportPrivateKey) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> exported_private_key =
      key_pair->private_key().ExportToBytes();
  EXPECT_THAT(exported_private_key, SizeIs(kPrivateKeyLengthInBytes));

  std::unique_ptr<SecureBoxPrivateKey> imported_private_key =
      SecureBoxPrivateKey::CreateByImport(exported_private_key);
  ASSERT_THAT(imported_private_key, NotNull());
  EXPECT_THAT(imported_private_key->ExportToBytes(), Eq(exported_private_key));
}

TEST_F(SecureBoxTest, ShouldExportPrivateKeyAndImportKeyPair) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> exported_private_key =
      key_pair->private_key().ExportToBytes();
  std::unique_ptr<SecureBoxKeyPair> imported_key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(exported_private_key);
  ASSERT_THAT(imported_key_pair, NotNull());
  EXPECT_THAT(imported_key_pair->private_key().ExportToBytes(),
              Eq(exported_private_key));
  EXPECT_THAT(imported_key_pair->public_key().ExportToBytes(),
              Eq(key_pair->public_key().ExportToBytes()));
}

TEST_F(SecureBoxTest, ShouldGenerateDifferentKeys) {
  std::unique_ptr<SecureBoxKeyPair> key_pair_a =
      SecureBoxKeyPair::GenerateRandom();
  std::unique_ptr<SecureBoxKeyPair> key_pair_b =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair_a, NotNull());
  ASSERT_THAT(key_pair_b, NotNull());

  EXPECT_THAT(key_pair_a->public_key().ExportToBytes(),
              Ne(key_pair_b->public_key().ExportToBytes()));
  EXPECT_THAT(key_pair_a->private_key().ExportToBytes(),
              Ne(key_pair_b->private_key().ExportToBytes()));
}

TEST_F(SecureBoxTest, ShouldEncryptThenDecrypt) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> encrypted = key_pair->public_key().Encrypt(
      kTestSharedSecret, kTestHeader, kTestPayload);

  std::optional<std::vector<uint8_t>> decrypted =
      key_pair->private_key().Decrypt(kTestSharedSecret, kTestHeader,
                                      encrypted);
  ASSERT_THAT(decrypted, Ne(std::nullopt));
  EXPECT_THAT(*decrypted, Eq(kTestPayload));
}

TEST_F(SecureBoxTest, ShouldEncryptThenDecryptWithEmptySharedSecret) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> encrypted = key_pair->public_key().Encrypt(
      /*shared_secret=*/base::span<uint8_t>(), kTestHeader, kTestPayload);

  std::optional<std::vector<uint8_t>> decrypted =
      key_pair->private_key().Decrypt(/*shared_secret=*/base::span<uint8_t>(),
                                      kTestHeader, encrypted);
  ASSERT_THAT(decrypted, Ne(std::nullopt));
  EXPECT_THAT(*decrypted, Eq(kTestPayload));
}

TEST_F(SecureBoxTest, ShouldEncryptThenDecryptWithEmptyHeader) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> encrypted = key_pair->public_key().Encrypt(
      kTestSharedSecret, /*header=*/base::span<uint8_t>(), kTestPayload);

  std::optional<std::vector<uint8_t>> decrypted =
      key_pair->private_key().Decrypt(
          kTestSharedSecret, /*header=*/base::span<uint8_t>(), encrypted);
  ASSERT_THAT(decrypted, Ne(std::nullopt));
  EXPECT_THAT(*decrypted, Eq(kTestPayload));
}

TEST_F(SecureBoxTest, ShouldEncryptThenDecryptWithEmptyPayload) {
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  ASSERT_THAT(key_pair, NotNull());

  std::vector<uint8_t> encrypted = key_pair->public_key().Encrypt(
      kTestSharedSecret, kTestHeader, /*payload=*/base::span<uint8_t>());

  std::optional<std::vector<uint8_t>> decrypted =
      key_pair->private_key().Decrypt(kTestSharedSecret, kTestHeader,
                                      encrypted);
  ASSERT_THAT(decrypted, Ne(std::nullopt));
  EXPECT_THAT(*decrypted, IsEmpty());
}

TEST_F(SecureBoxTest, ShouldDecryptTestVectors) {
  struct TestVector {
    std::string private_key;
    std::string shared_secret;
    std::string header;
    std::string payload;
    std::string encrypted_payload;
  };

  const std::vector<TestVector> kTestVectors = {
      {/*private_key=*/
       "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f",
       /*shared_secret=*/"efbaea750bb3e8190e8bb0b6df9f1382",
       /*header=*/"6430055109c9c1853ab0d11e",
       /*payload=*/
       "9f2648ea719ee452bd4f4e9e4b19df75de6c028c8bd3a385",
       /*encrypted_payload=*/
       "020004c4c803b44a189adac641bb04ed0073d352de5e2cfdba935a88e33c5f39f26d4d"
       "8c7e87e4dd9322491ac401f92d3336560d181629017bd58e4884ea25e44423ec75a889"
       "b43e4ea48f46864fc863430459dc241d7acef4042255eed8c4fbf9a71cde4ef4d650d5"
       "72f8a22f67a73751f4a1b4dad0fc"},
      {/*private_key=*/
       "2818d19cc43b873d94e50d9e4cd07dac8814c0597c6b11866350f1e17aeb87c3",
       /*shared_secret=*/"6689c9b65a6723c419b818c03340b3ce",
       /*header=*/"278a6d38639793e2dcdae738",
       /*payload=*/
       "732fd9df1c334a0404db3ca15cc8bb7dd03fa0a6b42f329f",
       /*encrypted_payload=*/
       "020004fe4928f722ec664567779d83b1254786931a2e119019f33b4e0413c0e08b0845"
       "eb892ce5a9bf54154081fae12808ce40824743df8c70aeb681d5f132d75aecf00eb89a"
       "4827bf50fb7e11ca57f5f7d9364dbf5d552a513ac705e2159d3bb801e414c16f14a837"
       "400e31cc85181c2dccd14f558af1"},
      {/*private_key=*/
       "90149c92a432b9a62d4ebee16e2d358aa7253c3160126f01bb16e3d70523643a",
       /*shared_secret=*/"6f7171c87b33c444f9f0b0c0956b4977",
       /*header=*/"6094dd78335861acaf599a29",
       /*payload=*/
       "011360559ef4615e42bfb67de144acf4a10c750a92af6cef",
       /*encrypted_payload=*/
       "02000413765df10f60bc52a5be14e1d7a3c08ab907704574d30993db6d960344e366d3"
       "42f0e06416ac0baa15a8c7e6adaffece55c4df14cc0f6d8769e3a6dc64a85df4ed5959"
       "f76b51b123bc8f2572bec12c46138b5362b967850cc6b297fffc20fa639adfc2aebc37"
       "f96aea37d9f46c42970d44ebe245"},
      {/*private_key=*/
       "332f062284ca639d4d89047b4518b57e081ad211ce60d2855c162e55adf702b4",
       /*shared_secret=*/"3f84552779a8e37a8cfed47cde41a14f",
       /*header=*/"8b21ec8a81b2e79221af61e3",
       /*payload=*/
       "92fd1f0f297b0e60cf1e61d59c7b820f90c027ef74f57a91",
       /*encrypted_payload=*/
       "020004c8d7a52c441f298e9366ccb10a0197db798f56ef1b94c026783b95bd209f48ea"
       "07f5f783ea7c9617ddd6c1651b7f983f0404fb6a0d59f57035416e7d079479b7197662"
       "21930955107978660153165b30aea9e6bf9cae23e9fa9156c27e44da6bc254d636fd0b"
       "3b5e1ec279c7d9d2ed5e6644d638"}};

  for (const TestVector& test_vector : kTestVectors) {
    SCOPED_TRACE("Failure with private key: " + test_vector.private_key);

    std::vector<uint8_t> private_key_bytes;
    ASSERT_TRUE(
        base::HexStringToBytes(test_vector.private_key, &private_key_bytes));
    std::unique_ptr<SecureBoxPrivateKey> private_key =
        SecureBoxPrivateKey::CreateByImport(private_key_bytes);
    ASSERT_THAT(private_key, NotNull());

    std::vector<uint8_t> shared_secret;
    ASSERT_TRUE(
        base::HexStringToBytes(test_vector.shared_secret, &shared_secret));

    std::vector<uint8_t> header;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.header, &header));

    std::vector<uint8_t> encrypted_payload;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.encrypted_payload,
                                       &encrypted_payload));

    std::optional<std::vector<uint8_t>> decrypted_payload =
        private_key->Decrypt(shared_secret, header, encrypted_payload);
    ASSERT_THAT(decrypted_payload, Ne(std::nullopt));

    std::vector<uint8_t> expected_payload;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.payload, &expected_payload));
    EXPECT_THAT(*decrypted_payload, Eq(expected_payload));
  }
}

TEST_F(SecureBoxTest, ShouldEncryptThenDecryptInSymmetricMode) {
  std::vector<uint8_t> encrypted =
      SecureBoxSymmetricEncrypt(kTestSharedSecret, kTestHeader, kTestPayload);

  std::optional<std::vector<uint8_t>> decrypted =
      SecureBoxSymmetricDecrypt(kTestSharedSecret, kTestHeader, encrypted);

  ASSERT_THAT(decrypted, Ne(std::nullopt));
  EXPECT_THAT(*decrypted, Eq(kTestPayload));
}

TEST_F(SecureBoxTest, ShouldDecryptTestVectorsInSymmetricMode) {
  struct TestVector {
    std::string shared_secret;
    std::string header;
    std::string payload;
    std::string encrypted_payload;
  };

  const std::vector<TestVector> kTestVectors = {
      {/*shared_secret=*/"b9bea7bdda7050128a93132a62df5eb9",
       /*header=*/"e343638bb4efdf81a7fe8dd4",
       /*payload=*/"a93b641a36b9cc09a60be2d6fc8b120d76d4e000c191004d",
       /*encrypted_payload=*/
       "0200f673db733138c4f2a688e71cdae2ad48319e11471139e5f0fccad6bfb2b67547cc0"
       "4cdcb95713d234275f3523bdaca1f8248f91c"},
      {/*shared_secret=*/"225ef04a79a0883f5f8c069a62484ea6",
       /*header=*/"bead3e2a4a6420715a46a0d3",
       /*payload=*/"a92998b81b6f6bafb5f477612365cdbc2cc2887901dc8738",
       /*encrypted_payload=*/
       "02004e25b46801af126c8dc4d022a2274f4f99286f7bb11b20beb591248794b7547b48b"
       "60653afe4c2f0783f4d8d4e2d50a17c9a761f"},
      {/*shared_secret=*/"71e6adfc21920c3202a108db88e82ea9",
       /*header=*/"64a990cc5776e78224c7386a",
       /*payload=*/"8bff0dd38037924864752daf872aeb76c46801e4ce8066ad",
       /*encrypted_payload=*/
       "0200074a4bb7fa9fa5fd3913b34c208351fba7f56d16fa32f23bfd3d93e9304a5ea50e5"
       "f610bf5efe5d572b993859b6e6e0a4baeb134"},
      {/*shared_secret=*/"777b4d6df6f20045dd5658f777029a72",
       /*header=*/"993c498d5b4da8bc5b525f04",
       /*payload=*/"7614145015b19eb161dba85a32eddab6156fc9d49a3ee73c",
       /*encrypted_payload=*/
       "0200f27b526bb6676548ddb05d3903703eb134346414443f610d8d2c0e46109129cc9f7"
       "2daa600c7686f1b1991790e905ad79f58a6c3"}};

  for (const TestVector& test_vector : kTestVectors) {
    SCOPED_TRACE("Failure with shared secret: " + test_vector.shared_secret);

    std::vector<uint8_t> shared_secret;
    ASSERT_TRUE(
        base::HexStringToBytes(test_vector.shared_secret, &shared_secret));

    std::vector<uint8_t> header;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.header, &header));

    std::vector<uint8_t> encrypted_payload;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.encrypted_payload,
                                       &encrypted_payload));

    std::optional<std::vector<uint8_t>> decrypted_payload =
        SecureBoxSymmetricDecrypt(shared_secret, header, encrypted_payload);
    ASSERT_THAT(decrypted_payload, Ne(std::nullopt));

    std::vector<uint8_t> expected_payload;
    ASSERT_TRUE(base::HexStringToBytes(test_vector.payload, &expected_payload));
    EXPECT_THAT(*decrypted_payload, Eq(expected_payload));
  }
}

}  // namespace

}  // namespace trusted_vault
