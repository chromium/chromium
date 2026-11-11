// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/probe_payload.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
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

TEST(ProbePayloadTest, BuildAndVerifyRoundTripSucceedsAndDetectsCorruption) {
  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 0, .delay_ms = 0},
      {.payload_size_bytes = 4096, .delay_ms = 25},
  };
  ASSERT_OK_AND_ASSIGN(
      TransportProbe::ProbeRequest request,
      BuildProbeRequest("probe-rtt-1", /*request_payload_size_bytes=*/128,
                        specs));

  EXPECT_TRUE(VerifyProbeRequestPayload("probe-rtt-1", 128, request.payload()));
  EXPECT_FALSE(VerifyProbeRequestPayload("wrong-id", 128, request.payload()));
  EXPECT_FALSE(VerifyProbeRequestPayload("probe-rtt-1", -1, request.payload()));
  EXPECT_FALSE(VerifyProbeRequestPayload("probe-rtt-1", 64, request.payload()));

  TransportProbe::ProbeResponse response_0;
  response_0.set_probe_id("probe-rtt-1");
  response_0.set_response_index(0);
  response_0.set_total_responses(2);
  response_0.set_payload("");
  EXPECT_TRUE(VerifyProbeResponsePayload(request, response_0));

  ASSERT_OK_AND_ASSIGN(const std::string payload_1,
                       GenerateProbeResponsePayload("probe-rtt-1", 1, 2, 4096));
  TransportProbe::ProbeResponse response_1;
  response_1.set_probe_id("probe-rtt-1");
  response_1.set_response_index(1);
  response_1.set_total_responses(2);
  response_1.set_payload(payload_1);
  EXPECT_TRUE(VerifyProbeResponsePayload(request, response_1));

  std::string corrupted = payload_1;
  corrupted.back() = (corrupted.back() == 'Z') ? '2' : 'Z';
  response_1.set_payload(corrupted);
  EXPECT_FALSE(VerifyProbeResponsePayload(request, response_1));

  response_1.set_payload(payload_1);
  TransportProbe::ProbeResponse wrong_id = response_1;
  wrong_id.set_probe_id("other-id");
  EXPECT_FALSE(VerifyProbeResponsePayload(request, wrong_id));

  TransportProbe::ProbeResponse wrong_total = response_1;
  wrong_total.set_total_responses(3);
  EXPECT_FALSE(VerifyProbeResponsePayload(request, wrong_total));

  TransportProbe::ProbeResponse negative_index = response_1;
  negative_index.set_response_index(-1);
  EXPECT_FALSE(VerifyProbeResponsePayload(request, negative_index));

  TransportProbe::ProbeResponse out_of_range = response_1;
  out_of_range.set_response_index(2);
  EXPECT_FALSE(VerifyProbeResponsePayload(request, out_of_range));

  TransportProbe::ProbeResponse truncated = response_1;
  truncated.mutable_payload()->pop_back();
  EXPECT_FALSE(VerifyProbeResponsePayload(request, truncated));
}

TEST(ProbePayloadTest, GenerateProbePayloadRejectsOutOfBoundsInputs) {
  const std::string oversized_id(kMaxProbeIdBytes + 1, 'a');
  EXPECT_THAT(GenerateProbeRequestPayload(oversized_id, 16),
              ErrorIs(Not(IsEmpty())));
  EXPECT_THAT(BuildProbeRequest("neg-req", /*request_payload_size_bytes=*/-1),
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

TEST(ProbePayloadTest, ValidateProbeRequestEnforcesAllSafetyBounds) {
  TransportProbe::ProbeRequest empty_valid;
  EXPECT_THAT(ValidateProbeRequest(empty_valid), HasValue());

  TransportProbe::ProbeRequest oversized_id;
  oversized_id.set_probe_id(std::string(kMaxProbeIdBytes + 1, 'a'));
  EXPECT_THAT(ValidateProbeRequest(oversized_id), ErrorIs(Not(IsEmpty())));

  TransportProbe::ProbeRequest oversized_payload;
  oversized_payload.set_payload(
      std::string(static_cast<size_t>(kMaxProbePayloadBytes) + 1, 'A'));
  EXPECT_THAT(ValidateProbeRequest(oversized_payload), ErrorIs(Not(IsEmpty())));

  const ProbeResponseSpecInput neg_spec[] = {{.payload_size_bytes = -1}};
  EXPECT_THAT(BuildProbeRequest("neg", 0, neg_spec), ErrorIs(Not(IsEmpty())));

  const ProbeResponseSpecInput over_spec[] = {
      {.payload_size_bytes = kMaxProbePayloadBytes + 1}};
  EXPECT_THAT(BuildProbeRequest("over", 0, over_spec), ErrorIs(Not(IsEmpty())));

  TransportProbe::ProbeRequest too_many_specs;
  for (int i = 0; i < kMaxProbeResponses + 1; ++i) {
    too_many_specs.add_response_specs()->set_payload_size_bytes(0);
  }
  EXPECT_THAT(ValidateProbeRequest(too_many_specs), ErrorIs(Not(IsEmpty())));

  const ProbeResponseSpecInput sum_ok_specs[] = {
      {.payload_size_bytes = kMaxProbePayloadBytes},
      {.payload_size_bytes = kMaxProbePayloadBytes},
  };
  EXPECT_THAT(BuildProbeRequest("sum-20mb-ok", 0, sum_ok_specs), HasValue());

  const ProbeResponseSpecInput sum_over_specs[] = {
      {.payload_size_bytes = kMaxProbePayloadBytes},
      {.payload_size_bytes = kMaxProbePayloadBytes},
      {.payload_size_bytes = 1},
  };
  EXPECT_THAT(BuildProbeRequest("sum-over-20mb", 0, sum_over_specs),
              ErrorIs(Not(IsEmpty())));

  const ProbeResponseSpecInput neg_delay[] = {{.delay_ms = -1}};
  EXPECT_THAT(BuildProbeRequest("neg-delay", 0, neg_delay),
              ErrorIs(Not(IsEmpty())));

  const ProbeResponseSpecInput over_delay[] = {
      {.delay_ms = kMaxProbeDelayMs + 1}};
  EXPECT_THAT(BuildProbeRequest("over-delay", 0, over_delay),
              ErrorIs(Not(IsEmpty())));
}

}  // namespace
}  // namespace browser_actuator::transport
