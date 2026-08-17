// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/athm_test_issuer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "components/private_verification_tokens/common/privacy_pass_athm_batch_request.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/anonymous_tokens/src/anonymous_tokens/cpp/privacy_pass/athm_token_encodings_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

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

std::optional<std::vector<uint8_t>> CreateAthmTokenRequestBytes(
    uint16_t token_type,
    uint8_t truncated_key_id,
    base::span<const uint8_t> encoded_request) {
  anonymous_tokens::AthmTokenRequest req{
      .token_type = token_type,
      .truncated_issuer_key_id = truncated_key_id,
      .encoded_request =
          std::string(encoded_request.begin(), encoded_request.end()),
  };
  std::string marshaled;
  if (!anonymous_tokens::MarshalAthmTokenRequest(req, &marshaled).ok()) {
    return std::nullopt;
  }
  return std::vector<uint8_t>(marshaled.begin(), marshaled.end());
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

TEST(AthmTestIssuerTest, KeyIdComputation_MatchesSha256OfPublicKey) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("key-id-test"));
  ASSERT_TRUE(issuer.has_value());
  EXPECT_EQ(issuer->key_id(), crypto::hash::Sha256(issuer->public_key()));
  EXPECT_EQ(issuer->truncated_key_id(), issuer->key_id().back());
}

TEST(AthmTestIssuerTest, BatchIssue_Success) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-issue"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("batch-issue"));
  ASSERT_TRUE(client.has_value());

  constexpr size_t kBatchSize = 3;
  std::vector<AthmTestClient::ClientRequest> client_requests;
  std::vector<std::vector<uint8_t>> requests;
  for (size_t i = 0; i < kBatchSize; ++i) {
    std::optional<AthmTestClient::ClientRequest> req =
        client->CreateClientRequest();
    ASSERT_TRUE(req.has_value());
    std::optional<std::vector<uint8_t>> request_bytes =
        CreateAthmTokenRequestBytes(
            PrivateVerificationTokensParameters::kAthmTokenType,
            issuer->truncated_key_id(), req->request);
    ASSERT_TRUE(request_bytes.has_value());
    requests.push_back(std::move(request_bytes.value()));
    client_requests.push_back(std::move(*req));
  }

  constexpr uint8_t kMetadata = 2;
  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue(requests, kMetadata);
  ASSERT_TRUE(batch_responses.has_value());
  ASSERT_EQ(batch_responses->size(), kBatchSize);

  for (size_t i = 0; i < kBatchSize; ++i) {
    std::optional<std::vector<uint8_t>> token =
        client->FinalizeToken(client_requests[i], (*batch_responses)[i]);
    ASSERT_TRUE(token.has_value());
    std::optional<uint8_t> recovered = issuer->Verify(*token);
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(*recovered, kMetadata);
  }
}

TEST(AthmTestIssuerTest, BatchIssue_EmptyRequests_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-issue-empty"));
  ASSERT_TRUE(issuer.has_value());

  const std::vector<std::vector<uint8_t>> empty_requests;
  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue(empty_requests, /*hidden_metadata=*/0);
  EXPECT_FALSE(batch_responses.has_value());
}

TEST(AthmTestIssuerTest, BatchIssue_InvalidRequestInBatch_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-issue-invalid"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("batch-issue-invalid"));
  ASSERT_TRUE(client.has_value());

  std::optional<AthmTestClient::ClientRequest> valid_req =
      client->CreateClientRequest();
  ASSERT_TRUE(valid_req.has_value());

  std::optional<std::vector<uint8_t>> formatted_valid_req =
      CreateAthmTokenRequestBytes(
          PrivateVerificationTokensParameters::kAthmTokenType,
          issuer->truncated_key_id(), valid_req->request);
  ASSERT_TRUE(formatted_valid_req.has_value());

  std::vector<uint8_t> invalid_req = {1, 2, 3};
  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue({formatted_valid_req.value(), invalid_req},
                         /*hidden_metadata=*/0);
  EXPECT_FALSE(batch_responses.has_value());
}

TEST(AthmTestIssuerTest, BatchIssue_InvalidMetadata_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-issue-meta"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("batch-issue-meta"));
  ASSERT_TRUE(client.has_value());

  std::optional<AthmTestClient::ClientRequest> valid_req =
      client->CreateClientRequest();
  ASSERT_TRUE(valid_req.has_value());

  std::optional<std::vector<uint8_t>> formatted_valid_req =
      CreateAthmTokenRequestBytes(
          PrivateVerificationTokensParameters::kAthmTokenType,
          issuer->truncated_key_id(), valid_req->request);
  ASSERT_TRUE(formatted_valid_req.has_value());

  // Metadata >= kBucketCount is out of range.
  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue({formatted_valid_req.value()},
                         /*hidden_metadata=*/kBucketCount);
  EXPECT_FALSE(batch_responses.has_value());
}

