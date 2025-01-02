// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_crypter.h"

#include <string>

#include "base/base64.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
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

inline bool operator==(const PublicKey& a, const PublicKey& b) {
  return a.g.CompareTo(b.g) && a.y.CompareTo(b.y);
}

// Create a big num from a given value.
absl::StatusOr<BigNum> Create(const ECGroup& group, uint64_t n) {
  // get one
  const auto order = group.GetOrder();
  const auto one = order >> (order.BitLength() - 1);
  if (!one.IsOne()) {
    return absl::InternalError("number is expected to be 1");
  }
  // get zero
  const auto zero = one >> 1;
  if (!zero.IsZero()) {
    return absl::InternalError("number is expected to be 0");
  }

  // build big number from zero and one
  auto bn = zero;
  for (int i = 0; i < 64; ++i) {
    if (n & (uint64_t(1) << i)) {
      bn += (one << i);
    }
  }

  // check value
  ASSIGN_OR_RETURN(uint64_t iiv, bn.ToIntValue());
  if (iiv != n) {
    return absl::InternalError(
        "Number should fit uint64 and must be same as n.");
  }
  return bn;
}

absl::StatusOr<ECPoint> Exponent(const ECGroup& group,
                                 const ECPoint& point,
                                 uint64_t n) {
  ASSIGN_OR_RETURN(BigNum bn, Create(group, n));
  return point.Mul(bn);
}

// gets g^n for
// where g is group generator for curve secp224r1.
absl::StatusOr<PublicKey> GetPublicKey(uint64_t private_key) {
  auto context = std::make_unique<Context>();
  ASSIGN_OR_RETURN(ECGroup group,
                   ECGroup::Create(NID_secp224r1, context.get()));
  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, Exponent(group, g, private_key));
  return PublicKey{std::move(g), std::move(y)};
}

absl::StatusOr<PrivateKey> CreatePrivateKey(uint64_t private_key) {
  auto context = std::make_unique<Context>();
  ASSIGN_OR_RETURN(ECGroup group,
                   ECGroup::Create(NID_secp224r1, context.get()));
  ASSIGN_OR_RETURN(BigNum pk, Create(group, private_key));
  return PrivateKey{std::move(pk)};
}

TEST(IpProtectionCrypterTest, SerializePublicKey) {
  std::string expected_serialized_public_key;
  // A9sva+Yw4kalz32ZuFGUsSPUh+LUZrlLJKA8Pig= corresponds to
  // base64 encoding of compressed serialization of g^7
  // where g is group generator for curve secp224r1
  ASSERT_TRUE(base::Base64Decode("A9sva+Yw4kalz32ZuFGUsSPUh+LUZrlLJKA8Pig=",
                                 &expected_serialized_public_key));

  const uint64_t private_key = 7;
  auto maybe_public_key = GetPublicKey(private_key);
  ASSERT_TRUE(maybe_public_key.ok());
  const auto& public_key = maybe_public_key.value();

  auto maybe_serialized_public_key = SerializePublicKey(public_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  EXPECT_EQ(expected_serialized_public_key,
            maybe_serialized_public_key.value());
}

TEST(IpProtectionCrypterTest, DeserializePublicKey) {
  const uint64_t private_key = 7;
  auto maybe_expected_public_key = GetPublicKey(private_key);
  ASSERT_TRUE(maybe_expected_public_key.ok());
  const auto& expected_public_key = maybe_expected_public_key.value();

  // A9sva+Yw4kalz32ZuFGUsSPUh+LUZrlLJKA8Pig= corresponds to
  // base64 encoding of compressed serialization of g^7
  // where g is group generator for curve secp224r1
  std::string serialized_public_key;
  ASSERT_TRUE(base::Base64Decode("A9sva+Yw4kalz32ZuFGUsSPUh+LUZrlLJKA8Pig=",
                                 &serialized_public_key));

  auto maybe_public_key = DeserializePublicKey(serialized_public_key);
  ASSERT_TRUE(maybe_public_key.ok());
  EXPECT_TRUE(expected_public_key == maybe_public_key.value());
}

// Test ciphertext randomization.
TEST(IpProtectionCrypterTest, Randomize) {
  Context context{};
  auto maybe_group = ECGroup::Create(NID_secp224r1, &context);
  ASSERT_TRUE(maybe_group.ok());
  const auto& group = maybe_group.value();

  // create an ECPoint to be encrypted
  auto maybe_plaintext = group.GetPointByHashingToCurveSha256(
      "Not all treasure is silver and gold, mate.");
  ASSERT_TRUE(maybe_plaintext.ok());
  const auto& plaintext = maybe_plaintext.value();

  // create private/public key pair
  const uint64_t private_key = 91;
  auto maybe_public_key = GetPublicKey(private_key);
  ASSERT_TRUE(maybe_public_key.ok());
  auto public_key =
      std::make_unique<PublicKey>(std::move(maybe_public_key.value()));

  // get serialized public key
  auto maybe_serialized_public_key = SerializePublicKey(*public_key);
  ASSERT_TRUE(maybe_serialized_public_key.ok());
  const auto& serialized_public_key = maybe_serialized_public_key.value();

  // create encrypter
  ElGamalEncrypter encrypter(&(maybe_group.value()), std::move(public_key));

  // encrypt plaintext
  auto maybe_ciphertext = encrypter.Encrypt(plaintext);
  ASSERT_TRUE(maybe_ciphertext.ok());

  // randomize
  auto maybe_randomized_ciphertext =
      Randomize(serialized_public_key, maybe_ciphertext.value());
  ASSERT_TRUE(maybe_randomized_ciphertext.ok());
  const auto& randomized_ciphertext = maybe_randomized_ciphertext.value();

  // create decrypter
  auto maybe_pk = CreatePrivateKey(private_key);
  ASSERT_TRUE(maybe_pk.ok());
  ElGamalDecrypter decrypter(
      std::make_unique<PrivateKey>(std::move(maybe_pk.value())));

  // decrypt randomized ciphertext
  auto maybe_decrypted_plaintext = decrypter.Decrypt(randomized_ciphertext);
  ASSERT_TRUE(maybe_decrypted_plaintext.ok());

  // decrypted randomized ciphertext should be same as plaintext
  EXPECT_EQ(plaintext, maybe_decrypted_plaintext.value());
}

}  // namespace
}  // namespace ip_protection
