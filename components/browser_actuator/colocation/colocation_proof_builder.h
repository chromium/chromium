// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_BUILDER_H_
#define COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_BUILDER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "base/containers/span.h"

// Pure helpers for the macOS co-location proof: the bytes Chrome signs with a
// per-profile key, and the serialized `Any` it writes to the Keychain so a
// Google-signed companion app can forward it to the backend. The signer and
// Keychain writer live in chrome/browser/browser_actuator/colocation/
// (`ColocationProofService`, follow-up CL); the backend verifies the signature
// against the profile's synced public key. See b/534995082.

namespace browser_actuator {

// Purpose label signed ahead of the timestamp. Not stored or sent; the
// verifier hard-codes the same constant. The NUL terminator is not signed.
inline constexpr char kColocationProofLabel[] = "Chrome co-location proof v1";
inline constexpr size_t kColocationProofLabelSize =
    sizeof(kColocationProofLabel) - 1;  // 27
inline constexpr size_t kColocationProofPayloadSize =
    kColocationProofLabelSize + sizeof(int64_t);  // 35

// Returns `kColocationProofLabel` (27 bytes, no NUL) concatenated with the
// 8-byte big-endian encoding of `seconds`: the exact 35 bytes Chrome signs.
std::array<uint8_t, kColocationProofPayloadSize> ColocationProofPayload(
    int64_t seconds);

// Wraps `signature` and `seconds` in a `ChromeColocationProof`, packs it
// inside a `google.protobuf.Any`, and returns the serialized `Any`.
std::string SerializeColocationProofAsAny(base::span<const uint8_t> signature,
                                          int64_t seconds);

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_BUILDER_H_