TEST(AthmTestIssuerTest, BatchIssue_WrongTokenType_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-type-inv"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("batch-type-inv"));
  ASSERT_TRUE(client.has_value());

  std::optional<AthmTestClient::ClientRequest> valid_req =
      client->CreateClientRequest();
  ASSERT_TRUE(valid_req.has_value());

  std::optional<std::vector<uint8_t>> wrong_type_req =
      CreateAthmTokenRequestBytes(/*token_type=*/0x1234,
                                  issuer->truncated_key_id(),
                                  valid_req->request);
  ASSERT_TRUE(wrong_type_req.has_value());

  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue({wrong_type_req.value()}, /*hidden_metadata=*/0);
  EXPECT_FALSE(batch_responses.has_value());
}

TEST(AthmTestIssuerTest, BatchIssue_WrongTruncatedKeyId_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-keyid-inv"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<AthmTestClient> client =
      AthmTestClient::Create(issuer->public_key(), issuer->public_key_proof(),
                             kBucketCount, DeploymentId("batch-keyid-inv"));
  ASSERT_TRUE(client.has_value());

  std::optional<AthmTestClient::ClientRequest> valid_req =
      client->CreateClientRequest();
  ASSERT_TRUE(valid_req.has_value());

  uint8_t wrong_key_id =
      static_cast<uint8_t>(issuer->truncated_key_id() ^ 0xFF);

  std::optional<std::vector<uint8_t>> wrong_key_req =
      CreateAthmTokenRequestBytes(
          PrivateVerificationTokensParameters::kAthmTokenType, wrong_key_id,
          valid_req->request);
  ASSERT_TRUE(wrong_key_req.has_value());

  std::optional<std::vector<std::vector<uint8_t>>> batch_responses =
      issuer->BatchIssue({wrong_key_req.value()}, /*hidden_metadata=*/0);
  EXPECT_FALSE(batch_responses.has_value());
}

TEST(AthmTestIssuerTest, BatchIssue_StringOverload_Success) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-str"));
  ASSERT_TRUE(issuer.has_value());

  constexpr size_t kBatchSize = 2;
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      issuer->public_key(), issuer->public_key_proof(),
      base::Time::Now() + base::Days(30), /*version=*/1);
  IssuerConfig issuer_config(GURL("https://issuer.example.com/request"),
                             kBatchSize, std::move(public_key), {},
                             "batch-str");

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kBucketCount);
  ASSERT_TRUE(batch_request.has_value());

  constexpr uint8_t kMetadata = 1;
  std::optional<std::string> response_body = issuer->BatchIssue(
      base::as_string_view(batch_request->request_body()), kMetadata);
  ASSERT_TRUE(response_body.has_value());
  ASSERT_FALSE(response_body->empty());

  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*response_body));
  ASSERT_TRUE(finalized_tokens.has_value());
  ASSERT_EQ(finalized_tokens->size(), kBatchSize);

  for (size_t i = 0; i < kBatchSize; ++i) {
    const std::vector<uint8_t>& token_bytes = (*finalized_tokens)[i];
    anonymous_tokens::AthmToken unmarshaled_token;
    absl::Status unmarshal_status = anonymous_tokens::UnmarshalAthmToken(
        base::as_string_view(token_bytes), &unmarshaled_token);
    ASSERT_TRUE(unmarshal_status.ok());
    std::optional<uint8_t> recovered =
        issuer->Verify(base::as_byte_span(unmarshaled_token.token));
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(*recovered, kMetadata);
  }
}

TEST(AthmTestIssuerTest, BatchIssue_StringOverload_Empty_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-str-empty"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<std::string> response_body =
      issuer->BatchIssue("", /*hidden_metadata=*/0);
  EXPECT_FALSE(response_body.has_value());
}

TEST(AthmTestIssuerTest,
     BatchIssue_StringOverload_InvalidLength_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("batch-str-inv"));
  ASSERT_TRUE(issuer.has_value());

  std::optional<std::string> response_body =
      issuer->BatchIssue("not_36_bytes", /*hidden_metadata=*/0);
  EXPECT_FALSE(response_body.has_value());
}

TEST(AthmTestIssuerTest, VerifyWithCheck_Success) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("verify-check-ok"));
  ASSERT_TRUE(issuer.has_value());

  constexpr size_t kBatchSize = 1;
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      issuer->public_key(), issuer->public_key_proof(),
      base::Time::Now() + base::Days(30), /*version=*/1);
  IssuerConfig issuer_config(GURL("https://issuer.example.com/request"),
                             kBatchSize, std::move(public_key), {},
                             "verify-check-ok");

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kBucketCount);
  ASSERT_TRUE(batch_request.has_value());

  constexpr uint8_t kMetadata = 3;
  std::optional<std::string> response_body = issuer->BatchIssue(
      base::as_string_view(batch_request->request_body()), kMetadata);
  ASSERT_TRUE(response_body.has_value());

  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*response_body));
  ASSERT_TRUE(finalized_tokens.has_value());
  ASSERT_EQ(finalized_tokens->size(), 1u);

  std::optional<uint8_t> recovered =
      issuer->VerifyWithCheck((*finalized_tokens)[0]);
  ASSERT_TRUE(recovered.has_value());
  EXPECT_EQ(*recovered, kMetadata);
}

