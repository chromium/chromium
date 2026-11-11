// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_PROBE_PAYLOAD_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_PROBE_PAYLOAD_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"

namespace stream_transport {

// Deterministic ASCII payload generator for `TransportProbe.ProbeRequest`
// (downstream) and `TransportProbe.ProbeResponse` (upstream) load-testing
// messages.
//
// Why deterministic synthesis instead of random bytes?
// Both the Google3 server (`libchrome_transport`) and the Chromium client
// (`browser_actuator`) synthesize the exact same byte sequence from
// `(probe_id, stream_discriminator, payload_size_bytes)` alone. Either endpoint
// can independently reconstruct and verify any `0 B .. 10 MB` payload
// bit-for-bit without transmitting a secondary hash or bundling large test
// fixtures.
//
// Payload anatomy:
//   - When `payload_size_bytes == 0`: empty string `""`.
//   - When `payload_size_bytes >= prefix.size() + kMinProbeEntropyChars` (8):
//     starts with a colon-delimited, human-readable ASCII header (`prefix`) so
//     packet captures, logs, and `chrome://browser-actuator-internals` remain
//     self-describing even when truncated, followed by deterministic Base32
//     characters filling the remaining bytes:
//       Request:  "PROBE:<safe_id>:req:<size>:<base32...>"
//       Response: "PROBE:<safe_id>:<index>/<total>:<size>:<base32...>"
//       Example:  "PROBE:p-42:0/2:64:FMLGM6T5AZEDFDKRNX5VV7TB4BTSSHZ36VZU..."
//   - When `0 < payload_size_bytes < prefix.size() + kMinProbeEntropyChars`:
//     omits `prefix` and emits `payload_size_bytes` pure Base32 characters
//     (e.g. "FMLGM6T5AZEDFDKR" for 16 bytes) so every byte length down to 1 is
//     supported and carries seed-dependent content.
//
// Generation pipeline (see `probe_payload.cc` for step-by-step details):
//   1. Seed derivation: 64-bit FNV-1a hash of the full `probe_id`, combined
//      with `stream_discriminator` (`UINT64_MAX` for requests;
//      `0 .. total_responses - 1` for responses) via Boost's 64-bit
//      `hash_combine`.
//   2. PRNG expansion: `SplitMix64` (64-bit golden-ratio Weyl sequence +
//      Stafford's `Mix13` avalanche finalizer).
//   3. Base32 encoding: slices each 64-bit word into 12 five-bit indices
//      (`word & 0x1F`, `word >>= 5`) into `kProbeBase32Alphabet` with zero
//      modulo bias.
//
// Safety and framing constants shared across the client and server-side
// `TransportProbe.ProbeRequest` / `TransportProbe.ProbeResponse`
// implementations.
//
// Any change to the alphabet, FNV-1a / SplitMix64 constants, or prefix-fit
// threshold must update the server-side implementation in lockstep so
// cross-endpoint payload verification remains bit-for-bit deterministic.
inline constexpr int32_t kMaxProbePayloadBytes = 10'000'000;
inline constexpr int32_t kMaxProbeResponses = 16;
inline constexpr int64_t kMaxTotalProbeResponseBytes = 20'000'000;
inline constexpr int32_t kMaxProbeDelayMs = 30'000;
inline constexpr size_t kMaxProbeIdBytes = 256;
inline constexpr size_t kMaxSanitizedProbeIdChars = 32;
inline constexpr size_t kMinProbeEntropyChars = 8;

// Unambiguous 32-character ASCII alphabet (drops `0`/`O` and `1`/`I`). Every
// character is a single UTF-8 byte, so character count equals wire byte length.
inline constexpr std::string_view kProbeBase32Alphabet =
    "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

// Describes one requested upstream response when constructing a `ProbeRequest`
// via `BuildProbeRequest()`.
struct ProbeResponseSpecInput {
  int32_t payload_size_bytes = 0;
  int32_t delay_ms = 0;
};

// Validates `request` against all transport safety caps (`kMaxProbeIdBytes`,
// `kMaxProbePayloadBytes`, `kMaxProbeResponses`, `kMaxTotalProbeResponseBytes`,
// and `kMaxProbeDelayMs`). Returns an error message on the first violated
// bound.
base::expected<void, std::string> ValidateProbeRequest(
    const browser_actuator::TransportProbe::ProbeRequest& request);

// Generates the deterministic ASCII payload of exact byte length
// `payload_size_bytes` for a `ProbeRequest` identified by `probe_id`.
//
// When `payload_size_bytes == 0`, returns `""`. When `payload_size_bytes` can
// accommodate `"PROBE:<safe_id>:req:<size>:"` plus at least
// `kMinProbeEntropyChars` random characters, prepends that self-describing
// header; otherwise emits `payload_size_bytes` pure Base32 characters.
base::expected<std::string, std::string> GenerateProbeRequestPayload(
    std::string_view probe_id,
    int32_t payload_size_bytes);

// Generates the deterministic ASCII payload of exact byte length
// `payload_size_bytes` for response `response_index` (`0 .. total_responses -
// 1`) of `probe_id`.
//
// When `payload_size_bytes == 0`, returns `""`. When `payload_size_bytes` can
// accommodate `"PROBE:<safe_id>:<index>/<total>:<size>:"` plus at least
// `kMinProbeEntropyChars` random characters, prepends that self-describing
// header; otherwise emits `payload_size_bytes` pure Base32 characters.
base::expected<std::string, std::string> GenerateProbeResponsePayload(
    std::string_view probe_id,
    int32_t response_index,
    int32_t total_responses,
    int32_t payload_size_bytes);

// Builds and validates a ready-to-send `TransportProbe::ProbeRequest` with a
// deterministic request payload of `request_payload_size_bytes` and the
// requested `response_specs`.
base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
BuildProbeRequest(std::string_view probe_id,
                  int32_t request_payload_size_bytes = 0,
                  base::span<const ProbeResponseSpecInput> response_specs = {});

// Returns true if `actual_payload` matches the deterministic request payload
// expected for `(probe_id, expected_payload_size_bytes)`.
bool VerifyProbeRequestPayload(std::string_view probe_id,
                               int32_t expected_payload_size_bytes,
                               std::string_view actual_payload);

// Returns true if `response` carries a valid index, matching `probe_id` and
// `total_responses`, and the exact deterministic payload requested by the
// corresponding entry in `request.response_specs()`.
bool VerifyProbeResponsePayload(
    const browser_actuator::TransportProbe::ProbeRequest& request,
    const browser_actuator::TransportProbe::ProbeResponse& response);

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_PROBE_PAYLOAD_H_
