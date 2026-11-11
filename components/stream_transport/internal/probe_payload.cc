// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/probe_payload.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"

namespace stream_transport {
namespace {

// 64-bit FNV-1a parameters from the IETF FNV specification
// (draft-eastlake-fnv). `kFnv1aOffsetBasis` (0xcbf29ce484222325) is the
// non-zero initial state produced by FNV-0 of the 32-byte signature
// "chongo <Landon Curt Noll> /\../\"; `kFnv1aPrime` (2^40 + 2^8 + 0xb3 =
// 0x100000001b3) is the smallest prime of that sparse form that disperses
// single-byte differences across all 64 bits.
constexpr uint64_t kFnv1aOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnv1aPrime = 1099511628211ULL;

// Odd integer closest to 2^64 / phi (where phi = (1 + sqrt(5)) / 2 is the
// golden ratio). Because gcd(kGoldenRatio64, 2^64) == 1, adding it modulo 2^64
// visits all 2^64 states with maximal equidistribution before repeating. Used
// both in Boost's 64-bit `hash_combine` step and as the Weyl-sequence increment
// in `SplitMix64`.
constexpr uint64_t kGoldenRatio64 = 0x9E3779B97F4A7C15ULL;

// David Stafford's "Mix13" 64-bit bijective finalizer constants, adopted by
// Sebastiano Vigna's `SplitMix64` PRNG (Steele, Lea, & Flood, "Fast
// Splittable Pseudorandom Number Generators", OOPSLA 2014). Each round XORs
// the upper bits down (`>> 30`, `>> 27`, `>> 31`) and multiplies by an odd
// constant chosen by simulated annealing so flipping any input bit flips ~50%
// of the output bits (full 64-bit avalanche).
constexpr uint64_t kSplitMix64Multiplier1 = 0xBF58476D1CE4E5B9ULL;
constexpr uint64_t kSplitMix64Multiplier2 = 0x94D049BB133111EBULL;
constexpr int kSplitMix64Shift1 = 30;
constexpr int kSplitMix64Shift2 = 27;
constexpr int kSplitMix64Shift3 = 31;

// Base32 bit-slicing parameters. `kProbeBase32Alphabet` has 32 = 2^5 symbols,
// so each symbol consumes exactly 5 bits (`word & kBase32SymbolMask`). A
// 64-bit `SplitMix64` word yields floor(64 / 5) = 12 five-bit symbols (60 bits)
// with zero modulo bias; the remaining 4 high bits are discarded.
constexpr size_t kBase32BitsPerSymbol = 5;
constexpr uint64_t kBase32SymbolMask = 0x1F;
constexpr size_t kBase32SymbolsPerWord = 12;

static_assert(kProbeBase32Alphabet.size() ==
              (size_t{1} << kBase32BitsPerSymbol));
static_assert(kBase32SymbolMask == (uint64_t{1} << kBase32BitsPerSymbol) - 1);
static_assert(kBase32SymbolsPerWord == 64 / kBase32BitsPerSymbol);

// Distinguishes the single `ProbeRequest` payload stream from `ProbeResponse`
// payload streams, which use `response_index` in `0 .. kMaxProbeResponses - 1`
// as their stream discriminator.
constexpr uint64_t kRequestStreamDiscriminator =
    std::numeric_limits<uint64_t>::max();

base::expected<void, std::string> ValidateMaxCount(uint64_t actual,
                                                   uint64_t maximum,
                                                   std::string_view label) {
  if (actual > maximum) {
    return base::unexpected(base::StrCat(
        {label, " (", base::NumberToString(actual), ") exceeds maximum (",
         base::NumberToString(maximum), ")."}));
  }
  return base::ok();
}

base::expected<void, std::string> ValidateInclusiveBounds(
    int64_t value,
    int64_t minimum,
    int64_t maximum,
    std::string_view label) {
  if (value < minimum || value > maximum) {
    return base::unexpected(
        base::StrCat({label, " (", base::NumberToString(value),
                      ") must be in [", base::NumberToString(minimum), ", ",
                      base::NumberToString(maximum), "]."}));
  }
  return base::ok();
}

base::expected<void, std::string> ValidateResponseSpecFieldBounds(
    int index,
    std::string_view field_name,
    int64_t value,
    int64_t minimum,
    int64_t maximum) {
  if (value < minimum || value > maximum) {
    return ValidateInclusiveBounds(
        value, minimum, maximum,
        base::StrCat({"ProbeRequest response_specs[",
                      base::NumberToString(index), "] ", field_name}));
  }
  return base::ok();
}

// Clamps `probe_id` to `kMaxSanitizedProbeIdChars` and replaces any byte
// outside printable non-colon ASCII (`[0x21, 0x7E]` minus `':'`) with `'_'` so
// the colon-delimited `PROBE:...` prefix stays unambiguous to parse.
std::string SanitizeProbeId(std::string_view probe_id) {
  const std::string_view clamped =
      probe_id.substr(0, kMaxSanitizedProbeIdChars);
  std::string sanitized;
  sanitized.reserve(clamped.size());
  for (const char character : clamped) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte >= 0x21 && byte <= 0x7E && byte != ':') {
      sanitized.push_back(static_cast<char>(byte));
    } else {
      sanitized.push_back('_');
    }
  }
  return sanitized;
}