TEST(AthmTestIssuerTest, VerifyWithCheck_WrongTokenType_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("verify-check-type"));
  ASSERT_TRUE(issuer.has_value());

  constexpr size_t kBatchSize = 1;
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      issuer->public_key(), issuer->public_key_proof(),
      base::Time::Now() + base::Days(30), /*version=*/1);
  IssuerConfig issuer_config(GURL("https://issuer.example.com/request"),
                             kBatchSize, std::move(public_key), {},
                             "verify-check-type");

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kBucketCount);
  ASSERT_TRUE(batch_request.has_value());

  std::optional<std::string> response_body =
      issuer->BatchIssue(base::as_string_view(batch_request->request_body()),
                         /*hidden_metadata=*/0);
  ASSERT_TRUE(response_body.has_value());

  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*response_body));
  ASSERT_TRUE(finalized_tokens.has_value());

  anonymous_tokens::AthmToken token;
  ASSERT_TRUE(anonymous_tokens::UnmarshalAthmToken(
                  base::as_string_view((*finalized_tokens)[0]), &token)
                  .ok());
  token.token_type = 0x1234;
  std::string marshaled;
  ASSERT_TRUE(anonymous_tokens::MarshalAthmToken(token, &marshaled).ok());

  EXPECT_FALSE(
      issuer->VerifyWithCheck(base::as_byte_span(marshaled)).has_value());
}

TEST(AthmTestIssuerTest, VerifyWithCheck_WrongKeyId_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("verify-check-keyid"));
  ASSERT_TRUE(issuer.has_value());

  constexpr size_t kBatchSize = 1;
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      issuer->public_key(), issuer->public_key_proof(),
      base::Time::Now() + base::Days(30), /*version=*/1);
  IssuerConfig issuer_config(GURL("https://issuer.example.com/request"),
                             kBatchSize, std::move(public_key), {},
                             "verify-check-keyid");

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kBucketCount);
  ASSERT_TRUE(batch_request.has_value());

  std::optional<std::string> response_body =
      issuer->BatchIssue(base::as_string_view(batch_request->request_body()),
                         /*hidden_metadata=*/0);
  ASSERT_TRUE(response_body.has_value());

  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*response_body));
  ASSERT_TRUE(finalized_tokens.has_value());

  anonymous_tokens::AthmToken token;
  ASSERT_TRUE(anonymous_tokens::UnmarshalAthmToken(
                  base::as_string_view((*finalized_tokens)[0]), &token)
                  .ok());
  token.issuer_key_id[0] ^= 0xFF;
  std::string marshaled;
  ASSERT_TRUE(anonymous_tokens::MarshalAthmToken(token, &marshaled).ok());

  EXPECT_FALSE(
      issuer->VerifyWithCheck(base::as_byte_span(marshaled)).has_value());
}

TEST(AthmTestIssuerTest, VerifyWithCheck_InvalidCrypto_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("verify-check-crypto"));
  ASSERT_TRUE(issuer.has_value());

  constexpr size_t kBatchSize = 1;
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      issuer->public_key(), issuer->public_key_proof(),
      base::Time::Now() + base::Days(30), /*version=*/1);
  IssuerConfig issuer_config(GURL("https://issuer.example.com/request"),
                             kBatchSize, std::move(public_key), {},
                             "verify-check-crypto");

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kBucketCount);
  ASSERT_TRUE(batch_request.has_value());

  std::optional<std::string> response_body =
      issuer->BatchIssue(base::as_string_view(batch_request->request_body()),
                         /*hidden_metadata=*/0);
  ASSERT_TRUE(response_body.has_value());

  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*response_body));
  ASSERT_TRUE(finalized_tokens.has_value());

  anonymous_tokens::AthmToken token;
  ASSERT_TRUE(anonymous_tokens::UnmarshalAthmToken(
                  base::as_string_view((*finalized_tokens)[0]), &token)
                  .ok());
  token.token[kTamperByteOffset] ^= 0x01;
  std::string marshaled;
  ASSERT_TRUE(anonymous_tokens::MarshalAthmToken(token, &marshaled).ok());

  EXPECT_FALSE(
      issuer->VerifyWithCheck(base::as_byte_span(marshaled)).has_value());
}

TEST(AthmTestIssuerTest, VerifyWithCheck_InvalidLength_ReturnsNullopt) {
  std::optional<AthmTestIssuer> issuer =
      AthmTestIssuer::Create(kBucketCount, DeploymentId("verify-check-len"));
  ASSERT_TRUE(issuer.has_value());

  const std::vector<uint8_t> short_bytes = {0xC0, 0x7E, 0x01, 0x02};
  EXPECT_FALSE(issuer->VerifyWithCheck(short_bytes).has_value());
}

}  // namespace
}  // namespace private_verification_tokens
