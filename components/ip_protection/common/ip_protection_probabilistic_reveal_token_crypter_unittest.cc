// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

namespace {

using ::private_join_and_compute::BigNum;
using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalDecrypter;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PrivateKey;
using ::private_join_and_compute::elgamal::PublicKey;

// Return serialized g^private_key where g is group generator for curve
// secp224r1.
absl::StatusOr<std::string> GetSerializedPublicKey(uint64_t private_key) {
  Context context;
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(NID_secp224r1, &context));
  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, g.Mul(context.CreateBigNum(private_key)));
  return y.ToBytesCompressed();
}

absl::StatusOr<ProbabilisticRevealToken> CreateTokenFromPlaintext(
    uint64_t private_key,
    const std::string& plaintext) {
  Context context;
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(NID_secp224r1, &context));
  ASSIGN_OR_RETURN(ECPoint plaintext_point,
                   group.GetPointByHashingToCurveSha256(plaintext));

  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, g.Mul(context.CreateBigNum(private_key)));
  ElGamalEncrypter encrypter(&group, std::make_unique<PublicKey>(PublicKey{
                                         std::move(g), std::move(y)}));
  ASSIGN_OR_RETURN(Ciphertext ciphertext, encrypter.Encrypt(plaintext_point));
  ASSIGN_OR_RETURN(std::string u_compressed, ciphertext.u.ToBytesCompressed());
  ASSIGN_OR_RETURN(std::string e_compressed, ciphertext.e.ToBytesCompressed());
  return ProbabilisticRevealToken{1, std::move(u_compressed),
                                  std::move(e_compressed)};
}

absl::StatusOr<std::vector<ProbabilisticRevealToken>> CreateTokens(
    uint64_t private_key,
    std::size_t num_tokens) {
  std::vector<ProbabilisticRevealToken> tokens(num_tokens);
  for (std::size_t i = 0; i < num_tokens; ++i) {
    ASSIGN_OR_RETURN(tokens[i],
                     CreateTokenFromPlaintext(
                         private_key, "token-" + base::NumberToString(i)));
  }
  return tokens;
}

absl::StatusOr<ECPoint> DecryptToken(uint64_t private_key,
                                     const ProbabilisticRevealToken& token) {
  Context context;
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(NID_secp224r1, &context));
  ASSIGN_OR_RETURN(ECPoint u, group.CreateECPoint(token.u));
  ASSIGN_OR_RETURN(ECPoint e, group.CreateECPoint(token.e));
  Ciphertext ciphertext{std::move(u), std::move(e)};
  ElGamalDecrypter decrypter(std::make_unique<PrivateKey>(
      PrivateKey{context.CreateBigNum(private_key)}));
  return decrypter.Decrypt(ciphertext);
}

}  // namespace