// Stage 1: Derives a deterministic, cross-platform 64-bit PRNG seed from the
// full unsanitized `probe_id` and `stream_discriminator`.
//
// Why not `std::hash` or `base::FastHash`? Standard-library and `//base`
// hashes are implementation-defined or process-seeded, so they produce
// different values between Chromium (`libc++`) and the Google3 server
// (`libstdc++` / Abseil).
//
// Algorithm:
//   1. Hash every byte of `probe_id` with 64-bit FNV-1a (XOR byte into the
//      low 8 bits, then multiply by `kFnv1aPrime` modulo 2^64).
//   2. Fold in `stream_discriminator` (`kRequestStreamDiscriminator` for the
//      request stream, or `0 .. kMaxProbeResponses - 1` for each response
//      stream) using Boost's 64-bit `hash_combine` mixing expression
//      (`hash ^= v + kGoldenRatio64 + (hash << 6) + (hash >> 2)`), ensuring the
//      request and each response for the same `probe_id` start from
//      uncorrelated 64-bit seeds.
uint64_t ComputeProbeSeed(std::string_view probe_id,
                          uint64_t stream_discriminator) {
  uint64_t hash = kFnv1aOffsetBasis;
  for (const char character : probe_id) {
    hash ^= static_cast<unsigned char>(character);
    hash *= kFnv1aPrime;
  }
  hash ^= stream_discriminator + kGoldenRatio64 + (hash << 6) + (hash >> 2);
  return hash;
}

// Stage 2: Advances `state` by one step of Sebastiano Vigna's `SplitMix64`
// PRNG and returns a mixed 64-bit pseudorandom word.
//
// Why `SplitMix64`? C++ `<random>` engines (`std::mt19937_64`) are banned by
// the Chromium C++ style guide (`styleguide/c++/c++-features.md`) and carry
// 2.5 KB of state. `SplitMix64` requires a single `uint64_t` of state, passes
// TestU01 BigCrush, and generates a 10 MB payload in a few milliseconds:
//   1. Advance `state` along an additive Weyl sequence (`+= kGoldenRatio64`).
//   2. Apply David Stafford's `Mix13` two-round xorshift-multiply bijective
//      finalizer so adjacent Weyl states decorrelate across all 64 bits.
uint64_t NextSplitMix64(uint64_t& state) {
  state += kGoldenRatio64;
  uint64_t mixed = state;
  mixed = (mixed ^ (mixed >> kSplitMix64Shift1)) * kSplitMix64Multiplier1;
  mixed = (mixed ^ (mixed >> kSplitMix64Shift2)) * kSplitMix64Multiplier2;
  return mixed ^ (mixed >> kSplitMix64Shift3);
}

