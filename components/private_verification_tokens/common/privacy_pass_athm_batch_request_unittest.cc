// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/privacy_pass_athm_batch_request.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {
namespace {

constexpr size_t kSingleTokenSize = 132;
constexpr uint8_t kNumBuckets = 4;
const char kDeploymentId[] = "batch-request-test-deployment";

std::vector<uint8_t> DeploymentIdBytes() {
  return base::ToVector(base::as_byte_span(std::string_view(kDeploymentId)));
}

IssuerConfig CreateTestIssuerConfig(const PrivacyPassAthmIssuer& issuer,
                                    int32_t batch_size) {
  PrivateVerificationTokensPublicKey public_key(
      url::Origin::Create(GURL("https://issuer.example.com")),
      base::ToVector(issuer.public_key_bytes()),
      base::ToVector(issuer.public_key_proof_bytes()),
      base::Time::Now() + base::Days(30), /*version=*/1);
  return IssuerConfig(GURL("https://issuer.example.com/request"), batch_size,
                      std::move(public_key), {}, kDeploymentId);
}

void VerifyFinalizedTokenMetadata(
    const PrivacyPassAthmIssuer& issuer,
    base::span<const uint8_t> finalized_token_bytes,
    uint8_t expected_metadata) {
  EXPECT_EQ(finalized_token_bytes.size(), kSingleTokenSize);
  auto recovered_metadata = issuer.verify_wire_token(
      rs_std::SliceRef<const uint8_t>(finalized_token_bytes));
  ASSERT_TRUE(recovered_metadata.has_value());
  EXPECT_EQ(*recovered_metadata, expected_metadata);
}

TEST(PrivacyPassAthmBatchRequestTest, RoundtripBatchIssuance) {
  auto issuer = PrivacyPassAthmIssuer::try_new(
      kNumBuckets, rs_std::SliceRef<const uint8_t>(DeploymentIdBytes()));
  ASSERT_TRUE(issuer.has_value());

  constexpr int32_t kBatchSize = 10;
  IssuerConfig issuer_config = CreateTestIssuerConfig(*issuer, kBatchSize);

  auto params = GetParametersForVersion(1);
  ASSERT_TRUE(params.has_value());

  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kNumBuckets);
  ASSERT_TRUE(batch_request.has_value());
  EXPECT_EQ(batch_request->request_body().size(),
            kBatchSize * params->single_request_size + sizeof(uint32_t));
  EXPECT_EQ(
      base::U32FromBigEndian(
          base::span(batch_request->request_body()).last<sizeof(uint32_t)>()),
      issuer_config.public_key.version());

  const uint8_t hidden_metadata = 1;
  auto batch_response = issuer->issue_batch_from_bytes(
      rs_std::SliceRef<const uint8_t>(batch_request->request_body()),
      hidden_metadata);
  ASSERT_TRUE(batch_response.has_value());

  // Finalize the batch request.
  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request->Finalize(base::as_byte_span(*batch_response));
  ASSERT_TRUE(finalized_tokens.has_value());
  ASSERT_EQ(finalized_tokens->size(), static_cast<size_t>(kBatchSize));

  // Verify each finalized token.
  for (int32_t i = 0; i < kBatchSize; ++i) {
    const std::vector<uint8_t>& token_bytes = (*finalized_tokens)[i];

    VerifyFinalizedTokenMetadata(*issuer, token_bytes,
                                 /*expected_metadata=*/hidden_metadata);
  }
}

TEST(PrivacyPassAthmBatchRequestTest, InvalidBatchParameters) {
  auto issuer = PrivacyPassAthmIssuer::try_new(
      kNumBuckets, rs_std::SliceRef<const uint8_t>(DeploymentIdBytes()));
  ASSERT_TRUE(issuer.has_value());

  // Zero batch size.
  IssuerConfig zero_batch_config = CreateTestIssuerConfig(*issuer, 0);
  auto zero_batch =
      PrivacyPassAthmBatchRequest::Create(zero_batch_config, kNumBuckets);
  EXPECT_FALSE(zero_batch.has_value());
  EXPECT_EQ(zero_batch.error(),
            PrivacyPassAthmBatchRequestError::kInvalidBatchSize);

  // Negative batch size.
  IssuerConfig negative_batch_config = CreateTestIssuerConfig(*issuer, -1);
  auto neg_batch =
      PrivacyPassAthmBatchRequest::Create(negative_batch_config, kNumBuckets);
  EXPECT_FALSE(neg_batch.has_value());
  EXPECT_EQ(neg_batch.error(),
            PrivacyPassAthmBatchRequestError::kInvalidBatchSize);

  // Zero buckets.
  IssuerConfig valid_config = CreateTestIssuerConfig(*issuer, 2);
  auto zero_buckets =
      PrivacyPassAthmBatchRequest::Create(valid_config, /*num_buckets=*/0);
  EXPECT_FALSE(zero_buckets.has_value());
  EXPECT_EQ(zero_buckets.error(),
            PrivacyPassAthmBatchRequestError::kInvalidBucketCount);
}

