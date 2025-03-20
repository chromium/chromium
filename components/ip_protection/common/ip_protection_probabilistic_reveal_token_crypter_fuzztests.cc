// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalDecrypter;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PrivateKey;
using ::private_join_and_compute::elgamal::PublicKey;

namespace {

// Returns serialized g^private_key where g is group generator for curve
// secp224r1.
absl::StatusOr<std::string> GetSerializedPublicKey(uint64_t private_key) {
  Context context;
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(NID_secp224r1, &context));
  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, g.Mul(context.CreateBigNum(private_key)));
  return y.ToBytesCompressed();
}

absl::StatusOr<ip_protection::ProbabilisticRevealToken>
CreateTokenFromPlaintext(uint64_t private_key, const std::string& plaintext) {
  Context context;
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(NID_secp224r1, &context));
  ASSIGN_OR_RETURN(ECPoint plaintext_point,
                   group.GetPointByHashingToCurveSha256(plaintext));

  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, g.Mul(context.CreateBigNum(private_key)));
  std::unique_ptr<PublicKey> public_key(
      new PublicKey{std::move(g), std::move(y)});
  ElGamalEncrypter encrypter(&group, std::move(public_key));
  ASSIGN_OR_RETURN(Ciphertext ciphertext, encrypter.Encrypt(plaintext_point));
  ASSIGN_OR_RETURN(std::string u_compressed, ciphertext.u.ToBytesCompressed());
  ASSIGN_OR_RETURN(std::string e_compressed, ciphertext.e.ToBytesCompressed());
  return ip_protection::ProbabilisticRevealToken{1, std::move(u_compressed),
                                                 std::move(e_compressed)};
}

}  // namespace

// Fuzz test for creating probabilistic reveal token crypter. Creating crypter
// de-serializes an Elgamal encryption public key and ciphertext (elliptic curve
// points) using private-join-and-compute third party library. Serialized public
// key and ciphertext are returned from the issuer server.
void CreateDoesNotCrash(const std::string& serialized_public_key,
                        const std::string& u,
                        const std::string& e) {
  auto crypter =
      ip_protection::IpProtectionProbabilisticRevealTokenCrypter::Create(
          serialized_public_key,
          {ip_protection::ProbabilisticRevealToken{1, u, e}});
}

// Fuzz test for probabilistic reveal token randomization of ciphertexts.
// library. In chrome, ciphertext is returned by the issuer service, which is
// from a hard coded url. Fuzzer creates a valid encrypter from valid public key
// and ciphertexts before calling randomize. Ciphertexts are created by hashing
// random strings to the curve.
void RandomizeDoesNotCrash(const std::string& plaintext) {
  // public key
  const uint64_t private_key = 12345;
  auto maybe_serialized_public_key = GetSerializedPublicKey(private_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  auto& serialized_public_key = maybe_serialized_public_key.value();

  // tokens
  auto maybe_token = CreateTokenFromPlaintext(private_key, plaintext);
  ASSERT_TRUE(maybe_token.ok());
  auto& token = maybe_token.value();

  auto maybe_crypter =
      ip_protection::IpProtectionProbabilisticRevealTokenCrypter::Create(
          serialized_public_key, {token});
  ASSERT_TRUE(maybe_crypter.ok());
  auto& crypter = maybe_crypter.value();
  ASSERT_TRUE(crypter->IsTokenAvailable());
  ASSERT_EQ(crypter->NumTokens(), std::size_t(1));
  auto result = crypter->Randomize(0);
}

FUZZ_TEST(IpProtectionProbabilisticRevealTokenCrypterFuzzTests,
          CreateDoesNotCrash);

FUZZ_TEST(IpProtectionProbabilisticRevealTokenCrypterFuzzTests,
          RandomizeDoesNotCrash);
