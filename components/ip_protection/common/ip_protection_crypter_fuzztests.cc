// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_crypter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

using private_join_and_compute::Context;
using private_join_and_compute::ECGroup;
using private_join_and_compute::elgamal::Ciphertext;

void DeserializePublicKeyDoesNotCrash(
    const std::string& serialized_public_key) {
  auto result = ip_protection::DeserializePublicKey(serialized_public_key);
}

// In chrome, ciphertext is returned by the issuer service, which is from a hard
// coded url. Fuzzer creates ECPoints by hashing arbitrary strings to curve and
// creates a ciphertext from these points.
void RandomizeDoesNotCrash(const std::string& serialized_public_key,
                           const std::string& ciphertext_u,
                           const std::string& ciphertext_e) {
  Context context{};
  auto maybe_group = ECGroup::Create(NID_secp224r1, &context);
  ASSERT_TRUE(maybe_group.ok());
  const auto& group = maybe_group.value();

  auto maybe_u = group.GetPointByHashingToCurveSha256(ciphertext_u);
  ASSERT_TRUE(maybe_u.ok());
  auto maybe_e = group.GetPointByHashingToCurveSha256(ciphertext_e);
  ASSERT_TRUE(maybe_e.ok());

  Ciphertext ciphertext{
      std::move(maybe_u.value()),
      std::move(maybe_e.value()),
  };

  auto result = ip_protection::Randomize(serialized_public_key, ciphertext);
}

FUZZ_TEST(IpProtectionCrypterFuzzTests, DeserializePublicKeyDoesNotCrash);

FUZZ_TEST(IpProtectionCrypterFuzzTests, RandomizeDoesNotCrash);