TEST(PrivacyPassAthmBatchRequestTest, SingleUseFinalization) {
  auto issuer = PrivacyPassAthmIssuer::try_new(
      kNumBuckets, rs_std::SliceRef<const uint8_t>(DeploymentIdBytes()));
  ASSERT_TRUE(issuer.has_value());

  IssuerConfig issuer_config =
      CreateTestIssuerConfig(*issuer, /*batch_size=*/1);
  base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
      batch_request =
          PrivacyPassAthmBatchRequest::Create(issuer_config, kNumBuckets);
  ASSERT_TRUE(batch_request.has_value());

  auto params = GetParametersForVersion(1);
  ASSERT_TRUE(params.has_value());
  ASSERT_EQ(batch_request->request_body().size(),
            params->single_request_size + sizeof(uint32_t));
  EXPECT_EQ(
      base::U32FromBigEndian(
          base::span(batch_request->request_body()).last<sizeof(uint32_t)>()),
      issuer_config.public_key.version());

  auto token_resp = issuer->issue_batch_from_bytes(
      rs_std::SliceRef<const uint8_t>(batch_request->request_body()),
      /*hidden_metadata=*/1);
  ASSERT_TRUE(token_resp.has_value());

  auto res1 = batch_request->Finalize(*token_resp);
  ASSERT_TRUE(res1.has_value());
  ASSERT_EQ(res1->size(), 1u);
  VerifyFinalizedTokenMetadata(*issuer, (*res1)[0],
                               /*expected_metadata=*/1);

  // Second call must fail with kAlreadyFinalized.
  auto res2 = batch_request->Finalize(*token_resp);
  EXPECT_FALSE(res2.has_value());
  EXPECT_EQ(res2.error(), PrivacyPassAthmBatchRequestError::kAlreadyFinalized);
}

TEST(PrivacyPassAthmBatchRequestTest, MalformedResponseBody) {
  auto issuer = PrivacyPassAthmIssuer::try_new(
      kNumBuckets, rs_std::SliceRef<const uint8_t>(DeploymentIdBytes()));
  ASSERT_TRUE(issuer.has_value());

  IssuerConfig issuer_config =
      CreateTestIssuerConfig(*issuer, /*batch_size=*/2);
  auto make_batch_request = [&]() {
    auto req = PrivacyPassAthmBatchRequest::Create(issuer_config, kNumBuckets);
    EXPECT_TRUE(req.has_value());
    return std::move(*req);
  };

  // Empty response body.
  auto empty_res = make_batch_request().Finalize({});
  EXPECT_FALSE(empty_res.has_value());
  EXPECT_EQ(empty_res.error(),
            PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);

  // Indivisible response body (odd number of bytes for batch_size = 2).
  std::vector<uint8_t> odd_bytes(15, 0xAA);
  auto odd_res = make_batch_request().Finalize(odd_bytes);
  EXPECT_FALSE(odd_res.has_value());
  EXPECT_EQ(odd_res.error(),
            PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);

  // Divisible by batch_size, but incorrect total length (e.g. 100 bytes per
  // token instead of expected 483 bytes for 4 buckets).
  std::vector<uint8_t> wrong_chunk_size_bytes(200, 0xAA);
  auto wrong_chunk_res = make_batch_request().Finalize(wrong_chunk_size_bytes);
  EXPECT_FALSE(wrong_chunk_res.has_value());
  EXPECT_EQ(wrong_chunk_res.error(),
            PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);

  // Expected length for 2 tokens with 4 buckets is 2 * 483 = 966 bytes.
  constexpr size_t kExpectedBatchResponseSize = 2 * 483;

  // Correct size minus one byte (truncated).
  std::vector<uint8_t> truncated_bytes(kExpectedBatchResponseSize - 1, 0xAA);
  auto truncated_res = make_batch_request().Finalize(truncated_bytes);
  EXPECT_FALSE(truncated_res.has_value());
  EXPECT_EQ(truncated_res.error(),
            PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);

  // Correct size plus one byte (extra).
  std::vector<uint8_t> extra_bytes(kExpectedBatchResponseSize + 1, 0xAA);
  auto extra_res = make_batch_request().Finalize(extra_bytes);
  EXPECT_FALSE(extra_res.has_value());
  EXPECT_EQ(extra_res.error(),
            PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);
}

}  // namespace
}  // namespace private_verification_tokens