// Stage 3: Appends `byte_count` deterministic Base32 characters derived from
// `seed` to `output`.
//
// Because `kProbeBase32Alphabet` has exactly 32 = 2^5 characters, each 5-bit
// chunk (`word & kBase32SymbolMask`) selects an alphabet index in `[0, 31]`
// with zero modulo bias and no integer division. Each 64-bit `SplitMix64` word
// supplies `kBase32SymbolsPerWord` (12) five-bit symbols from least-significant
// bit upward (consuming 60 bits and discarding the top 4 bits) before the next
// word is drawn.
void AppendBase32Stream(uint64_t seed, size_t byte_count, std::string& output) {
  uint64_t prng_state = seed;
  size_t remaining_bytes = byte_count;
  while (remaining_bytes > 0) {
    uint64_t word = NextSplitMix64(prng_state);
    const size_t chunk_length =
        std::min<size_t>(remaining_bytes, kBase32SymbolsPerWord);
    for (size_t i = 0; i < chunk_length; ++i) {
      output.push_back(
          kProbeBase32Alphabet[static_cast<size_t>(word & kBase32SymbolMask)]);
      word >>= kBase32BitsPerSymbol;
    }
    remaining_bytes -= chunk_length;
  }
}

// Synthesizes a payload of exact length `payload_size_bytes` (> 0).
//
// If `payload_size_bytes` can fit the human-readable `prefix` plus at least
// `kMinProbeEntropyChars` (8 characters = 40 bits of seed-dependent Base32
// output), prepends `prefix` and fills the remainder from the Base32 stream.
// Otherwise omits `prefix` and fills all `payload_size_bytes` from the Base32
// stream so tiny payloads (`1 .. prefix.size() + 7` bytes) still carry
// seed-dependent content.
std::string BuildDeterministicPayload(std::string_view probe_id,
                                      uint64_t stream_discriminator,
                                      std::string_view prefix,
                                      int32_t payload_size_bytes) {
  CHECK_GT(payload_size_bytes, 0);
  const auto target_length = static_cast<size_t>(payload_size_bytes);
  std::string payload;
  payload.reserve(target_length);

  const uint64_t seed = ComputeProbeSeed(probe_id, stream_discriminator);
  if (target_length >= prefix.size() + kMinProbeEntropyChars) {
    payload.append(prefix);
    AppendBase32Stream(seed, target_length - prefix.size(), payload);
  } else {
    AppendBase32Stream(seed, target_length, payload);
  }
  return payload;
}

}  // namespace

base::expected<void, std::string> ValidateProbeRequest(
    const browser_actuator::TransportProbe::ProbeRequest& request) {
  RETURN_IF_ERROR(ValidateMaxCount(request.probe_id().size(), kMaxProbeIdBytes,
                                   "ProbeRequest probe_id length"));
  RETURN_IF_ERROR(ValidateMaxCount(request.payload().size(),
                                   static_cast<uint64_t>(kMaxProbePayloadBytes),
                                   "ProbeRequest payload size"));
  RETURN_IF_ERROR(
      ValidateMaxCount(static_cast<uint64_t>(request.response_specs_size()),
                       static_cast<uint64_t>(kMaxProbeResponses),
                       "ProbeRequest response_specs count"));

  int64_t total_response_bytes = 0;
  for (int i = 0; i < request.response_specs_size(); ++i) {
    const browser_actuator::TransportProbe::ProbeRequest::ResponseSpec& spec =
        request.response_specs(i);
    RETURN_IF_ERROR(ValidateResponseSpecFieldBounds(i, "payload_size_bytes",
                                                    spec.payload_size_bytes(),
                                                    0, kMaxProbePayloadBytes));
    RETURN_IF_ERROR(ValidateResponseSpecFieldBounds(
        i, "delay_ms", spec.delay_ms(), 0, kMaxProbeDelayMs));
    total_response_bytes += spec.payload_size_bytes();
    RETURN_IF_ERROR(
        ValidateMaxCount(static_cast<uint64_t>(total_response_bytes),
                         static_cast<uint64_t>(kMaxTotalProbeResponseBytes),
                         "ProbeRequest total requested response bytes"));
  }
  return base::ok();
}

