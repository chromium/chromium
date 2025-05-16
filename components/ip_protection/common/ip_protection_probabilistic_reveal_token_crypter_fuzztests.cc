// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/fuzztest/src/fuzztest/domain_core.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"


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
          {
              ip_protection::ProbabilisticRevealToken{1, u, e},
          });
}

// Fuzztest for probabilistic reveal token randomization. In Chrome, PRTs are
// returned by the issuer service, which is from a hard coded url. Fuzzer
// creates a valid public key and a PRT before calling randomize.
void RandomizeDoesNotCrash(const std::string& plaintext) {
  base::expected<
      std::unique_ptr<ip_protection::ProbabilisticRevealTokenTestIssuer>,
      absl::Status>
      maybe_issuer = ip_protection::ProbabilisticRevealTokenTestIssuer::Create(
          /*private_key=*/12345);
  ASSERT_TRUE(maybe_issuer.has_value())
      << "creating test issuer failed with error: " << maybe_issuer.error();
  auto issuer = std::move(maybe_issuer).value();
  base::expected<ip_protection::GetProbabilisticRevealTokenResponse,
                 absl::Status>
      maybe_response =
          issuer->IssueByHashingToPoint({plaintext},
                                        /*expiration=*/base::Time::Now(),
                                        /*next_epoch_start=*/base::Time::Now(),
                                        /*num_tokens_with_signal=*/0,
                                        /*epoch_id=*/"epoch-id");
  ASSERT_TRUE(maybe_response.has_value())
      << "creating tokens failed with error: " << maybe_response.error();
  ASSERT_THAT(issuer->Tokens(), testing::SizeIs(1));
  const ip_protection::ProbabilisticRevealToken& token = issuer->Tokens()[0];

  base::expected<
      std::unique_ptr<
          ip_protection::IpProtectionProbabilisticRevealTokenCrypter>,
      absl::Status>
      maybe_crypter =
          ip_protection::IpProtectionProbabilisticRevealTokenCrypter::Create(
              issuer->GetSerializedPublicKey(), {token});
  ASSERT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  ASSERT_TRUE(crypter->IsTokenAvailable());
  ASSERT_EQ(crypter->NumTokens(), std::size_t(1));
  auto result = crypter->Randomize(0);
}

FUZZ_TEST(IpProtectionProbabilisticRevealTokenCrypterFuzzTests,
          CreateDoesNotCrash);

FUZZ_TEST(IpProtectionProbabilisticRevealTokenCrypterFuzzTests,
          RandomizeDoesNotCrash);
