// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/athm_test_issuer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {
namespace {

// ATHM token wire layout: t[32] || big_p[33] || big_q[33].
constexpr size_t kTokenSize = 98;
// Offset of a coordinate byte inside the big_p compressed point (big_p starts
// immediately after the 32-byte t field).
constexpr size_t kTamperByteOffset = 40;
// Metadata buckets exercised by the all-buckets round-trip test.
constexpr uint8_t kBucketCount = 8;
// One past the ATHM parameters' 255-byte deployment_id limit.
constexpr size_t kOverlongDeploymentIdSize = 256;

std::vector<uint8_t> DeploymentId(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

// Drives the issuer and client as separate parties through a full issuance +
// redemption (the client touches only public material, the issuer holds the
// secret key) and returns the recovered metadata, or nullopt if any step fails.
std::optional<uint8_t> RunRoundtrip(const AthmTestIssuer& issuer,
                                    const AthmTestClient& client,
                                    uint8_t metadata) {
  std::optional<AthmTestClient::ClientRequest> request =
      client.CreateClientRequest();
  if (!request) {
    return std::nullopt;
  }
  std::optional<std::vector<uint8_t>> response =
      issuer.Issue(request->request, metadata);
  if (!response) {
    return std::nullopt;
  }
  std::optional<std::vector<uint8_t>> token =
      client.FinalizeToken(*request, *response);
  if (!token) {
    return std::nullopt;
  }
  return issuer.Verify(*token);
}

TEST(AthmTestIssuerTest, IssuanceRedemptionRoundtrip) {
  std::optional<AthmTestIssuer> issuer = AthmTestIssuer::Create(
      /*num_buckets=*/4, DeploymentId("pvt-issuer-test"));

  ASSERT_TRUE(issuer.has_value());
  EXPECT_THAT(issuer->public_key(), testing::Not(testing::IsEmpty()));
  EXPECT_THAT(issuer->public_key_proof(), testing::Not(testing::IsEmpty()));
  EXPECT_THAT(issuer->params(), testing::Not(testing::IsEmpty()));

  std::optional<AthmTestClient> client = AthmTestClient::Create(
      issuer->public_key(), issuer->public_key_proof(), /*num_buckets=*/4,
      DeploymentId("pvt-issuer-test"));
  ASSERT_TRUE(client.has_value());

  std::optional<uint8_t> recovered =
      RunRoundtrip(*issuer, *client, /*metadata=*/3);
  ASSERT_TRUE(recovered.has_value());
  EXPECT_EQ(int{*recovered}, 3);
}

// Every metadata bucket should round-trip, showing the value is genuinely
// carried through issuance and recovered at verification.
class AthmTestIssuerBucketTest : public testing::TestWithParam<uint8_t> {};

TEST_P(AthmTestIssuerBucketTest, MetadataRoundtrips) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("buckets"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("buckets"));
  ASSERT_TRUE(client.has_value());

  const uint8_t metadata = GetParam();
  std::optional<uint8_t> recovered = RunRoundtrip(*issuer, *client, metadata);
  ASSERT_TRUE(recovered.has_value()) << "metadata=" << int{metadata};
  EXPECT_EQ(int{*recovered}, int{metadata});
}

INSTANTIATE_TEST_SUITE_P(AllBuckets,
                         AthmTestIssuerBucketTest,
                         testing::Range(uint8_t{0}, kBucketCount));

// Create() returns nullopt for parameters the ATHM crate rejects.
struct InvalidConstructionCase {
  std::string name;
  uint8_t num_buckets;
  size_t deployment_id_size;
};

class AthmTestIssuerInvalidConstructionTest
    : public testing::TestWithParam<InvalidConstructionCase> {};

TEST_P(AthmTestIssuerInvalidConstructionTest, CreateReturnsNullopt) {
  const InvalidConstructionCase& test_case = GetParam();
  std::vector<uint8_t> deployment_id(test_case.deployment_id_size,
                                     uint8_t{'d'});
  EXPECT_FALSE(
      AthmTestIssuer::Create(test_case.num_buckets, deployment_id).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AthmTestIssuerInvalidConstructionTest,
    testing::Values(InvalidConstructionCase{"ZeroBuckets", 0, 8},
                    InvalidConstructionCase{"DeploymentIdTooLong", 4,
                                            kOverlongDeploymentIdSize}),
    [](const testing::TestParamInfo<InvalidConstructionCase>& info) {
      return info.param.name;
    });

// Each token request uses freshly randomized blinding, so two requests from the
// same issuer must differ.
TEST(AthmTestIssuerTest, ClientRequestsAreFreshlyBlinded) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(/*num_buckets=*/4, DeploymentId("blinding"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             /*num_buckets=*/4, DeploymentId("blinding"));
  ASSERT_TRUE(client.has_value());
  std::optional<AthmTestClient::ClientRequest> first =
      client->CreateClientRequest();
  std::optional<AthmTestClient::ClientRequest> second =
      client->CreateClientRequest();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_THAT(first->context, testing::Not(testing::IsEmpty()));
  EXPECT_NE(first->request, second->request);
}

// A tampered token must not verify. (Negative case: an early guard against
// silently accepting corrupted tokens.)
TEST(AthmTestIssuerTest, TamperedTokenDoesNotVerify) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(/*num_buckets=*/4, DeploymentId("tamper"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             /*num_buckets=*/4, DeploymentId("tamper"));
  ASSERT_TRUE(client.has_value());

  std::optional<AthmTestClient::ClientRequest> request =
      client->CreateClientRequest();
  ASSERT_TRUE(request.has_value());
  std::optional<std::vector<uint8_t>> response =
      issuer->Issue(request->request, /*hidden_metadata=*/2);
  ASSERT_TRUE(response.has_value());
  std::optional<std::vector<uint8_t>> token =
      client->FinalizeToken(*request, *response);
  ASSERT_TRUE(token.has_value());
  ASSERT_EQ(issuer->Verify(*token), std::optional<uint8_t>(2));

  // Corrupt a coordinate byte of the big_p point field and require verification
  // to fail. A malformed compressed point is rejected gracefully by the
  // decoder; it does not abort.
  std::vector<uint8_t> tampered = *token;
  ASSERT_EQ(tampered.size(), kTokenSize);
  tampered[kTamperByteOffset] ^= 0xFF;
  EXPECT_FALSE(issuer->Verify(tampered).has_value());
}

// A token issued by one issuer must not verify under a different issuer's key.
TEST(AthmTestIssuerTest, WrongIssuerKeyDoesNotVerify) {
  std::optional<AthmTestIssuer> issuer_a =
      AthmTestIssuer::Create(/*num_buckets=*/4, DeploymentId("cross-key"));
  std::optional<AthmTestIssuer> issuer_b =
      AthmTestIssuer::Create(/*num_buckets=*/4, DeploymentId("cross-key"));
  ASSERT_TRUE(issuer_a.has_value());
  ASSERT_TRUE(issuer_b.has_value());

  std::optional<AthmTestClient> client_a = AthmTestClient::Create(
      issuer_a->public_key(), issuer_a->public_key_proof(),
      /*num_buckets=*/4, DeploymentId("cross-key"));
  ASSERT_TRUE(client_a.has_value());
  std::optional<AthmTestClient::ClientRequest> request =
      client_a->CreateClientRequest();
  ASSERT_TRUE(request.has_value());
  std::optional<std::vector<uint8_t>> response =
      issuer_a->Issue(request->request, /*hidden_metadata=*/1);
  ASSERT_TRUE(response.has_value());
  std::optional<std::vector<uint8_t>> token =
      client_a->FinalizeToken(*request, *response);
  ASSERT_TRUE(token.has_value());

  EXPECT_EQ(issuer_a->Verify(*token), std::optional<uint8_t>(1));
  EXPECT_FALSE(issuer_b->Verify(*token).has_value());
}
}  // namespace
}  // namespace private_verification_tokens