base::expected<std::string, std::string> GenerateProbeRequestPayload(
    std::string_view probe_id,
    int32_t payload_size_bytes) {
  RETURN_IF_ERROR(ValidateMaxCount(probe_id.size(), kMaxProbeIdBytes,
                                   "ProbeRequest probe_id length"));
  RETURN_IF_ERROR(ValidateInclusiveBounds(payload_size_bytes, 0,
                                          kMaxProbePayloadBytes,
                                          "ProbeRequest payload_size_bytes"));
  if (payload_size_bytes == 0) {
    return base::ok(std::string());
  }
  const std::string prefix =
      base::StrCat({"PROBE:", SanitizeProbeId(probe_id),
                    ":req:", base::NumberToString(payload_size_bytes), ":"});
  return base::ok(BuildDeterministicPayload(
      probe_id, kRequestStreamDiscriminator, prefix, payload_size_bytes));
}

base::expected<std::string, std::string> GenerateProbeResponsePayload(
    std::string_view probe_id,
    int32_t response_index,
    int32_t total_responses,
    int32_t payload_size_bytes) {
  RETURN_IF_ERROR(ValidateMaxCount(probe_id.size(), kMaxProbeIdBytes,
                                   "ProbeResponse probe_id length"));
  RETURN_IF_ERROR(ValidateInclusiveBounds(
      total_responses, 1, kMaxProbeResponses, "ProbeResponse total_responses"));
  RETURN_IF_ERROR(ValidateInclusiveBounds(
      response_index, 0, total_responses - 1, "ProbeResponse response_index"));
  RETURN_IF_ERROR(ValidateInclusiveBounds(payload_size_bytes, 0,
                                          kMaxProbePayloadBytes,
                                          "ProbeResponse payload_size_bytes"));
  if (payload_size_bytes == 0) {
    return base::ok(std::string());
  }
  const std::string prefix =
      base::StrCat({"PROBE:", SanitizeProbeId(probe_id), ":",
                    base::NumberToString(response_index), "/",
                    base::NumberToString(total_responses), ":",
                    base::NumberToString(payload_size_bytes), ":"});
  return base::ok(
      BuildDeterministicPayload(probe_id, static_cast<uint64_t>(response_index),
                                prefix, payload_size_bytes));
}

base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
BuildProbeRequest(std::string_view probe_id,
                  int32_t request_payload_size_bytes,
                  base::span<const ProbeResponseSpecInput> response_specs) {
  ASSIGN_OR_RETURN(
      std::string payload,
      GenerateProbeRequestPayload(probe_id, request_payload_size_bytes));

  browser_actuator::TransportProbe::ProbeRequest request;
  request.set_probe_id(std::string(probe_id));
  request.set_payload(std::move(payload));
  for (const ProbeResponseSpecInput& spec_input : response_specs) {
    browser_actuator::TransportProbe::ProbeRequest::ResponseSpec* spec =
        request.add_response_specs();
    spec->set_payload_size_bytes(spec_input.payload_size_bytes);
    spec->set_delay_ms(spec_input.delay_ms);
  }

  RETURN_IF_ERROR(ValidateProbeRequest(request));
  return request;
}

bool VerifyProbeRequestPayload(std::string_view probe_id,
                               int32_t expected_payload_size_bytes,
                               std::string_view actual_payload) {
  if (expected_payload_size_bytes < 0 ||
      actual_payload.size() !=
          static_cast<size_t>(expected_payload_size_bytes)) {
    return false;
  }
  base::expected<std::string, std::string> expected_payload =
      GenerateProbeRequestPayload(probe_id, expected_payload_size_bytes);
  return expected_payload.has_value() && *expected_payload == actual_payload;
}

bool VerifyProbeResponsePayload(
    const browser_actuator::TransportProbe::ProbeRequest& request,
    const browser_actuator::TransportProbe::ProbeResponse& response) {
  if (response.probe_id() != request.probe_id()) {
    return false;
  }
  const int32_t total_responses = request.response_specs_size();
  if (total_responses <= 0 || response.total_responses() != total_responses) {
    return false;
  }
  const int32_t index = response.response_index();
  if (index < 0 || index >= total_responses) {
    return false;
  }
  const int32_t expected_size =
      request.response_specs(index).payload_size_bytes();
  if (expected_size < 0 ||
      response.payload().size() != static_cast<size_t>(expected_size)) {
    return false;
  }
  base::expected<std::string, std::string> expected_payload =
      GenerateProbeResponsePayload(request.probe_id(), index, total_responses,
                                   expected_size);
  return expected_payload.has_value() &&
         *expected_payload == response.payload();
}

}  // namespace stream_transport