TEST(IpProtectionProbabilisticRevealTokenCrypter, CreateSuccess) {
  const uint64_t private_key = 12345;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 3;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  const auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, CreateEmptyTokens) {
  const uint64_t private_key = 12345;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, {});
  EXPECT_TRUE(maybe_crypter.ok());
  const auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, CreateInvalidPublicKey) {
  const auto& serialized_public_key = "invalid-public-key";
  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, {});
  EXPECT_EQ(maybe_crypter.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, CreateInvalidTokenU) {
  const uint64_t private_key = 12345;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 7;
  auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  auto& tokens = maybe_tokens.value();
  tokens[num_tokens - 1].u = "invalid-u";

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_EQ(maybe_crypter.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, CreateInvalidTokenE) {
  const uint64_t private_key = 12345;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 3;
  auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  auto& tokens = maybe_tokens.value();
  tokens[num_tokens - 1].e = "invalid-e";

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_EQ(maybe_crypter.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter,
     SetNewPublicKeyAndTokensSuccess) {
  auto maybe_serialized_public_key =
      GetSerializedPublicKey(/* private_key = */ 11111);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  // Create a crypter with no tokens.
  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, {});
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));

  // Create a new key.
  const uint64_t private_key = 22222;
  maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  serialized_public_key = maybe_serialized_public_key.value();

  // Create tokens with the new key.
  const std::size_t num_tokens = 12;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  auto status =
      crypter->SetNewPublicKeyAndTokens(serialized_public_key, tokens);

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter,
     SetNewPublicKeyAndTokensEmptyTokens) {
  auto maybe_serialized_public_key =
      GetSerializedPublicKey(/* private_key = */ 11111);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();
  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, {});
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));

  // Create a new key.
  const uint64_t private_key = 22222;
  maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  serialized_public_key = maybe_serialized_public_key.value();

  auto status = crypter->SetNewPublicKeyAndTokens(serialized_public_key, {});
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST(IpProtectionProbabilisticRevealTokenCrypter,
     SetNewPublicKeyAndTokensInvalidPublicKey) {
  const uint64_t private_key = 42;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 10;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);

  // Should fail and crypter should keep its state.
  auto status = crypter->SetNewPublicKeyAndTokens("invalid-pk", {});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter,
     SetNewPublicKeyAndTokensInvalidTokenU) {
  const uint64_t private_key = 42;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 10;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);

  // Get a new public key.
  const uint64_t new_private_key = 23;
  maybe_serialized_public_key = GetSerializedPublicKey(new_private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  serialized_public_key = maybe_serialized_public_key.value();

  // Get new tokens with new public key.
  const std::size_t new_num_tokens = 2;
  auto maybe_new_tokens = CreateTokens(new_private_key, new_num_tokens);
  ASSERT_TRUE(maybe_new_tokens.ok());
  auto& new_tokens = maybe_new_tokens.value();
  new_tokens[1].u = "not-a-valid-token-u";

  // Should fail and crypter should keep its state.
  auto status =
      crypter->SetNewPublicKeyAndTokens(serialized_public_key, new_tokens);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  // NumTokens() should remain as num_tokens not new_num_tokens
  EXPECT_EQ(crypter->NumTokens(), num_tokens);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter,
     SetNewPublicKeyAndTokensInvalidTokenE) {
  const uint64_t private_key = 42;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 10;
  auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  auto& tokens = maybe_tokens.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);

  // Get a new public key.
  const uint64_t new_private_key = 23;
  maybe_serialized_public_key = GetSerializedPublicKey(new_private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  serialized_public_key = maybe_serialized_public_key.value();

  // Get new tokens with new public key.
  const std::size_t new_num_tokens = 2;
  auto maybe_new_tokens = CreateTokens(new_private_key, new_num_tokens);
  ASSERT_TRUE(maybe_new_tokens.ok());
  auto& new_tokens = maybe_new_tokens.value();
  new_tokens[1].e = "not-a-valid-token-e";

  // Should fail and crypter should keep its state.
  auto status =
      crypter->SetNewPublicKeyAndTokens(serialized_public_key, new_tokens);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  // NumTokens() should remain as num_tokens not new_num_tokens
  EXPECT_EQ(crypter->NumTokens(), num_tokens);
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, ClearTokens) {
  const uint64_t private_key = 7654;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 27;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      serialized_public_key, tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);

  crypter->ClearTokens();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST(IpProtectionProbabilisticRevealTokenCrypter, Randomize) {
  const uint64_t private_key = 2025;
  const auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  const auto& serialized_public_key = maybe_serialized_public_key.value();

  const std::size_t num_tokens = 12;
  const auto maybe_tokens = CreateTokens(private_key, num_tokens);
  ASSERT_TRUE(maybe_tokens.ok());
  const auto& tokens = maybe_tokens.value();

  const auto maybe_crypter =
      IpProtectionProbabilisticRevealTokenCrypter::Create(serialized_public_key,
                                                          tokens);
  EXPECT_TRUE(maybe_crypter.ok());
  const auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), num_tokens);

  // Decrypt tokens and randomized tokens, compare results.
  for (std::size_t i = 0; i < num_tokens; ++i) {
    const auto maybe_randomized_token = crypter->Randomize(i);
    ASSERT_TRUE(maybe_randomized_token.ok());
    const auto& randomized_token = maybe_randomized_token.value();

    const auto maybe_decrypted_randomized_token =
        DecryptToken(private_key, randomized_token);
    ASSERT_TRUE(maybe_decrypted_randomized_token.ok());

    const auto maybe_decrypted_token = DecryptToken(private_key, tokens[i]);
    ASSERT_TRUE(maybe_decrypted_token.ok());

    EXPECT_EQ(maybe_decrypted_token.value(),
              maybe_decrypted_randomized_token.value());
  }

  // Try to randomize a token that does not exist.
  const auto maybe_token = crypter->Randomize(num_tokens);
  ASSERT_FALSE(maybe_token.ok());
  EXPECT_EQ(maybe_token.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace ip_protection
