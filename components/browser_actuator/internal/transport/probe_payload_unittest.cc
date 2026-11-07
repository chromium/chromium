// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/probe_payload.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;

bool UsesOnlyUnambiguousBase32Alphabet(std::string_view characters) {
  for (const char character : characters) {
    if (!kProbeBase32Alphabet.contains(character)) {
      return false;
    }
  }
  return true;
}

TEST(ProbePayloadTest, ZeroByteRequestAndResponsePayloadsAreEmpty) {
  EXPECT_THAT(GenerateProbeRequestPayload("p-42", /*payload_size_bytes=*/0),
              ValueIs(IsEmpty()));
  EXPECT_THAT(GenerateProbeResponsePayload("p-42", /*response_index=*/0,
                                           /*total_responses=*/2,
                                           /*payload_size_bytes=*/0),
              ValueIs(IsEmpty()));
}

TEST(ProbePayloadTest, OmitsPrefixWhenPayloadCannotFitHeaderPlusEightChars) {
  // For probe_id "p-42", index 0/2, size 25:
  // Prefix "PROBE:p-42:0/2:25:" is 18 bytes; 18 + 8 = 26 > 25, so the prefix
  // must be omitted in favor of pure Base32 characters.
  ASSERT_OK_AND_ASSIGN(
      const std::string tiny_one_byte,
      GenerateProbeResponsePayload("p-42", 0, 2, /*payload_size_bytes=*/1));
  EXPECT_THAT(tiny_one_byte, SizeIs(1));
  EXPECT_TRUE(UsesOnlyUnambiguousBase32Alphabet(tiny_one_byte));

  ASSERT_OK_AND_ASSIGN(
      const std::string just_below_threshold,
      GenerateProbeResponsePayload("p-42", 0, 2, /*payload_size_bytes=*/25));
  EXPECT_THAT(just_below_threshold, SizeIs(25));
  EXPECT_FALSE(just_below_threshold.starts_with("PROBE:"));
  EXPECT_TRUE(UsesOnlyUnambiguousBase32Alphabet(just_below_threshold));
}

TEST(ProbePayloadTest, PrependsSelfDescribingPrefixAtExactFitBoundary) {
  // At size 26, prefix "PROBE:p-42:0/2:26:" is 18 bytes and 18 + 8 == 26.
  ASSERT_OK_AND_ASSIGN(
      const std::string at_threshold,
      GenerateProbeResponsePayload("p-42", 0, 2, /*payload_size_bytes=*/26));
  EXPECT_THAT(at_threshold, SizeIs(26));
  EXPECT_THAT(at_threshold, StartsWith("PROBE:p-42:0/2:26:"));
  EXPECT_TRUE(UsesOnlyUnambiguousBase32Alphabet(
      std::string_view(at_threshold)
          .substr(std::string_view("PROBE:p-42:0/2:26:").size())));

  ASSERT_OK_AND_ASSIGN(
      const std::string request_below,
      GenerateProbeRequestPayload("p-42", /*payload_size_bytes=*/25));
  EXPECT_THAT(request_below, SizeIs(25));
  EXPECT_FALSE(request_below.starts_with("PROBE:"));

  ASSERT_OK_AND_ASSIGN(
      const std::string request_at_threshold,
      GenerateProbeRequestPayload("p-42", /*payload_size_bytes=*/26));
  EXPECT_THAT(request_at_threshold, SizeIs(26));
  EXPECT_THAT(request_at_threshold, StartsWith("PROBE:p-42:req:26:"));
}

TEST(ProbePayloadTest, SanitizesColonsSpacesAndLongIdsInPrefixOnly) {
  const std::string raw_id_a =
      "id:with spaces/and\tcontrol\nchars_plus_extra_tail_beyond_32_bytes_A";
  const std::string raw_id_b =
      "id:with spaces/and\tcontrol\nchars_plus_extra_tail_beyond_32_bytes_B";

  ASSERT_OK_AND_ASSIGN(const std::string payload_a,
                       GenerateProbeResponsePayload(raw_id_a, 0, 1, 96));
  ASSERT_OK_AND_ASSIGN(const std::string payload_b,
                       GenerateProbeResponsePayload(raw_id_b, 0, 1, 96));

  constexpr char kExpectedPrefix[] =
      "PROBE:id_with_spaces/and_control_chars:0/1:96:";
  EXPECT_THAT(payload_a, StartsWith(kExpectedPrefix));
  EXPECT_THAT(payload_b, StartsWith(kExpectedPrefix));
  EXPECT_NE(payload_a, payload_b);
}

TEST(ProbePayloadTest, MatchesCrossRepositoryGoldenVectors) {
  // Pinned identically in the server-side transport's `probe_payload_test.cc`
  // so both endpoints never drift in seed derivation or Base32 framing.
  EXPECT_THAT(
      GenerateProbeRequestPayload("p-42", /*payload_size_bytes=*/64),
      ValueIs(
          "PROBE:p-42:req:64:83BKGWJRGRX6QEMKCLRHR6DBJ4A743T9EMJXGSVZVAGEED"));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", /*response_index=*/0,
                                   /*total_responses=*/2,
                                   /*payload_size_bytes=*/64),
      ValueIs(
          "PROBE:p-42:0/2:64:FMLGM6T5AZEDFDKRNX5VV7TB4BTSSHZ36VZUJPYMPAU8S9"));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", /*response_index=*/1,
                                   /*total_responses=*/2,
                                   /*payload_size_bytes=*/64),
      ValueIs(
          "PROBE:p-42:1/2:64:UFC3AHSHL4PY8E6B39RNQP9MGW53SK83HL9LDF96TZWRMM"));
  EXPECT_THAT(GenerateProbeResponsePayload("p-42", /*response_index=*/0,
                                           /*total_responses=*/2,
                                           /*payload_size_bytes=*/16),
              ValueIs("FMLGM6T5AZEDFDKR"));
}

TEST(ProbePayloadTest, GeneratesExactTenMegabytePayloadWithinCap) {
  ASSERT_OK_AND_ASSIGN(
      const std::string max_payload,
      GenerateProbeResponsePayload("load-test", 0, 1, kMaxProbePayloadBytes));
  EXPECT_EQ(max_payload.size(), static_cast<size_t>(kMaxProbePayloadBytes));
  EXPECT_THAT(max_payload, StartsWith("PROBE:load-test:0/1:10000000:"));
}

TEST(ProbePayloadTest, GenerateProbePayloadRejectsOutOfBoundsInputs) {
  const std::string oversized_id(kMaxProbeIdBytes + 1, 'a');
  EXPECT_THAT(GenerateProbeRequestPayload(oversized_id, 16),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(GenerateProbeRequestPayload("neg-req", /*payload_size_bytes=*/-1),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(
      GenerateProbeRequestPayload("over-req", kMaxProbePayloadBytes + 1),
      ErrorIs(Not(IsEmpty())));

  EXPECT_THAT(GenerateProbeResponsePayload(oversized_id, 0, 1, 16),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", 0, /*total_responses=*/0, 16),
      ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(GenerateProbeResponsePayload(
                  "p-42", 0, /*total_responses=*/kMaxProbeResponses + 1, 16),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", /*response_index=*/-1, 2, 16),
      ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(GenerateProbeResponsePayload("p-42", /*response_index=*/2, 2, 16),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", 0, 1, /*payload_size_bytes=*/-1),
      ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(
      GenerateProbeResponsePayload("p-42", 0, 1, kMaxProbePayloadBytes + 1),
      ErrorIs(Not(IsEmpty())));
}

}  // namespace
}  // namespace browser_actuator::transport
