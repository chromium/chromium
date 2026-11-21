// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <stdint.h>

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_reader_writer.h"
#include "components/variations/variations_safe_seed_store.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mldsa.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace variations {
namespace {

// The legacy ECDSA P-256 public key (Key ID 0) of the variations server for
// verifying variations seed signatures.
const uint8_t kPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x51, 0x7c, 0x31, 0x4b, 0x50, 0x42, 0xdd, 0x59, 0xda,
    0x0b, 0xfa, 0x43, 0x44, 0x33, 0x7c, 0x5f, 0xa1, 0x0b, 0xd5, 0x82, 0xf6,
    0xac, 0x04, 0x19, 0x72, 0x6c, 0x40, 0xd4, 0x3e, 0x56, 0xe2, 0xa0, 0x80,
    0xa0, 0x41, 0xb3, 0x23, 0x7b, 0x71, 0xc9, 0x80, 0x87, 0xde, 0x35, 0x0d,
    0x25, 0x71, 0x09, 0x7f, 0xb4, 0x15, 0x2b, 0xff, 0x82, 0x4d, 0xd3, 0xfe,
    0xc5, 0xef, 0x20, 0xc6, 0xa3, 0x10, 0xbf,
};

// Constants for Tink COMPOSITE_SIGN_MLDSA_44_ECDSA_P256_TINK_RAW verification.
constexpr std::string_view kCompositeSignaturePrefix =
    "CompositeAlgorithmSignatures2025";
constexpr std::string_view kCompositeSignatureLabel =
    "COMPSIG-MLDSA44-ECDSA-P256-SHA256";
constexpr size_t kEcdsaP256UncompressedPointBytes = 65;
constexpr size_t kNumPinnedHybridKeys = 3;

// A pinned post-quantum hybrid public key for
// COMPOSITE_SIGN_MLDSA_44_ECDSA_P256_TINK_RAW:
// - `mldsa44_public_key`: raw FIPS 204 ML-DSA-44 public key (1312 bytes).
// - `ecdsa_p256_public_key`: uncompressed X9.62 P-256 point (65 bytes:
//   0x04 || X(32) || Y(32)).
struct PinnedHybridPublicKey {
  std::array<uint8_t, MLDSA44_PUBLIC_KEY_BYTES> mldsa44_public_key = {};
  std::array<uint8_t, kEcdsaP256UncompressedPointBytes> ecdsa_p256_public_key =
      {};
};

// Pinned hybrid public keys (Key IDs 1, 2, 3) for Variations seed key
// rotation (crbug.com/588691630). Each entry contains a raw FIPS 204
// ML-DSA-44 public key (1312 bytes) and an uncompressed X9.62 ECDSA
// P-256 point (65 bytes).
constexpr std::array<PinnedHybridPublicKey, kNumPinnedHybridKeys>
    kPinnedHybridKeys = {{
        // Key ID 1
        {
            .mldsa44_public_key =
                {
                    0x16, 0xf0, 0x5c, 0x94, 0x4c, 0xe1, 0xdf, 0x1c, 0x66, 0x25,
                    0x32, 0x7c, 0x53, 0x41, 0xa8, 0x94, 0x66, 0x51, 0x53, 0xb3,
                    0x6e, 0x87, 0xc3, 0x15, 0xc5, 0xd6, 0xfb, 0x0c, 0x8f, 0xd4,
                    0xdb, 0xcb, 0xbc, 0x08, 0xc4, 0x65, 0x61, 0xbc, 0x80, 0x21,
                    0x2c, 0xa5, 0x4d, 0xd0, 0x85, 0x4f, 0x5f, 0x66, 0x79, 0x49,
                    0x20, 0x73, 0x92, 0x8d, 0x16, 0x2d, 0xfb, 0x22, 0xbb, 0xf1,
                    0xc7, 0xfc, 0x58, 0x59, 0x64, 0x22, 0x25, 0x5e, 0x3a, 0x23,
                    0x04, 0xcb, 0xc7, 0x63, 0xe4, 0x65, 0x53, 0xd0, 0x54, 0x60,
                    0xbc, 0x3a, 0xb9, 0x25, 0xbd, 0x75, 0x01, 0x12, 0x59, 0x1a,
                    0x1c, 0xf8, 0x05, 0x21, 0xb1, 0x3b, 0x62, 0x6f, 0xfb, 0xce,
                    0xe6, 0x8a, 0x91, 0xee, 0x38, 0xe0, 0x7c, 0xe9, 0xef, 0x72,
                    0xea, 0xab, 0x1b, 0x8a, 0xad, 0x80, 0x6a, 0x1c, 0xa0, 0x9a,
                    0x3a, 0xe4, 0x3a, 0x93, 0x93, 0xe7, 0xf1, 0x34, 0xea, 0xee,
                    0x4b, 0x76, 0xd2, 0xc1, 0x76, 0x27, 0x78, 0xeb, 0xd9, 0xc7,
                    0xda, 0x15, 0x2d, 0x80, 0xb6, 0x6d, 0x83, 0x48, 0x04, 0x0f,
                    0xfa, 0x41, 0x5f, 0x86, 0x66, 0x09, 0x20, 0x42, 0x9b, 0x7d,
                    0x70, 0x13, 0xc7, 0x21, 0x97, 0xbc, 0xf1, 0x9d, 0xf8, 0x2e,
                    0x14, 0x53, 0xbb, 0xc0, 0x07, 0xc1, 0x63, 0x64, 0x15, 0x16,
                    0x9a, 0x3b, 0xa2, 0x9b, 0x56, 0xb5, 0x1a, 0xba, 0xfe, 0xeb,
                    0x8a, 0xdd, 0x6f, 0x1e, 0x3d, 0x02, 0x64, 0x48, 0xa6, 0x97,
                    0xb1, 0xad, 0xe3, 0x72, 0xbb, 0x39, 0x72, 0xf0, 0x15, 0x7f,
                    0xb0, 0x0a, 0xd5, 0x83, 0xc8, 0x93, 0x71, 0x31, 0xd5, 0xec,
                    0xf9, 0x44, 0x89, 0xcd, 0x40, 0x35, 0xa1, 0x74, 0x3e, 0xaa,
                    0x5c, 0x20, 0x3c, 0xb5, 0xf1, 0xd6, 0xf3, 0x0c, 0xc3, 0xf4,
                    0xff, 0x2e, 0x21, 0xbb, 0x9b, 0x18, 0x4f, 0x06, 0xfb, 0xdf,
                    0x85, 0xd1, 0x41, 0x57, 0x29, 0xfc, 0xbb, 0x25, 0xde, 0xbc,
                    0xf5, 0x83, 0x05, 0xb2, 0xe9, 0xaa, 0xdf, 0x18, 0xa6, 0x90,
                    0xc5, 0x91, 0xdb, 0x23, 0xb1, 0x03, 0x1c, 0x89, 0x20, 0x11,
                    0x39, 0x2f, 0x86, 0x65, 0xf3, 0xd5, 0xf0, 0xc3, 0x68, 0xf4,
                    0x69, 0xf2, 0xc2, 0x29, 0xb4, 0x98, 0xa8, 0xb1, 0x34, 0xc7,
                    0x2f, 0x83, 0x53, 0x92, 0xdc, 0xe5, 0xf5, 0xbc, 0xc3, 0x14,
                    0x0d, 0xf0, 0xe1, 0xa7, 0x19, 0xad, 0x4e, 0xfa, 0xf8, 0x21,
                    0xa9, 0x58, 0x7c, 0xd3, 0x25, 0xc2, 0xe0, 0xe8, 0x9f, 0xbf,
                    0xe1, 0x5e, 0x9b, 0xa7, 0xe3, 0x1a, 0x2d, 0x3c, 0x69, 0x17,
                    0xa2, 0xa8, 0x26, 0xf0, 0xa3, 0x91, 0x45, 0x75, 0xb9, 0xac,
                    0xc7, 0x1a, 0x45, 0x10, 0xcd, 0x30, 0x19, 0x40, 0x61, 0xae,
                    0x04, 0x07, 0xae, 0xb5, 0xc5, 0x80, 0x18, 0x27, 0x9f, 0xa3,
                    0xcf, 0x06, 0x4f, 0xc3, 0xfb, 0xac, 0xe3, 0x46, 0xc7, 0x21,
                    0xbf, 0x27, 0x86, 0xb6, 0x4d, 0xf5, 0x3b, 0x2f, 0x7c, 0xec,
                    0x52, 0xab, 0x1c, 0x19, 0xf7, 0x3b, 0x43, 0x5f, 0xad, 0xfe,
                    0x83, 0x98, 0xba, 0xbc, 0xff, 0x83, 0x65, 0x31, 0x81, 0xbd,
                    0x59, 0xe7, 0x72, 0xcc, 0x39, 0x5e, 0x7e, 0xc9, 0x25, 0x5f,
                    0x34, 0x22, 0xee, 0x3c, 0xec, 0xc5, 0x88, 0x42, 0x62, 0x29,
                    0x53, 0x40, 0x2e, 0x4b, 0x7e, 0xc3, 0x51, 0x0c, 0x4d, 0xcc,
                    0xd6, 0xc3, 0xe4, 0x12, 0x36, 0xc0, 0x6c, 0x14, 0xd8, 0x8b,
                    0x66, 0x6e, 0xf6, 0x3e, 0x81, 0x3c, 0x87, 0x62, 0xab, 0x29,
                    0x87, 0xd4, 0xe4, 0x30, 0x01, 0x7e, 0xc0, 0xa4, 0x7a, 0x2c,
                    0xe9, 0xee, 0x0a, 0xb7, 0x54, 0x56, 0x39, 0xae, 0x7c, 0x02,
                    0x3b, 0xca, 0xca, 0x29, 0xb7, 0x68, 0xb1, 0x2a, 0xd7, 0xf4,
                    0x77, 0xd4, 0xf4, 0x76, 0xcc, 0x56, 0xad, 0x00, 0x6b, 0x15,
                    0xbd, 0xaf, 0xdf, 0x1f, 0x14, 0x4b, 0xa0, 0xde, 0xb9, 0x57,
                    0x7d, 0xb2, 0x16, 0xf9, 0x14, 0x22, 0x9e, 0xb9, 0xd1, 0xfb,
                    0xe9, 0xb3, 0x24, 0x4d, 0x55, 0x2e, 0x23, 0x32, 0x95, 0x49,
                    0xed, 0xb7, 0x49, 0x92, 0x7e, 0xbd, 0x5c, 0xf5, 0xb9, 0xf2,
                    0x2c, 0x5d, 0x88, 0xa3, 0xab, 0x5a, 0x1e, 0xc8, 0x89, 0x22,
                    0x1b, 0x55, 0x61, 0x1a, 0xff, 0xfb, 0xf6, 0xe8, 0xbe, 0x42,
                    0xa4, 0x9e, 0x7a, 0x83, 0x98, 0x1a, 0x59, 0x1a, 0x98, 0xfe,
                    0x22, 0x16, 0x40, 0xed, 0xe2, 0xee, 0x5b, 0xae, 0xc6, 0x3d,
                    0x88, 0xd4, 0xdc, 0xc7, 0x1a, 0xf6, 0x0d, 0x42, 0x94, 0x95,
                    0xef, 0x33, 0x5e, 0xf8, 0x49, 0x43, 0x95, 0x86, 0xc8, 0xc2,
                    0xf8, 0x2a, 0x6b, 0x3f, 0xbc, 0xc6, 0x8d, 0xae, 0x24, 0x72,
                    0x36, 0xe0, 0xa6, 0x61, 0x10, 0x49, 0xe2, 0x01, 0xc9, 0x20,
                    0xd4, 0x8f, 0xe1, 0x42, 0x69, 0x8d, 0x1e, 0xcb, 0x76, 0x34,
                    0xc9, 0xfe, 0xc7, 0x6d, 0xe2, 0x8a, 0x94, 0xae, 0xe6, 0xdd,
                    0x84, 0xf4, 0x97, 0x46, 0x10, 0x2a, 0x7d, 0x4d, 0x0e, 0xcc,
                    0x2f, 0x2c, 0x34, 0xf6, 0xbb, 0xc4, 0x73, 0x5b, 0xf8, 0xf3,
                    0xa0, 0xea, 0x57, 0x34, 0x48, 0x96, 0x19, 0xe4, 0x14, 0xe5,
                    0xa0, 0x08, 0xfc, 0xa1, 0x0f, 0xf1, 0x24, 0x32, 0x73, 0xbf,
                    0x76, 0x75, 0x77, 0x1b, 0x67, 0x0a, 0x6e, 0x72, 0xe5, 0xcf,
                    0x27, 0x73, 0x43, 0x7b, 0xd3, 0x18, 0x3a, 0x47, 0x0d, 0x06,
                    0x1a, 0xba, 0x0d, 0x8b, 0x1a, 0xd2, 0x3e, 0x75, 0x9b, 0x86,
                    0x59, 0x7c, 0xc3, 0xb7, 0xff, 0x60, 0xa3, 0x03, 0x71, 0x94,
                    0x49, 0x64, 0xc3, 0x8a, 0xb1, 0xeb, 0x41, 0x8c, 0xd2, 0x12,
                    0x35, 0x68, 0x36, 0xcd, 0xa0, 0xf8, 0x3e, 0x03, 0x14, 0xe8,
                    0xc5, 0x27, 0x3e, 0xb4, 0xf7, 0x36, 0x36, 0x4d, 0x2f, 0xb8,
                    0xa3, 0x64, 0x30, 0x09, 0x20, 0xe7, 0xf4, 0x9f, 0x78, 0xa7,
                    0x27, 0x10, 0xd1, 0x01, 0xd4, 0x04, 0x80, 0xc5, 0x5f, 0x2d,
                    0xd1, 0x65, 0x38, 0x77, 0x98, 0x89, 0xc7, 0x4e, 0x4a, 0x6e,
                    0x3d, 0xdd, 0xaa, 0x9f, 0x9e, 0x87, 0xf8, 0xe1, 0x77, 0x6f,
                    0xb9, 0x6b, 0xf9, 0x8e, 0x6f, 0x4b, 0xe8, 0x5c, 0x6c, 0xc2,
                    0x77, 0xc2, 0xb0, 0x8c, 0xf1, 0x44, 0xe6, 0xd0, 0x6e, 0xbd,
                    0xdf, 0x85, 0x1e, 0x81, 0x17, 0x3c, 0xaf, 0x6a, 0x35, 0xf1,
                    0xbc, 0xaa, 0x0e, 0xf2, 0x26, 0x78, 0x10, 0xee, 0x96, 0x43,
                    0x54, 0x22, 0x94, 0x12, 0x1a, 0xf4, 0xd4, 0x2b, 0xa8, 0x7e,
                    0x36, 0x65, 0xd8, 0xe5, 0x64, 0xb6, 0xe3, 0xf7, 0x0c, 0x0a,
                    0xd1, 0x85, 0xb1, 0x82, 0x68, 0x9d, 0x8f, 0xf7, 0xa2, 0xa1,
                    0x0e, 0xbf, 0xa5, 0xa0, 0x34, 0xe5, 0x1a, 0x92, 0xb3, 0xb4,
                    0x38, 0x15, 0xf9, 0x47, 0x8d, 0xa7, 0xab, 0xa5, 0xef, 0x73,
                    0xf9, 0x38, 0xb6, 0xdf, 0x23, 0xdd, 0x23, 0x0f, 0xf7, 0xd3,
                    0x7d, 0x11, 0xc1, 0x48, 0x9c, 0x21, 0x5f, 0x23, 0xe2, 0x95,
                    0x1e, 0xe5, 0x23, 0x44, 0x55, 0x6b, 0xa9, 0x0c, 0xf7, 0x07,
                    0xfb, 0x5b, 0xf0, 0xfa, 0xd1, 0x31, 0x60, 0xec, 0x32, 0xc3,
                    0xf1, 0xf2, 0x5d, 0x72, 0x46, 0xc1, 0x5d, 0x31, 0x02, 0x75,
                    0xe3, 0x62, 0x4b, 0x99, 0x4a, 0x1c, 0xfe, 0x12, 0xbb, 0x36,
                    0x91, 0xc4, 0x40, 0xe8, 0x87, 0xe1, 0x53, 0x9f, 0x63, 0xd3,
                    0x74, 0x2c, 0x0a, 0x33, 0xe8, 0xca, 0x1b, 0x12, 0x83, 0x24,
                    0x03, 0x97, 0xbd, 0x49, 0x23, 0xa3, 0x21, 0x23, 0xe8, 0x78,
                    0x0d, 0x1f, 0xe3, 0x56, 0xc7, 0x24, 0x27, 0x24, 0x61, 0x69,
                    0x8c, 0xa0, 0xe1, 0x83, 0x05, 0x07, 0xe7, 0x5e, 0x30, 0x7d,
                    0x8c, 0xe8, 0xbb, 0xfc, 0xb7, 0xa7, 0x8f, 0x72, 0x82, 0x7e,
                    0x11, 0x8b, 0xc6, 0x9c, 0x13, 0xf1, 0xf7, 0x28, 0xa0, 0x21,
                    0x66, 0xf0, 0x2c, 0x92, 0x36, 0xe7, 0x7c, 0x47, 0xf3, 0xa8,
                    0xe2, 0x1e, 0x37, 0x27, 0xec, 0xd9, 0xc6, 0x3b, 0x41, 0xb9,
                    0xef, 0x8c, 0x2f, 0x20, 0x65, 0xb3, 0xae, 0x59, 0xec, 0x9b,
                    0x79, 0xd6, 0x68, 0x75, 0x9b, 0xed, 0xc3, 0x49, 0x88, 0x5c,
                    0xa2, 0xe8, 0xaf, 0x29, 0x82, 0x2f, 0xd4, 0x07, 0x3c, 0x43,
                    0xe8, 0xbf, 0xe6, 0xd1, 0xa6, 0xa0, 0x90, 0xca, 0x33, 0xce,
                    0x30, 0x18, 0xa5, 0x23, 0x9b, 0xb4, 0x31, 0x58, 0x2f, 0xda,
                    0x98, 0xed, 0x31, 0x2c, 0x2c, 0xd7, 0x2a, 0xfa, 0x4f, 0x26,
                    0x04, 0x5e, 0xff, 0xa3, 0x69, 0x0f, 0xfe, 0x25, 0xc5, 0xa7,
                    0xaf, 0xd6, 0x4e, 0xd4, 0xc4, 0x1d, 0xde, 0xbc, 0xb7, 0x04,
                    0x05, 0x90, 0x41, 0xe8, 0x15, 0x10, 0xab, 0x5a, 0x34, 0xed,
                    0x1a, 0xfe, 0xb5, 0xed, 0xc0, 0xc2, 0xe4, 0x0c, 0x7b, 0xea,
                    0x52, 0x6c, 0x8c, 0x00, 0x6b, 0x96, 0xb4, 0xfc, 0x9a, 0x5f,
                    0x63, 0x2a, 0xba, 0x81, 0x67, 0x44, 0x4c, 0xf3, 0x71, 0x4e,
                    0x26, 0xc9, 0x75, 0x26, 0xe2, 0xcf, 0x1f, 0x08, 0x86, 0x05,
                    0xcf, 0xfc, 0x6e, 0x21, 0xb3, 0xfd, 0x6b, 0x93, 0x20, 0xda,
                    0x28, 0x92, 0x02, 0x39, 0xc9, 0x23, 0x12, 0x6d, 0x21, 0xf2,
                    0xc9, 0x4f, 0x50, 0x70, 0x21, 0xeb, 0x29, 0xac, 0x10, 0xf4,
                    0x8f, 0xf0, 0x3f, 0x2a, 0x86, 0x8d, 0x1c, 0x60, 0x5c, 0xd8,
                    0xc2, 0xe4, 0xd3, 0xdf, 0xad, 0xa1, 0x39, 0x0b, 0x41, 0x61,
                    0xc1, 0x1f, 0xbd, 0xc7, 0xeb, 0x57, 0x8c, 0x86, 0xe6, 0x19,
                    0x8b, 0xca, 0x43, 0x31, 0x32, 0x82, 0xe6, 0x40, 0xea, 0x6b,
                    0x41, 0x7a, 0x1c, 0xa2, 0x4f, 0x35, 0x2e, 0x0e, 0xac, 0xfe,
                    0x6f, 0xc4, 0xea, 0x6b, 0xd4, 0xdc, 0x0c, 0x01, 0x0c, 0xf3,
                    0xf6, 0xad, 0xbe, 0x8f, 0x6c, 0xb6, 0x1b, 0xfc, 0xb6, 0x57,
                    0xa8, 0x42, 0x30, 0x76, 0xee, 0x6f, 0x3b, 0x86, 0xf9, 0xde,
                    0xc9, 0x2c, 0xff, 0x92, 0x77, 0x7c, 0x42, 0xdd, 0xbb, 0xae,
                    0xe0, 0x04, 0xca, 0xd8, 0xf9, 0x96, 0x3e, 0xe8, 0x6b, 0x32,
                    0x49, 0xc8, 0x0d, 0xd2, 0x89, 0xae, 0xc4, 0xac, 0x9a, 0x43,
                    0x82, 0xeb, 0x18, 0xec, 0xa9, 0x8e, 0x1b, 0xdc, 0x08, 0x3f,
                    0x35, 0x7a,
                },
            .ecdsa_p256_public_key =
                {
                    0x04, 0xf0, 0xb3, 0xa5, 0x28, 0xef, 0xe7, 0xd3, 0x69, 0x60,
                    0x29, 0x54, 0xd3, 0xc3, 0xe9, 0x0c, 0x62, 0xfd, 0x44, 0xe8,
                    0x2b, 0x44, 0xf6, 0x41, 0x5d, 0x1c, 0xbf, 0xa3, 0x22, 0xa0,
                    0xca, 0xce, 0x5e, 0xa5, 0x8c, 0x10, 0x09, 0x79, 0xd9, 0xd4,
                    0x9d, 0x1f, 0xd4, 0xc0, 0x24, 0x0d, 0x80, 0x49, 0x60, 0xbc,
                    0x28, 0x46, 0xda, 0xc8, 0xa2, 0xcb, 0xe0, 0x73, 0xdb, 0xa6,
                    0x39, 0xb3, 0x84, 0x1e, 0x12,
                },
        },
        // Key ID 2
        {
            .mldsa44_public_key =
                {
                    0x69, 0x5c, 0x39, 0xb8, 0x1b, 0x8d, 0xe9, 0xfc, 0x7a, 0xc5,
                    0x90, 0x29, 0x67, 0xce, 0x0c, 0x6f, 0x9b, 0xd7, 0x45, 0xf7,
                    0x5b, 0x8d, 0x54, 0xcf, 0x52, 0x17, 0xe3, 0x70, 0x26, 0x86,
                    0x3f, 0x6c, 0xc9, 0x11, 0x48, 0x50, 0x1e, 0xca, 0xcc, 0x34,
                    0xac, 0x7e, 0x40, 0x47, 0x00, 0x4a, 0x54, 0x9f, 0x7d, 0x0e,
                    0x0f, 0xa5, 0xc2, 0x3d, 0x22, 0x17, 0xc6, 0x49, 0xc1, 0xcb,
                    0xa7, 0xa1, 0xe6, 0xac, 0xfa, 0x9d, 0x39, 0x77, 0x85, 0x13,
                    0xf4, 0x57, 0x03, 0x0a, 0x4a, 0x70, 0x9c, 0x6a, 0xa7, 0x5a,
                    0x67, 0xf8, 0xe3, 0x8d, 0xdc, 0xcd, 0x53, 0x5f, 0xf1, 0x99,
                    0x89, 0x75, 0x5e, 0xc4, 0x0f, 0xc3, 0x3f, 0x58, 0xe0, 0x4d,
                    0xd6, 0x50, 0x58, 0x9b, 0xdd, 0x9d, 0xbc, 0xf8, 0x30, 0x30,
                    0x06, 0xfe, 0xaa, 0xbd, 0xc7, 0x99, 0xd4, 0x0e, 0xc6, 0x03,
                    0x4d, 0xd8, 0xff, 0x9e, 0x17, 0xa9, 0x47, 0x85, 0x04, 0xb4,
                    0x8c, 0xe8, 0x89, 0xcc, 0x9a, 0xc0, 0x1b, 0x94, 0xe1, 0x40,
                    0x6f, 0x89, 0x9c, 0x12, 0xa1, 0xa2, 0xf9, 0x61, 0xfb, 0xf5,
                    0x17, 0x98, 0x50, 0x37, 0x81, 0x91, 0x5f, 0x90, 0x5b, 0xaa,
                    0xc3, 0x26, 0x5b, 0x6c, 0xea, 0x79, 0xef, 0xca, 0x55, 0x60,
                    0x58, 0xe3, 0x18, 0xff, 0x19, 0x35, 0x59, 0xe2, 0xbf, 0xd0,
                    0x46, 0x74, 0x0b, 0x20, 0x2e, 0xa8, 0x24, 0x12, 0x4e, 0x7e,
                    0x13, 0x93, 0x3e, 0xd6, 0x81, 0xc9, 0x38, 0x91, 0x92, 0xaa,
                    0x82, 0x07, 0xe9, 0xa7, 0x01, 0xf7, 0x91, 0x43, 0xf0, 0xb5,
                    0xbc, 0x74, 0x72, 0x9a, 0x18, 0x9f, 0xf2, 0x28, 0x76, 0x5a,
                    0x9c, 0xc3, 0x16, 0x29, 0xdf, 0x0c, 0x22, 0x56, 0x53, 0x08,
                    0x6b, 0x40, 0x03, 0x8d, 0xcb, 0xbb, 0x20, 0xc8, 0xf0, 0x31,
                    0xf0, 0x68, 0x82, 0x41, 0x6e, 0x55, 0x97, 0x48, 0x59, 0x96,
                    0xdb, 0x90, 0x3e, 0x27, 0x68, 0xbc, 0xcd, 0x5a, 0xc3, 0x9e,
                    0x99, 0xd8, 0x05, 0xae, 0x9d, 0x27, 0xdc, 0xc7, 0xd2, 0xc0,
                    0xd6, 0xb8, 0x29, 0x9a, 0x3f, 0x09, 0xe9, 0x48, 0x1d, 0xa9,
                    0xeb, 0x1d, 0xec, 0x7e, 0xb0, 0x0c, 0x66, 0xf9, 0xca, 0xe5,
                    0xd6, 0xd5, 0xf5, 0x61, 0xf2, 0xaf, 0xef, 0xac, 0xbb, 0x33,
                    0xf4, 0x5f, 0x02, 0x98, 0x54, 0x7c, 0xdc, 0x9b, 0xb2, 0xc9,
                    0x07, 0x12, 0x63, 0xdb, 0x15, 0xb2, 0xf3, 0x8b, 0x9f, 0xb1,
                    0xa4, 0xd5, 0x20, 0xa7, 0x05, 0x2f, 0xcf, 0x16, 0x85, 0xac,
                    0xaa, 0x2d, 0xea, 0x1d, 0x31, 0x92, 0xbe, 0xe4, 0xc3, 0x9a,
                    0x25, 0x4b, 0x52, 0x36, 0x47, 0x06, 0x5c, 0x8c, 0xed, 0x0e,
                    0x12, 0xde, 0x49, 0x77, 0xb0, 0x79, 0x1a, 0xbb, 0x47, 0x50,
                    0xe5, 0x50, 0x88, 0x60, 0xf0, 0x0f, 0xec, 0x00, 0x8b, 0x15,
                    0x2b, 0x55, 0xf1, 0xd3, 0x76, 0x58, 0x1a, 0x44, 0x1f, 0x1c,
                    0x14, 0x5e, 0xa6, 0x61, 0x71, 0xc1, 0x2b, 0x17, 0x8f, 0x90,
                    0xf5, 0x58, 0x16, 0x81, 0x96, 0xea, 0x98, 0x36, 0x43, 0x72,
                    0xcc, 0x76, 0xcb, 0xf4, 0x69, 0xa3, 0x1e, 0x4b, 0xbd, 0x96,
                    0x29, 0x84, 0xc0, 0x2d, 0x90, 0xe9, 0xc9, 0x25, 0x82, 0x3d,
                    0x38, 0xa6, 0x0a, 0xa1, 0x4e, 0xae, 0xef, 0x97, 0xed, 0xc4,
                    0x52, 0xfa, 0x3f, 0x2a, 0xa2, 0xce, 0xc1, 0xa3, 0xe8, 0x0b,
                    0x66, 0x37, 0x6b, 0x26, 0x15, 0x59, 0x33, 0x37, 0x8a, 0xff,
                    0x9a, 0x1a, 0x59, 0x26, 0x48, 0xdb, 0xb7, 0x70, 0xb1, 0x2d,
                    0x82, 0x27, 0x09, 0x11, 0xe2, 0x6e, 0xca, 0x34, 0xea, 0x51,
                    0xf8, 0x92, 0x50, 0xea, 0x99, 0x37, 0xc3, 0x9a, 0x49, 0x8b,
                    0x38, 0x2e, 0x5c, 0x18, 0x56, 0x7a, 0xff, 0x92, 0x01, 0x6d,
                    0x90, 0x8f, 0xee, 0x39, 0xd4, 0xb6, 0x65, 0x28, 0x11, 0x53,
                    0x13, 0x72, 0xe1, 0xc1, 0x12, 0x39, 0x6d, 0xc8, 0x55, 0xa9,
                    0x26, 0x69, 0xda, 0xd4, 0x84, 0x5d, 0xb0, 0xa2, 0x7f, 0x07,
                    0xe9, 0x2c, 0xeb, 0x74, 0x16, 0x5a, 0x03, 0xf1, 0x61, 0xf2,
                    0xfb, 0xc8, 0xd2, 0xcc, 0xc4, 0x0b, 0x36, 0xdf, 0xa1, 0x1e,
                    0x53, 0x5a, 0xb0, 0x59, 0x70, 0x4d, 0x2e, 0x2c, 0xbc, 0x99,
                    0x73, 0xe3, 0x2e, 0xe1, 0xd0, 0x99, 0x06, 0x8a, 0x7a, 0x29,
                    0x0a, 0xeb, 0xae, 0x29, 0x45, 0x68, 0x98, 0xbc, 0xba, 0x70,
                    0xe7, 0x7f, 0x4b, 0xdf, 0x48, 0x58, 0xc6, 0x4b, 0x9a, 0x45,
                    0x12, 0x30, 0x05, 0xd7, 0x8d, 0xa0, 0xdf, 0x56, 0x73, 0xf9,
                    0x93, 0x23, 0xdd, 0x01, 0xdb, 0xcf, 0x2d, 0x9e, 0xa4, 0xbf,
                    0x00, 0x3b, 0xab, 0x1f, 0x8d, 0xfb, 0xfb, 0x2d, 0xa7, 0x14,
                    0x5c, 0xd1, 0xec, 0xc3, 0x71, 0x51, 0x71, 0xf1, 0x6e, 0x87,
                    0xf8, 0xc5, 0x27, 0xbd, 0x13, 0xb9, 0xd4, 0x9f, 0x79, 0x6a,
                    0x9e, 0xc8, 0x02, 0xfc, 0xfa, 0x2a, 0x4d, 0x9a, 0x5c, 0x3d,
                    0x29, 0x5d, 0xe8, 0x09, 0x51, 0x6c, 0xe5, 0xdf, 0xb4, 0x01,
                    0x18, 0xca, 0x86, 0x74, 0x90, 0x05, 0xe2, 0xd5, 0x63, 0xb3,
                    0xac, 0x44, 0xf1, 0x00, 0x5b, 0x4e, 0xba, 0xaf, 0x46, 0xfd,
                    0x25, 0x02, 0x2b, 0xe1, 0x73, 0xc8, 0xdb, 0x2e, 0x2c, 0xd0,
                    0x11, 0x92, 0xd6, 0x4e, 0x8f, 0x12, 0xca, 0x2e, 0xe3, 0x29,
                    0x0d, 0x63, 0xf0, 0x30, 0x31, 0x91, 0x79, 0x1f, 0xe9, 0xa7,
                    0x1f, 0x60, 0x6f, 0xa5, 0x13, 0x06, 0xd9, 0x72, 0x60, 0x2b,
                    0x2b, 0xb1, 0x65, 0x3e, 0x31, 0xe9, 0x18, 0x70, 0xa7, 0xb9,
                    0x01, 0x80, 0xec, 0xfc, 0xaa, 0xfb, 0x92, 0x93, 0x00, 0x7a,
                    0x01, 0xf7, 0x12, 0xa8, 0xd4, 0x67, 0x2b, 0x4c, 0xc7, 0x63,
                    0x51, 0xbb, 0x65, 0xa4, 0x1d, 0xfd, 0x8c, 0x75, 0xde, 0x33,
                    0x39, 0x26, 0xa0, 0x5d, 0x0e, 0xa1, 0xd4, 0x07, 0x78, 0x17,
                    0xb2, 0x32, 0x9e, 0x42, 0x16, 0xbf, 0x4d, 0x63, 0xc3, 0xb8,
                    0x4b, 0x5a, 0x3c, 0x7b, 0x08, 0x06, 0x61, 0x72, 0x4d, 0xad,
                    0x80, 0x2e, 0x68, 0x3a, 0x10, 0x88, 0x24, 0x59, 0xe1, 0x1f,
                    0x4a, 0xcf, 0x8e, 0xed, 0x87, 0x50, 0x6c, 0x76, 0x80, 0xfc,
                    0x9b, 0xb2, 0x1c, 0x61, 0x09, 0xcc, 0x49, 0xee, 0x90, 0xcd,
                    0xab, 0xba, 0x29, 0xf6, 0xcf, 0x35, 0x66, 0x77, 0x9a, 0x25,
                    0xa0, 0x03, 0x3d, 0x3c, 0x19, 0x20, 0x8b, 0x22, 0xa5, 0x26,
                    0x32, 0xa3, 0x55, 0x4d, 0xbe, 0x39, 0xfe, 0xb9, 0xc2, 0x5f,
                    0x36, 0x41, 0x72, 0xe2, 0x9e, 0xf7, 0xaa, 0x70, 0xa3, 0xa2,
                    0x8b, 0xef, 0x19, 0xf5, 0x3e, 0x8a, 0xb2, 0x53, 0x84, 0x3d,
                    0x82, 0xad, 0x23, 0x9d, 0x99, 0x87, 0x47, 0xba, 0xb4, 0x10,
                    0x7a, 0x13, 0x5d, 0xb9, 0xfc, 0x29, 0xf1, 0x7b, 0x5b, 0xa4,
                    0x04, 0x56, 0x63, 0x90, 0xa6, 0xa5, 0xfe, 0x93, 0xb0, 0x93,
                    0xf8, 0x47, 0xc9, 0x56, 0x5e, 0x18, 0xf3, 0x72, 0xfa, 0x92,
                    0xb8, 0x4a, 0xd2, 0xe0, 0x1f, 0x76, 0x37, 0x3c, 0x1e, 0xa7,
                    0x3a, 0x16, 0x82, 0xf3, 0xbf, 0x69, 0x6a, 0x0b, 0x0e, 0xe3,
                    0x92, 0xe0, 0x9f, 0x15, 0x45, 0x4b, 0x93, 0xb1, 0x14, 0x1d,
                    0x82, 0xdb, 0xff, 0xdd, 0xed, 0x6f, 0x64, 0xe1, 0xa4, 0x76,
                    0x23, 0x99, 0x54, 0xc0, 0x58, 0xe9, 0x6c, 0x2e, 0x71, 0x68,
                    0x7d, 0x81, 0x5b, 0x9c, 0xb2, 0x6e, 0xf2, 0xd6, 0x56, 0x24,
                    0xfb, 0x1a, 0xba, 0x73, 0x61, 0x28, 0x21, 0xd0, 0x05, 0x60,
                    0x0c, 0x98, 0x53, 0x87, 0x8c, 0x4c, 0x6e, 0x5f, 0x0e, 0xe3,
                    0x64, 0x07, 0x7a, 0x0b, 0x27, 0x09, 0xa6, 0xba, 0x31, 0xd0,
                    0x20, 0xa4, 0x25, 0xe2, 0x89, 0x47, 0xc5, 0xfe, 0x6b, 0xae,
                    0x4e, 0x65, 0x8f, 0xae, 0xa0, 0x0a, 0xef, 0x61, 0x66, 0xd7,
                    0x01, 0xf1, 0x83, 0xbf, 0x96, 0x75, 0xb3, 0x2a, 0x4a, 0xd2,
                    0x88, 0xb9, 0xb7, 0x66, 0x93, 0x7c, 0xe5, 0xf4, 0x87, 0x7e,
                    0x4a, 0xdc, 0x1b, 0x2d, 0x7a, 0x45, 0x09, 0x46, 0x1d, 0xef,
                    0x77, 0x33, 0x5b, 0x0e, 0x86, 0x9e, 0x5e, 0x3c, 0x5f, 0x20,
                    0x5b, 0x79, 0x62, 0xa7, 0x79, 0x51, 0xb0, 0x41, 0xe5, 0xd5,
                    0x6a, 0x5c, 0xf5, 0xfb, 0x5c, 0xe1, 0x77, 0x8f, 0xcb, 0x1e,
                    0x96, 0x72, 0xfc, 0xdc, 0xf9, 0xbc, 0x7f, 0xda, 0xd8, 0x4f,
                    0xb4, 0x20, 0x9b, 0x77, 0x3e, 0x9d, 0x06, 0x4a, 0x01, 0x5a,
                    0x93, 0x02, 0xb5, 0xcd, 0x21, 0x25, 0xb5, 0xc6, 0xc5, 0xe7,
                    0x0c, 0x88, 0xa7, 0x37, 0x25, 0x3e, 0x92, 0x5d, 0xa0, 0xb6,
                    0xbf, 0xf4, 0xff, 0x73, 0xd0, 0x93, 0x8f, 0x0a, 0x47, 0xc2,
                    0x22, 0x26, 0xea, 0x5c, 0xb7, 0x68, 0x02, 0xc2, 0x9a, 0x89,
                    0x7e, 0x86, 0xb0, 0xb8, 0x20, 0x81, 0xaa, 0x99, 0x7b, 0xb6,
                    0x1c, 0xeb, 0x48, 0xe0, 0x59, 0x19, 0x30, 0x7e, 0x3a, 0xb8,
                    0x90, 0xa5, 0x2d, 0xeb, 0x3b, 0xef, 0x10, 0x2c, 0x79, 0xa9,
                    0xa9, 0x69, 0x68, 0x55, 0x81, 0x43, 0x16, 0x5e, 0x0b, 0xb4,
                    0x04, 0xfb, 0x7a, 0x42, 0x78, 0xba, 0x5e, 0x92, 0xf9, 0xc1,
                    0x88, 0xb6, 0x7e, 0xe5, 0x2c, 0xe0, 0x13, 0x19, 0x25, 0x72,
                    0x80, 0x34, 0x5b, 0x43, 0xc1, 0xd3, 0x89, 0xa9, 0xad, 0xe6,
                    0x18, 0xa7, 0x04, 0x82, 0x7e, 0x11, 0xa8, 0xca, 0x6e, 0xb0,
                    0xaf, 0x92, 0x6b, 0x8b, 0x47, 0x9b, 0xb5, 0xdb, 0x77, 0xed,
                    0xde, 0xb3, 0xdc, 0xa4, 0x1f, 0x6d, 0xff, 0x54, 0x49, 0x4d,
                    0x49, 0x11, 0x13, 0x17, 0x11, 0x52, 0x90, 0x3f, 0x40, 0xd5,
                    0x7f, 0xb1, 0x3a, 0x5a, 0xb5, 0xb1, 0xa9, 0x1a, 0x1e, 0x8b,
                    0xff, 0x62, 0x30, 0x42, 0x84, 0x51, 0x86, 0x34, 0x73, 0x13,
                    0xa6, 0xb8, 0xe5, 0x61, 0x82, 0x7e, 0xaf, 0xe3, 0x67, 0xf1,
                    0x39, 0x70, 0x65, 0x47, 0xf8, 0xb1, 0xb7, 0xa2, 0xa4, 0x1f,
                    0x76, 0x55, 0xe9, 0x18, 0xa6, 0x43, 0xdc, 0x18, 0xd3, 0x57,
                    0xc1, 0xa1, 0x96, 0x8d, 0x09, 0xab, 0x04, 0xa4, 0x44, 0x5a,
                    0xfd, 0x56, 0x42, 0xcf, 0x59, 0xb2, 0x92, 0x5a, 0x2f, 0x43,
                    0x8c, 0x44,
                },
            .ecdsa_p256_public_key =
                {
                    0x04, 0x5f, 0x4f, 0x31, 0xd7, 0xa4, 0xed, 0xe4, 0xd6, 0x73,
                    0x8f, 0xe6, 0x1d, 0x0c, 0x53, 0x35, 0xc2, 0x96, 0x75, 0x8c,
                    0x7c, 0x0e, 0x0a, 0xe7, 0x1d, 0xc4, 0x22, 0xc4, 0xed, 0xa6,
                    0xac, 0x52, 0xb0, 0x86, 0x80, 0x27, 0x19, 0x7c, 0x27, 0x5c,
                    0x93, 0x45, 0xea, 0x5d, 0x93, 0x61, 0x08, 0x77, 0xc9, 0x06,
                    0x10, 0xfa, 0x63, 0x04, 0xcd, 0x2a, 0x22, 0x94, 0x0f, 0x87,
                    0xc4, 0xe9, 0x95, 0xe1, 0xb5,
                },
        },
        // Key ID 3
        {
            .mldsa44_public_key =
                {
                    0x47, 0x5c, 0x28, 0xf5, 0xbf, 0xc6, 0x18, 0x21, 0x12, 0xa6,
                    0x82, 0xda, 0x85, 0x75, 0x56, 0x38, 0x70, 0x56, 0x87, 0xce,
                    0xda, 0x82, 0x74, 0x49, 0x10, 0x9e, 0xb4, 0xf4, 0xab, 0xd4,
                    0xe0, 0x71, 0x8c, 0x12, 0x3d, 0xbc, 0x47, 0x19, 0x47, 0xca,
                    0xf3, 0x28, 0x48, 0x4e, 0xac, 0xf2, 0xa1, 0x01, 0x65, 0x6c,
                    0xfd, 0xb4, 0xec, 0xfa, 0x1e, 0x0c, 0x21, 0xba, 0x41, 0x11,
                    0xd8, 0xd2, 0xf4, 0x88, 0x6a, 0x13, 0xd6, 0xb7, 0x8c, 0x10,
                    0xad, 0x4f, 0xad, 0xf9, 0x22, 0x3f, 0x5c, 0x11, 0xb5, 0xa0,
                    0x96, 0x6a, 0xf7, 0x4a, 0x37, 0xd5, 0x9d, 0x51, 0x4e, 0xd7,
                    0x33, 0x3a, 0x16, 0xad, 0x64, 0x38, 0xe6, 0x9c, 0x52, 0xec,
                    0xa8, 0xfa, 0xd9, 0x0a, 0x5a, 0xe1, 0x04, 0x31, 0xfd, 0x46,
                    0xaa, 0x4e, 0x43, 0x6a, 0x7d, 0x9f, 0xcd, 0x43, 0xd7, 0xe0,
                    0xfb, 0xf6, 0x6c, 0x36, 0x19, 0x7f, 0xa3, 0x08, 0x4f, 0x43,
                    0x30, 0xec, 0x15, 0x23, 0x54, 0xa2, 0xb1, 0x4f, 0xb9, 0x98,
                    0xbd, 0x18, 0xe3, 0x00, 0xb2, 0xae, 0x13, 0x1e, 0xa6, 0xbf,
                    0x48, 0xc5, 0x77, 0x03, 0xfb, 0x12, 0xff, 0x83, 0x81, 0xda,
                    0x4a, 0x20, 0x0c, 0x30, 0x36, 0xd9, 0x91, 0xbb, 0xc3, 0xde,
                    0x5d, 0xa8, 0x2f, 0x9f, 0x33, 0x83, 0x20, 0x07, 0xe6, 0x59,
                    0x42, 0xd7, 0x7c, 0x68, 0xfd, 0xa6, 0x51, 0x20, 0x3b, 0xd8,
                    0xe9, 0x05, 0x39, 0x50, 0x15, 0x78, 0x39, 0xfc, 0xec, 0x27,
                    0xf4, 0x56, 0x21, 0xf8, 0xdc, 0xee, 0x65, 0x0f, 0x69, 0xec,
                    0xb9, 0xa6, 0xa9, 0x7c, 0x61, 0x03, 0xa0, 0x40, 0xd4, 0x40,
                    0xdf, 0xad, 0x24, 0x69, 0x4a, 0xb7, 0x0c, 0xaf, 0xbf, 0x7f,
                    0x4a, 0xf6, 0x5e, 0x2c, 0xfb, 0x42, 0x69, 0x5d, 0x41, 0x24,
                    0xf7, 0x69, 0x8f, 0xa4, 0x75, 0xc8, 0x51, 0xa1, 0x27, 0x55,
                    0x88, 0x85, 0x78, 0xcf, 0x5e, 0x7d, 0xc9, 0xfe, 0xcf, 0x9c,
                    0x50, 0xf2, 0x6c, 0x07, 0xf1, 0x84, 0x8c, 0xa9, 0x91, 0x03,
                    0xb2, 0xf0, 0x3c, 0x1c, 0xa5, 0x18, 0xbe, 0x7f, 0x94, 0x0e,
                    0xde, 0x16, 0x14, 0x5e, 0x4c, 0x5e, 0x7f, 0x9e, 0xff, 0x3a,
                    0x87, 0x44, 0x70, 0xa7, 0xba, 0x52, 0xe2, 0xa6, 0x4e, 0x91,
                    0x23, 0x9c, 0xac, 0x72, 0x29, 0xec, 0x2f, 0x05, 0xb1, 0x7a,
                    0x3c, 0x03, 0xcb, 0x89, 0xb4, 0x98, 0x9e, 0xa8, 0xe7, 0xbe,
                    0xba, 0x16, 0xaf, 0xcd, 0xa5, 0xfb, 0x93, 0xab, 0x80, 0x6d,
                    0xfe, 0xcb, 0x5e, 0xbb, 0x02, 0x19, 0x54, 0xc5, 0x0d, 0x3a,
                    0xf6, 0x0d, 0xa0, 0xb4, 0x4f, 0xae, 0x98, 0x32, 0xa9, 0x37,
                    0x3a, 0xb6, 0x6d, 0x46, 0x2a, 0x43, 0x55, 0x6e, 0x43, 0x14,
                    0x56, 0xca, 0x67, 0x85, 0x84, 0xb7, 0xfa, 0x0d, 0xa5, 0xdd,
                    0x81, 0x22, 0x65, 0xbc, 0xfb, 0xcb, 0x3d, 0x76, 0x8e, 0xdd,
                    0x67, 0x9c, 0xa5, 0xc6, 0x86, 0xbb, 0x49, 0x13, 0xd9, 0xeb,
                    0x8a, 0x6a, 0x4b, 0x05, 0x34, 0x9a, 0x47, 0x5f, 0x24, 0x97,
                    0xf8, 0xe7, 0x6c, 0x21, 0x2c, 0xb5, 0xa8, 0xb1, 0x0e, 0xfc,
                    0xdb, 0xa2, 0xf9, 0x97, 0x15, 0x82, 0x0f, 0xf7, 0x51, 0xfa,
                    0x93, 0x0e, 0x58, 0xb8, 0x8f, 0xb0, 0x19, 0x4c, 0x5b, 0x4e,
                    0xb7, 0xcd, 0x91, 0x5a, 0x19, 0x4a, 0xe0, 0x42, 0xe5, 0x68,
                    0x8f, 0x9e, 0xcc, 0xf3, 0x3f, 0xf8, 0xd9, 0x64, 0x67, 0xd2,
                    0xa1, 0x33, 0xd7, 0xbf, 0x8d, 0xeb, 0x7f, 0xdd, 0x9d, 0x23,
                    0xe3, 0x49, 0xbb, 0xc3, 0x0d, 0x5e, 0xcf, 0xdb, 0x9a, 0x17,
                    0xd4, 0xe4, 0x34, 0x6f, 0x44, 0x2b, 0x5b, 0xde, 0x26, 0x41,
                    0x02, 0xc1, 0x8c, 0x5d, 0xa6, 0xfd, 0xe0, 0x7d, 0xc9, 0xce,
                    0x5e, 0xbb, 0x83, 0x54, 0x60, 0x31, 0xf0, 0x58, 0x52, 0x1b,
                    0xe6, 0xb3, 0xf8, 0xfd, 0xda, 0x21, 0x39, 0x87, 0x1f, 0x91,
                    0x46, 0xab, 0x96, 0x28, 0x0d, 0xf2, 0x9d, 0x28, 0x11, 0x7b,
                    0xf4, 0x23, 0xd3, 0x4b, 0x30, 0x7d, 0x44, 0x46, 0x84, 0x95,
                    0xbf, 0xc0, 0xa4, 0xf5, 0xda, 0x24, 0x71, 0x2f, 0xe3, 0xa4,
                    0xea, 0x12, 0xfd, 0xc9, 0x21, 0xe4, 0x07, 0xd1, 0x58, 0x92,
                    0x18, 0x95, 0xc8, 0x7f, 0x9f, 0xa8, 0xdd, 0x3e, 0x5d, 0xb9,
                    0x54, 0xee, 0x44, 0x71, 0x32, 0x89, 0x78, 0x71, 0x4f, 0x6c,
                    0xef, 0xcb, 0x5e, 0xc9, 0xa5, 0xf8, 0xc1, 0x61, 0x25, 0x1d,
                    0x08, 0xad, 0x30, 0x0f, 0xca, 0xde, 0xb5, 0xae, 0xb3, 0xc6,
                    0x4b, 0x34, 0xe6, 0x04, 0x5b, 0xcd, 0x49, 0x98, 0x40, 0x40,
                    0x63, 0xfe, 0x86, 0xc0, 0xb4, 0x1e, 0x29, 0xcc, 0x0f, 0x78,
                    0x1c, 0xff, 0x80, 0x14, 0x59, 0xd4, 0xd9, 0xe2, 0x3d, 0x29,
                    0xd7, 0xd1, 0x19, 0xd2, 0x62, 0x5e, 0x70, 0x1a, 0x26, 0xca,
                    0xc1, 0x35, 0x1b, 0x33, 0x84, 0xf8, 0xfd, 0xe7, 0xf4, 0xb8,
                    0xaf, 0x0d, 0xe2, 0x52, 0x89, 0x9a, 0xb7, 0xca, 0x71, 0xe2,
                    0x86, 0x4d, 0xb3, 0x39, 0x6a, 0x2c, 0x26, 0xf3, 0xfe, 0x54,
                    0x45, 0x17, 0xdb, 0xdf, 0xc4, 0xa1, 0xdd, 0xbb, 0x05, 0xa7,
                    0x3f, 0x98, 0x81, 0x81, 0x3a, 0x88, 0x18, 0xe2, 0xfc, 0xbb,
                    0x7a, 0x59, 0x72, 0x65, 0x17, 0x4b, 0x57, 0x35, 0xdf, 0xf8,
                    0x1e, 0x95, 0x4e, 0x37, 0x9d, 0xac, 0xe4, 0xf3, 0x36, 0x36,
                    0x07, 0x0b, 0x46, 0x18, 0x5e, 0xf4, 0x6f, 0x96, 0x80, 0xca,
                    0xad, 0x04, 0x8f, 0x13, 0xba, 0x2b, 0x5a, 0x81, 0x63, 0xb9,
                    0xc7, 0xbc, 0x9a, 0x26, 0xb4, 0xf4, 0x8d, 0x80, 0x5f, 0x89,
                    0xfb, 0xb0, 0xf2, 0x3c, 0xbb, 0x39, 0x9c, 0xb2, 0x22, 0x32,
                    0x8e, 0x0e, 0x7b, 0x88, 0x91, 0x5f, 0xce, 0xae, 0x8e, 0x75,
                    0xf5, 0x6b, 0x43, 0x87, 0xf5, 0x1a, 0xd2, 0x50, 0x35, 0x02,
                    0xfd, 0x85, 0x31, 0x0c, 0x6d, 0xd6, 0xab, 0x26, 0x70, 0xb4,
                    0x77, 0x60, 0xcb, 0xcb, 0xb7, 0x9e, 0x3d, 0xf7, 0x56, 0x1c,
                    0x01, 0x05, 0x6b, 0x00, 0xd4, 0x6d, 0x29, 0x68, 0x9c, 0x95,
                    0xb9, 0x79, 0x95, 0xcb, 0x96, 0x1f, 0x3b, 0x04, 0xd3, 0x1f,
                    0x4a, 0xb3, 0x6d, 0x58, 0x35, 0x24, 0xae, 0xbe, 0x4a, 0xd5,
                    0x97, 0x61, 0x2b, 0xd1, 0x10, 0xeb, 0xc7, 0x24, 0x02, 0xc6,
                    0x22, 0xb6, 0xb7, 0xb7, 0xa1, 0xdb, 0x69, 0xca, 0x42, 0xe1,
                    0xfc, 0x57, 0x98, 0xa3, 0xf9, 0xcb, 0x20, 0x58, 0x31, 0xc9,
                    0x4e, 0xfd, 0xb8, 0xed, 0x19, 0x4f, 0x5c, 0xeb, 0xaf, 0x47,
                    0x2d, 0x9d, 0xdd, 0x57, 0x11, 0x16, 0x78, 0x0e, 0x66, 0x10,
                    0xc1, 0x88, 0x61, 0x3b, 0x3b, 0x66, 0x37, 0x80, 0x62, 0x54,
                    0x67, 0x8e, 0x59, 0x64, 0x3a, 0x10, 0x72, 0x24, 0xd3, 0x60,
                    0x05, 0xc1, 0xfc, 0x59, 0x3f, 0x83, 0x28, 0x16, 0x64, 0x32,
                    0xc9, 0xb6, 0x9e, 0xf8, 0x7f, 0x1e, 0x7c, 0xae, 0x79, 0x6f,
                    0xec, 0xef, 0x82, 0x72, 0xe5, 0xd0, 0x49, 0x5b, 0x60, 0x0a,
                    0x88, 0xa0, 0xcd, 0x1d, 0xf0, 0x05, 0x54, 0x16, 0x77, 0xd2,
                    0x8b, 0xb1, 0x78, 0xd2, 0x7f, 0x5e, 0x5f, 0x3b, 0xe9, 0x28,
                    0x43, 0x13, 0x83, 0x50, 0x36, 0xc9, 0xd2, 0x61, 0xd0, 0xd0,
                    0xa8, 0xeb, 0x7a, 0xc2, 0xe3, 0xc9, 0x09, 0x66, 0xbe, 0x6d,
                    0x4d, 0x1a, 0xfb, 0xa8, 0xdf, 0xac, 0x5d, 0x53, 0x38, 0x03,
                    0x3c, 0x47, 0xf5, 0x88, 0xd5, 0xd8, 0xf4, 0xc4, 0x7b, 0x5f,
                    0x26, 0x6d, 0xa9, 0xbe, 0xd2, 0x2a, 0x36, 0x6a, 0x5c, 0x42,
                    0x95, 0xe1, 0x5c, 0xea, 0xa8, 0xdd, 0xe2, 0x5c, 0x4a, 0x77,
                    0xcf, 0x8d, 0x10, 0xda, 0xb0, 0x8e, 0xad, 0x41, 0x73, 0xda,
                    0x4d, 0xb4, 0xbf, 0x6f, 0x5f, 0x06, 0x87, 0x7d, 0xe1, 0xee,
                    0x8a, 0x50, 0x1f, 0xd1, 0x32, 0xae, 0x60, 0x57, 0xef, 0x52,
                    0x0e, 0xf8, 0x74, 0xcb, 0x99, 0x21, 0xb8, 0x71, 0x57, 0xa2,
                    0x9a, 0xda, 0x71, 0x66, 0x9b, 0xe6, 0x27, 0x0f, 0x87, 0x92,
                    0x47, 0x27, 0xc2, 0x6f, 0x4f, 0x2c, 0x69, 0x19, 0x83, 0x2a,
                    0xcb, 0xf2, 0xaa, 0x87, 0xbe, 0xc1, 0x45, 0x4b, 0xd5, 0x06,
                    0xf5, 0xf0, 0xbb, 0x80, 0xf9, 0x26, 0xb0, 0x82, 0xb1, 0xb2,
                    0x98, 0xe3, 0x92, 0xe0, 0x10, 0x13, 0x9d, 0xe3, 0x6a, 0xb4,
                    0x42, 0x0e, 0x02, 0x92, 0x47, 0x3b, 0x04, 0xaf, 0x4d, 0x82,
                    0x74, 0xaa, 0xb6, 0x11, 0x81, 0x2e, 0x10, 0x91, 0x5f, 0x5c,
                    0xe7, 0xf2, 0x07, 0x48, 0x71, 0x3a, 0xc9, 0x4b, 0x7b, 0x9c,
                    0xd5, 0xda, 0x00, 0x08, 0x5f, 0xe2, 0x7f, 0x89, 0x71, 0xcc,
                    0xaf, 0x0e, 0x1b, 0x9b, 0xe1, 0xee, 0xb0, 0x29, 0x1d, 0x3d,
                    0xec, 0x85, 0x48, 0x62, 0x7a, 0x64, 0xa8, 0x7f, 0xc9, 0x56,
                    0xa7, 0x23, 0x53, 0x53, 0x16, 0xfd, 0x44, 0x93, 0x2a, 0xf8,
                    0xec, 0xaf, 0x75, 0x82, 0x80, 0xf2, 0x65, 0x05, 0xe5, 0x69,
                    0x95, 0x8d, 0xe5, 0x7f, 0xe3, 0x8f, 0x7e, 0x69, 0x4b, 0x00,
                    0xc6, 0x55, 0xb2, 0x43, 0x10, 0xfe, 0xf7, 0x1f, 0x42, 0x6f,
                    0xd2, 0xcf, 0xf1, 0x3d, 0xc3, 0x0a, 0xf4, 0x0f, 0x90, 0xd2,
                    0x49, 0xe1, 0xcc, 0xeb, 0x4d, 0x4d, 0x04, 0x3c, 0x86, 0xc2,
                    0xa7, 0xb1, 0xba, 0x62, 0x72, 0xe5, 0x58, 0x0a, 0x73, 0x01,
                    0xcb, 0x88, 0x18, 0x8e, 0xa5, 0x7f, 0x9d, 0x46, 0x67, 0x64,
                    0x8a, 0x07, 0xe9, 0xce, 0xa9, 0xad, 0x7d, 0xbc, 0xe8, 0x0b,
                    0xaa, 0xab, 0x76, 0x81, 0xc8, 0x4c, 0x05, 0x58, 0xd2, 0x7d,
                    0xe0, 0x44, 0x00, 0x5d, 0xe9, 0xda, 0x6f, 0x78, 0x5d, 0x90,
                    0xe2, 0x0d, 0xe1, 0x1d, 0x64, 0x2e, 0x25, 0x16, 0x1c, 0xf1,
                    0x2c, 0x21, 0x3b, 0x00, 0x7f, 0x3e, 0x88, 0xf2, 0x52, 0x71,
                    0xd1, 0x3e, 0x97, 0x31, 0x45, 0x96, 0x6f, 0x71, 0xd2, 0xa8,
                    0xf5, 0x30, 0x24, 0x6c, 0x4a, 0xfb, 0x49, 0x6b, 0x5e, 0x42,
                    0x71, 0x9f, 0xd9, 0x47, 0xae, 0x1c, 0x9b, 0xe7, 0x8b, 0x75,
                    0x78, 0xc6, 0xf2, 0xb2, 0x8f, 0xdd, 0x27, 0x76, 0x55, 0x53,
                    0x05, 0x6d,
                },
            .ecdsa_p256_public_key =
                {
                    0x04, 0xf4, 0xa0, 0xfc, 0xb3, 0x7d, 0x4d, 0xa1, 0x94, 0x3e,
                    0x76, 0xd8, 0xbe, 0x6e, 0x83, 0x1a, 0x03, 0x59, 0x40, 0xe2,
                    0x4b, 0x61, 0xc1, 0xc7, 0x2e, 0xc9, 0x98, 0x84, 0x0e, 0x34,
                    0xd8, 0x62, 0xd4, 0xd8, 0xa4, 0x05, 0xe2, 0xaf, 0x8f, 0x7a,
                    0xb4, 0x46, 0x21, 0x0e, 0xc0, 0x3b, 0xd0, 0xb5, 0x2b, 0x4f,
                    0x2b, 0x30, 0x82, 0x16, 0x43, 0x5c, 0x3f, 0x11, 0xcf, 0xee,
                    0x3e, 0xa8, 0x05, 0xf9, 0x05,
                },
        },
    }};

// Storage for scoped test overrides of pinned hybrid public keys.
constinit std::array<std::optional<PinnedHybridPublicKey>, kNumPinnedHybridKeys>
    g_pinned_hybrid_key_overrides;

// Returns true if `signature` uses the canonical hybrid post-quantum signature
// prefix ("<key_id>:" with key_id in '1'..'3').
bool IsHybridSeedSignature(std::string_view signature) {
  static_assert(kNumPinnedHybridKeys <= 9);
  return signature.size() >= 2 && signature[1] == ':' && signature[0] >= '1' &&
         signature[0] <= static_cast<char>('0' + kNumPinnedHybridKeys);
}

// Verifies a Tink COMPOSITE_SIGN_MLDSA_44_ECDSA_P256_TINK_RAW signature.
VerifySignatureResult VerifyHybridSeedSignature(
    const std::string& seed_bytes,
    std::string_view key_id_str,
    std::string_view base64_signature) {
  // Require canonical single-digit key IDs in [1, kNumPinnedHybridKeys].
  static_assert(kNumPinnedHybridKeys <= 9);
  if (key_id_str.size() != 1 || key_id_str[0] < '1' ||
      key_id_str[0] > static_cast<char>('0' + kNumPinnedHybridKeys)) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }
  const size_t key_index = static_cast<size_t>(key_id_str[0] - '1');
  const PinnedHybridPublicKey& pinned_key =
      g_pinned_hybrid_key_overrides[key_index]
          ? *g_pinned_hybrid_key_overrides[key_index]
          : kPinnedHybridKeys[key_index];

  std::string raw_signature;
  if (!base::Base64Decode(base64_signature, &raw_signature)) {
    return VerifySignatureResult::kDecodeFailed;
  }
  if (raw_signature.size() <= MLDSA44_SIGNATURE_BYTES) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }

  // Construct M' = "CompositeAlgorithmSignatures2025" || Label || '\x00' ||
  // SHA256(seed_bytes).
  const std::array<uint8_t, crypto::hash::kSha256Size> seed_hash =
      crypto::hash::Sha256(seed_bytes);
  const std::string m_prime = base::StrCat(
      {kCompositeSignaturePrefix, kCompositeSignatureLabel,
       std::string_view("\0", 1), base::as_string_view(seed_hash)});
  const base::span<const uint8_t> m_prime_bytes = base::as_byte_span(m_prime);

  auto [mldsa_sig, ecdsa_sig] = base::as_byte_span(raw_signature)
                                    .split_at(size_t{MLDSA44_SIGNATURE_BYTES});

  // 1. Verify DER-encoded ECDSA P-256 SHA-256 signature over M' first so
  // bad ECDSA signatures fail fast without allocating MLDSA44_public_key.
  std::optional<crypto::keypair::PublicKey> ecdsa_pub =
      crypto::keypair::PublicKey::FromEcP256Point(
          pinned_key.ecdsa_p256_public_key);
  if (!ecdsa_pub ||
      !crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                            *ecdsa_pub, m_prime_bytes, ecdsa_sig)) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }

  // 2. Verify ML-DSA-44 signature over M' with context = Label.
  // Allocate MLDSA44_public_key on the heap to avoid a ~4 KiB stack frame.
  auto mldsa_pub = std::make_unique<MLDSA44_public_key>();
  CBS cbs;
  CBS_init(&cbs, pinned_key.mldsa44_public_key.data(),
           pinned_key.mldsa44_public_key.size());
  if (!MLDSA44_parse_public_key(mldsa_pub.get(), &cbs)) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }
  if (!MLDSA44_verify(mldsa_pub.get(), mldsa_sig.data(), mldsa_sig.size(),
                      m_prime_bytes.data(), m_prime_bytes.size(),
                      base::as_byte_span(kCompositeSignatureLabel).data(),
                      kCompositeSignatureLabel.size())) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }

  return VerifySignatureResult::kValidSignature;
}

// LINT.IfChange
// The name of the seed file that stores the latest seed data and other
// seed-related information in a compressed proto.
const base::FilePath::CharType kSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSeedV2");
// LINT.ThenChange(/components/variations/variations_safe_seed_store.cc,
// /chrome/browser/metrics/variations/variations_safe_mode_end_to_end_browsertest.cc)

// Returns true if |signature| is empty and if the command-line flag to accept
// empty seed signature is specified.
bool AcceptEmptySeedSignatureForTesting(const std::string& signature) {
  return signature.empty() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAcceptEmptySeedSignatureForTesting);
}

// Verifies a variations seed (the serialized proto bytes) with the specified
// signature string received from the server or loaded from disk.
//
// Two formats are supported in `X-Seed-Signature`:
// - Hybrid PQ: "<key_id>:<base64_signature>" where key_id is 1, 2, or 3
//   (Tink COMPOSITE_SIGN_MLDSA_44_ECDSA_P256_TINK_RAW).
// - Legacy: "<base64_signature>" (ECDSA P-256 SHA-256 using `kPublicKey`).
//   Rejected when `legacy_key_distrusted` is true.
VerifySignatureResult VerifySeedSignature(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature,
    bool legacy_key_distrusted) {
  if (base64_seed_signature.empty()) {
    return VerifySignatureResult::kMissingSignature;
  }

  // Hybrid signatures are formatted as "<key_id>:<base64_signature>". Legacy
  // base64 signatures never contain ':'.
  const size_t colon_pos = base64_seed_signature.find(':');
  if (colon_pos != std::string::npos) {
    const std::string_view full_view(base64_seed_signature);
    return VerifyHybridSeedSignature(seed_bytes, full_view.substr(0, colon_pos),
                                     full_view.substr(colon_pos + 1));
  }

  // Legacy ECDSA P-256 signature.
  if (legacy_key_distrusted) {
    return VerifySignatureResult::kLegacyKeyDistrusted;
  }

  std::string signature;
  if (!base::Base64Decode(base64_seed_signature, &signature)) {
    return VerifySignatureResult::kDecodeFailed;
  }

  std::optional<crypto::keypair::PublicKey> public_key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(kPublicKey);
  // Since kPublicKey is hardcoded, if it fails to load, that's either
  // programmer error or a corrupt binary.
  CHECK(public_key);

  if (!crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                            *public_key, base::as_byte_span(seed_bytes),
                            base::as_byte_span(signature))) {
    return VerifySignatureResult::kInvalidSeedSignature;
  }

  return VerifySignatureResult::kValidSignature;
}

// Truncates a time to the start of the day in UTC. If given a time representing
// 2014-03-11 10:18:03.1 UTC, it will return a time representing
// 2014-03-11 00:00:00.0 UTC.
base::Time TruncateToUTCDay(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time;
}

UpdateSeedDateResult GetSeedDateChangeState(base::Time server_seed_date,
                                            base::Time stored_seed_date) {
  CHECK(!server_seed_date.is_null());
  CHECK(!stored_seed_date.is_null());

  if (server_seed_date < stored_seed_date) {
    return UpdateSeedDateResult::kNewDateIsOlder;
  }

  if (TruncateToUTCDay(server_seed_date) !=
      TruncateToUTCDay(stored_seed_date)) {
    // The server date is later than the stored date, and they are from
    // different UTC days, so |server_seed_date| is a valid new day.
    return UpdateSeedDateResult::kNewDay;
  }
  return UpdateSeedDateResult::kSameDay;
}

// Dumps without crashing if the uncompressed size of a validated seed exceeds
// `kDumpThreshold`, alerting us that actual seeds are approaching the
// `kMaxUncompressedSeedSize` rejection threshold in `Uncompress()`.
void MaybeDumpIfSeedSizeApproachingLimit(size_t uncompressed_seed_size) {
  constexpr static base::ByteSize kDumpThreshold = base::MiB(40);
  if (uncompressed_seed_size > kDumpThreshold.InBytes()) {
    base::debug::DumpWithoutCrashing();
  }
}

// Remove gzip compression from |data|.
// Returns success or error, populating result on success.
StoreSeedResult Uncompress(const std::string& compressed, std::string* result) {
  DCHECK(result);
  uint32_t uncompressed_size = compression::GetUncompressedSize(compressed);

  // Note: We intentionally do not call `MaybeDumpIfSeedSizeApproachingLimit()`
  // here because `GetUncompressedSize()` blindly reads the 4-byte gzip trailer
  // before decompression or signature validation, so corrupt or truncated
  // payloads would trigger false-positive dumps. Instead, the helper is called
  // in `ProcessSeedData()` after `ValidateSeedBytes()` succeeds.
  // Enforce a maximum uncompressed size to prevent OOM / Gzip bomb crashes.
  // We use 50 MiB as a conservative limit, similar to seed_reader_writer.cc.
  constexpr static base::ByteSize kMaxUncompressedSeedSize = base::MiB(50);
  if (uncompressed_size > kMaxUncompressedSeedSize.InBytes()) {
    VLOG(1) << "Rejecting seed: uncompressed size " << uncompressed_size
            << " exceeds limit of " << kMaxUncompressedSeedSize.InBytes();
    return StoreSeedResult::kUncompressedSizeLimitExceeded;
  }
  if (!compression::GzipUncompress(compressed, result)) {
    return StoreSeedResult::kFailedUngzip;
  }
  if (result->empty()) {
    return StoreSeedResult::kFailedEmptyGzipContents;
  }
  return StoreSeedResult::kSuccess;
}

// Wrapper to record the time elapsed between the call of this function and the
// call to the given callback.
template <typename... Args>
base::OnceCallback<void(Args...)> CallbackUmaHistogramTimer(
    base::OnceCallback<void(Args...)> done_callback,
    std::string_view histogram_name) {
  return base::BindOnce(
      [](base::OnceCallback<void(Args...)> done_callback,
         std::string histogram_name, base::TimeTicks start_time, Args... args) {
        base::UmaHistogramTimes(histogram_name,
                                base::TimeTicks::Now() - start_time);
        std::move(done_callback).Run(std::forward<Args>(args)...);
      },
      std::move(done_callback), std::string(histogram_name),
      base::TimeTicks::Now());
}

}  // namespace

ValidatedSeed::ValidatedSeed() = default;
ValidatedSeed::~ValidatedSeed() = default;
ValidatedSeed::ValidatedSeed(ValidatedSeed&& other) = default;
ValidatedSeed& ValidatedSeed::operator=(ValidatedSeed&& other) = default;

VariationsSeedStore::VariationsSeedStore(
    PrefService* local_state,
    std::unique_ptr<SeedResponse> initial_seed,
    bool signature_verification_enabled_on_load,
    bool signature_verification_enabled_on_receive,
    std::unique_ptr<VariationsSafeSeedStore> safe_seed_store,
    version_info::Channel channel,
    const base::FilePath& seed_file_dir,
    const EntropyProviders* entropy_providers,
    bool use_first_run_prefs,
    base::OnceClosure on_initial_seed_consumed)
    : local_state_(local_state),
      safe_seed_store_(std::move(safe_seed_store)),
      signature_verification_enabled_on_load_(
          signature_verification_enabled_on_load),
      signature_verification_enabled_on_receive_(
          signature_verification_enabled_on_receive),
      use_first_run_prefs_(use_first_run_prefs),
      seed_reader_writer_(
          std::make_unique<SeedReaderWriter>(local_state,
                                             seed_file_dir,
                                             kSeedFilename,
                                             kRegularSeedFieldsPrefs,
                                             channel,
                                             entropy_providers,
                                             /*histogram_suffix=*/"Latest")) {
  if (initial_seed) {
    ImportInitialSeed(std::move(initial_seed),
                      std::move(on_initial_seed_consumed));
  } else if (on_initial_seed_consumed) {
    std::move(on_initial_seed_consumed).Run();
  }
}

VariationsSeedStore::~VariationsSeedStore() = default;

void VariationsSeedStore::LoadSeed(LoadSeedCallback done_callback,
                                   bool require_synchronous) {
  CHECK(!seed_purged_from_local_state_)
      << "LoadSeed() cannot be called after PurgeSeedFromLocalStatePrefs().";
  auto verify_and_parse_seed_cb =
      base::BindOnce(&VariationsSeedStore::VerifyAndParseSeedAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     SeedType::LATEST);
  ReadSeedData(std::move(verify_and_parse_seed_cb), SeedType::LATEST,
               require_synchronous);
}

bool VariationsSeedStore::LoadSeedSync(VariationsSeed* seed,
                                       std::string* seed_data,
                                       std::string* base64_seed_signature) {
  std::optional<bool> success;
  // It's safe to pass pointers here because the callback runs synchronously.
  LoadSeed(base::BindOnce(
               [](std::string* seed_data, std::string* seed_signature,
                  std::optional<bool>* success, VariationsSeed* seed,
                  std::string cb_seed_data, std::string cb_seed_signature,
                  bool cb_success, VariationsSeed cb_seed) {
                 *success = cb_success;
                 *seed = std::move(cb_seed);
                 *seed_data = std::move(cb_seed_data);
                 *seed_signature = std::move(cb_seed_signature);
               },
               seed_data, base64_seed_signature, &success, seed),
           /*require_synchronous=*/true);
  CHECK(success.has_value())
      << "LoadSeed callback should have run synchronously.";
  return success.value();
}

void VariationsSeedStore::StoreSeedData(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    std::string data,
    std::string base64_seed_signature,
    std::string country_code,
    std::string geo_level1,
    base::Time seed_date,
    bool is_delta_compressed,
    bool is_gzip_compressed,
    bool require_synchronous) {
  auto done_callback_with_timer = CallbackUmaHistogramTimer(
      std::move(done_callback), /*histogram_name=*/"Variations.StoreSeed.Time");

  base::UmaHistogramCounts1000("Variations.StoreSeed.DataSize",
                               data.length() / 1024);
  InstanceManipulations im = {
      .gzip_compressed = is_gzip_compressed,
      .delta_compressed = is_delta_compressed,
  };
  RecordSeedInstanceManipulations(im);

  // Note: SeedData is move-only, so it will be moved into a param below.
  SeedData seed_data;
  seed_data.data = std::move(data);
  seed_data.base64_seed_signature = std::move(base64_seed_signature);
  seed_data.country_code = std::move(country_code);
  seed_data.geo_level1 = std::move(geo_level1);
  seed_data.seed_date = seed_date;
  seed_data.is_gzip_compressed = is_gzip_compressed;
  seed_data.is_delta_compressed = is_delta_compressed;

  if (is_delta_compressed) {
    ReadSeedData(
        /*done_callback=*/base::BindOnce(
            &VariationsSeedStore::ProcessAndStoreSeedData,
            weak_ptr_factory_.GetWeakPtr(), std::move(done_callback_with_timer),
            std::move(seed_data), require_synchronous),
        SeedType::LATEST, require_synchronous);
  } else {
    ProcessAndStoreSeedData(
        std::move(done_callback_with_timer), std::move(seed_data),
        require_synchronous,
        SeedReaderWriter::ReadSeedDataResult{LoadSeedResult::kSuccess, "", ""});
  }
}

void VariationsSeedStore::LoadSafeSeed(LoadSeedCallback done_callback,
                                       bool require_synchronous) {
  auto verify_and_parse_seed_cb = base::BindOnce(
      &VariationsSeedStore::VerifyAndParseSeedAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(done_callback), SeedType::SAFE);
  ReadSeedData(std::move(verify_and_parse_seed_cb), SeedType::SAFE,
               require_synchronous);
}

bool VariationsSeedStore::LoadSafeSeedSync(
    VariationsSeed* seed,
    ClientFilterableState* client_state) {
  std::string unused_seed_data;
  std::string unused_base64_seed_signature;
  std::optional<bool> success;
  LoadSafeSeed(
      base::BindOnce(
          [](std::string* seed_data, std::string* seed_signature,
             std::optional<bool>* success, VariationsSeed* seed,
             std::string cb_seed_data, std::string cb_seed_signature,
             bool cb_success, VariationsSeed cb_seed) {
            *success = cb_success;
            *seed = std::move(cb_seed);
            *seed_data = std::move(cb_seed_data);
            *seed_signature = std::move(cb_seed_signature);
          },
          &unused_seed_data, &unused_base64_seed_signature, &success, seed),
      /*require_synchronous=*/true);
  CHECK(success.has_value())
      << "LoadSafeSeed callback should have run synchronously.";
  if (!success.value()) {
    return false;
  }

  // TODO(crbug.com/40202311): While it's not immediately obvious,
  // |client_state| is not used for successfully loaded safe seeds that are
  // rejected after additional validation (expiry and future milestone).
  client_state->reference_date =
      GetTimeForStudyDateChecks(/*is_safe_seed=*/true);
  client_state->locale = safe_seed_store_->GetLocale();
  client_state->permanent_consistency_country =
      safe_seed_store_->GetPermanentConsistencyCountry();
  client_state->session_consistency_country =
      safe_seed_store_->GetSessionConsistencyCountry();
  return true;
}

void VariationsSeedStore::StoreSafeSeed(
    base::OnceCallback<void(bool)> done_callback,
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    int seed_milestone,
    const ClientFilterableState& client_state,
    base::Time seed_fetch_time) {
  ValidatedSeed seed;
  // TODO(crbug.com/40839193): See if we can avoid calling this on the UI
  // thread.
  const bool legacy_key_distrusted =
      local_state_->GetBoolean(prefs::kVariationsLegacySeedKeyDistrusted);
  if (signature_verification_enabled_on_receive_ && legacy_key_distrusted &&
      !base64_seed_signature.empty() &&
      !IsHybridSeedSignature(base64_seed_signature)) {
    // During the transition session, the active seed from startup may still be
    // legacy-signed after a newly fetched PQ-signed latest seed stops trusting
    // the legacy key. Skip promoting it without logging a signature-failure
    // spike.
    std::move(done_callback).Run(false);
    return;
  }
  StoreSeedResult validation_result = ValidateSeedBytes(
      seed_data, base64_seed_signature, SeedType::SAFE,
      signature_verification_enabled_on_receive_, legacy_key_distrusted, &seed);
  if (validation_result != StoreSeedResult::kSuccess) {
    RecordStoreSafeSeedResult(validation_result);
    std::move(done_callback).Run(false);
    return;
  }

  auto on_validated_safe_seed_stored_cb =
      base::BindOnce(&VariationsSeedStore::OnValidatedSafeSeedStored,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback));

  auto store_validated_safe_seed_cb = base::BindOnce(
      &VariationsSeedStore::StoreValidatedSafeSeed,
      weak_ptr_factory_.GetWeakPtr(),
      /*done_callback=*/std::move(on_validated_safe_seed_stored_cb),
      std::move(seed), seed_milestone, client_state.reference_date,
      client_state.session_consistency_country,
      client_state.permanent_consistency_country, client_state.locale,
      seed_fetch_time);

  // Read both seeds and call VariationsSeedStore::StoreValidatedSafeSeed()
  ReadBothSeedsData(std::move(store_validated_safe_seed_cb));
}

void VariationsSeedStore::ReadBothSeedsData(
    ReadBothSeedsCallback done_callback) {
  // Finish reading safe before starting latest: initialization callbacks may
  // update latest while the safe read is pending.
  ReadSeedData(
      base::BindOnce(&VariationsSeedStore::OnSafeSeedRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)),
      SeedType::SAFE,
      /*require_synchronous=*/false);
}

void VariationsSeedStore::OnSafeSeedRead(
    ReadBothSeedsCallback done_callback,
    SeedReaderWriter::ReadSeedDataResult safe_seed_read_result) {
  ReadSeedData(base::BindOnce(std::move(done_callback),
                              std::move(safe_seed_read_result)),
               SeedType::LATEST,
               /*require_synchronous=*/false);
}

void VariationsSeedStore::OnValidatedSafeSeedStored(
    base::OnceCallback<void(bool)> done_callback,
    StoreSeedResult validation_result) {
  RecordStoreSafeSeedResult(validation_result);
  std::move(done_callback).Run(validation_result == StoreSeedResult::kSuccess);
}

base::Time VariationsSeedStore::GetLatestSeedFetchTime() const {
  return seed_reader_writer_->GetSeedInfo().client_fetch_time;
}

base::Time VariationsSeedStore::GetSafeSeedFetchTime() const {
  return safe_seed_store_->GetFetchTime();
}

int VariationsSeedStore::GetLatestMilestone() const {
  return seed_reader_writer_->GetSeedInfo().milestone;
}

int VariationsSeedStore::GetSafeSeedMilestone() const {
  return safe_seed_store_->GetMilestone();
}

base::Time VariationsSeedStore::GetLatestTimeForStudyDateChecks() const {
  return seed_reader_writer_->GetSeedInfo().seed_date;
}

base::Time VariationsSeedStore::GetSafeSeedTimeForStudyDateChecks() const {
  return safe_seed_store_->GetTimeForStudyDateChecks();
}

base::Time VariationsSeedStore::GetTimeForStudyDateChecks(bool is_safe_seed) {
  base::Time seed_date = is_safe_seed ? GetSafeSeedTimeForStudyDateChecks()
                                      : GetLatestTimeForStudyDateChecks();
  const base::Time build_time = base::GetBuildTime();

  // Use the build time for date checks if either the seed date is unknown or
  // the build time is newer than the seed date.
  if (seed_date.is_null() || seed_date < build_time) {
    return build_time;
  }
  return seed_date;
}

void VariationsSeedStore::RecordLastFetchTime(base::Time fetch_time) {
  CHECK(!fetch_time.is_null()) << "Can't record null fetch time.";
  seed_reader_writer_->SetFetchTime(fetch_time);
  // If the latest and safe seeds are identical, update the fetch time for the
  // safe seed as well.
  if (seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    safe_seed_store_->SetFetchTime(fetch_time);
  }
}

UpdateSeedDateResult VariationsSeedStore::UpdateSeedDateAndLogDayChange(
    base::Time seed_date) {
  const UpdateSeedDateResult result = GetSeedDateUpdateResult(seed_date);
  LogSeedDayChange(seed_date);
  seed_reader_writer_->SetSeedDate(seed_date);
  return result;
}

bool VariationsSeedStore::UpdateSessionCountryAndGeoLevel1(
    std::string_view country_code,
    std::string_view geo_level1) {
  return seed_reader_writer_->SetSessionCountryAndGeoLevel1(country_code,
                                                            geo_level1);
}

UpdateSeedDateResult VariationsSeedStore::GetSeedDateUpdateResult(
    base::Time seed_date) const {
  if (seed_date.is_null()) {
    return UpdateSeedDateResult::kNoNewDate;
  }

  const base::Time stored_date = seed_reader_writer_->GetSeedInfo().seed_date;
  if (stored_date.is_null()) {
    return UpdateSeedDateResult::kNoOldDate;
  }

  return GetSeedDateChangeState(seed_date, stored_date);
}

void VariationsSeedStore::LogSeedDayChange(base::Time seed_date) {
  base::UmaHistogramEnumeration("Variations.SeedDateChange",
                                GetSeedDateUpdateResult(seed_date));
}

const std::string& VariationsSeedStore::GetLatestSerialNumber() {
  return local_state_->GetString(prefs::kVariationsSeedSerialNumber);
}

std::string VariationsSeedStore::GetLatestCountry() {
  return std::string(seed_reader_writer_->GetSeedInfo().session_country_code);
}

std::string VariationsSeedStore::GetLatestGeoLevel1() {
  return std::string(seed_reader_writer_->GetSeedInfo().session_geo_level1);
}

std::string VariationsSeedStore::GetPermanentConsistencyCountry() {
  return std::string(seed_reader_writer_->GetSeedInfo().permanent_country_code);
}

std::string VariationsSeedStore::GetPermanentConsistencyVersion() {
  return std::string(
      seed_reader_writer_->GetSeedInfo().permanent_country_version);
}

void VariationsSeedStore::ClearPermanentConsistencyCountryAndVersion() {
  seed_reader_writer_->ClearPermanentConsistencyCountryAndVersion();
}

void VariationsSeedStore::SetPermanentConsistencyCountryAndVersion(
    const std::string_view country,
    const std::string_view version) {
  seed_reader_writer_->SetPermanentConsistencyCountryAndVersion(country,
                                                                version);
}

// static
void VariationsSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
  // Regular seed prefs:
  registry->RegisterStringPref(prefs::kVariationsCompressedSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsCountry, std::string());
  registry->RegisterStringPref(prefs::kVariationsGeoLevel1, std::string());
  registry->RegisterTimePref(prefs::kVariationsLastFetchTime, base::Time());
  registry->RegisterBooleanPref(prefs::kVariationsLegacySeedKeyDistrusted,
                                false);
  registry->RegisterIntegerPref(prefs::kVariationsSeedMilestone, 0);
  registry->RegisterTimePref(prefs::kVariationsSeedDate, base::Time());
  registry->RegisterStringPref(prefs::kVariationsSeedSignature, std::string());
  // This preference keeps track of the country code used to filter
  // permanent-consistency studies.
  registry->RegisterListPref(prefs::kVariationsPermanentConsistencyCountry);
  registry->RegisterStringPref(prefs::kVariationsSeedSerialNumber,
                               std::string());

  VariationsSafeSeedStore::RegisterPrefs(registry);
}

// static
VerifySignatureResult VariationsSeedStore::VerifySeedSignatureForTesting(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  return VerifySeedSignature(seed_bytes, base64_seed_signature,
                             /*legacy_key_distrusted=*/false);
}

// static
base::ScopedClosureRunner
VariationsSeedStore::OverridePinnedHybridKeyForTesting(
    uint32_t key_id,
    base::span<const uint8_t> mldsa44_public_key,
    base::span<const uint8_t> ecdsa_p256_public_key) {
  CHECK_GE(key_id, 1u);
  CHECK_LE(key_id, kNumPinnedHybridKeys);

  PinnedHybridPublicKey key;
  base::span(key.mldsa44_public_key).copy_from(mldsa44_public_key);
  base::span(key.ecdsa_p256_public_key).copy_from(ecdsa_p256_public_key);

  const size_t index = key_id - 1;
  auto& slot = g_pinned_hybrid_key_overrides[index];
  std::optional<PinnedHybridPublicKey> previous = std::move(slot);
  slot = std::move(key);
  return base::ScopedClosureRunner(base::BindOnce(
      [](size_t index, std::optional<PinnedHybridPublicKey> previous) {
        g_pinned_hybrid_key_overrides[index] = std::move(previous);
      },
      index, std::move(previous)));
}

VariationsSeedStore::SeedData::SeedData() = default;
VariationsSeedStore::SeedData::~SeedData() = default;
VariationsSeedStore::SeedData::SeedData(VariationsSeedStore::SeedData&& other) =
    default;
VariationsSeedStore::SeedData& VariationsSeedStore::SeedData::operator=(
    VariationsSeedStore::SeedData&& other) = default;

VariationsSeedStore::SeedProcessingResult::SeedProcessingResult(
    SeedData seed_data,
    StoreSeedResult result)
    : seed_data(std::move(seed_data)), result(result) {}
VariationsSeedStore::SeedProcessingResult::~SeedProcessingResult() = default;
VariationsSeedStore::SeedProcessingResult::SeedProcessingResult(
    VariationsSeedStore::SeedProcessingResult&& other) = default;
VariationsSeedStore::SeedProcessingResult&
VariationsSeedStore::SeedProcessingResult::operator=(
    VariationsSeedStore::SeedProcessingResult&& other) = default;

// It is intentional that country-related prefs are retained for regular seeds
// and cleared for safe seeds.
//
// For regular seeds, the prefs are kept for two reasons. First, it's better to
// have some idea of a country recently associated with the device. Second, some
// past, country-gated launches started relying on the VariationsService-
// provided country when they retired server-side configs.
//
// The safe seed prefs are needed to correctly apply a safe seed, so if the safe
// seed is cleared, there's no reason to retain them as they may be incorrect
// for the next safe seed.
void VariationsSeedStore::ClearPrefs(SeedType seed_type) {
  if (seed_type == SeedType::LATEST) {
    local_state_->ClearPref(prefs::kVariationsSeedSerialNumber);
    // Seed and other related information is cleared by the SeedReaderWriter.
    seed_reader_writer_->ClearSeedInfo();
    return;
  }

  DCHECK_EQ(seed_type, SeedType::SAFE);
  safe_seed_store_->ClearState();
}

void VariationsSeedStore::ImportInitialSeed(
    std::unique_ptr<SeedResponse> initial_seed,
    base::OnceClosure on_seed_consumed) {
  // A non-null `on_seed_consumed` means the embedder keeps the first-run seed
  // staged until it is durably stored, so the same seed is offered again if a
  // prior session ended before `on_seed_consumed` ran. Skip re-importing it
  // when a non-empty seed is already stored. Callers without the callback
  // (e.g. Android WebView, which supplies a fresh seed on every startup) still
  // overwrite the stored seed.
  if (on_seed_consumed && !seed_reader_writer_->IsEmpty()) {
    std::move(on_seed_consumed).Run();
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::kSkippedSeedAlreadyStored);
    return;
  }

  if (initial_seed->data.empty()) {
    if (on_seed_consumed) {
      std::move(on_seed_consumed).Run();
    }
    // Note: This is an expected case on non-first run starts.
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::kFailNoFirstRunSeed);
    return;
  }

  if (initial_seed->date.is_null()) {
    if (on_seed_consumed) {
      std::move(on_seed_consumed).Run();
    }
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::kFailInvalidResponseDate);
    LOG(WARNING) << "Missing response date";
    return;
  }

  // `StoreSeedData()` runs `done_callback` synchronously here, so `self` is
  // expected to be valid. If the store were gone, `on_seed_consumed` would be
  // dropped without running, which leaves the staged seed in place for the
  // next startup.
  auto done_callback = base::BindOnce(
      [](base::WeakPtr<VariationsSeedStore> self,
         base::OnceClosure on_seed_consumed, bool store_success,
         VariationsSeed seed) {
        if (store_success) {
          RecordFirstRunSeedImportResult(FirstRunSeedImportResult::kSuccess);
          if (on_seed_consumed && self) {
            self->seed_reader_writer_->RunOrScheduleOnSuccessfulWrite(
                std::move(on_seed_consumed));
          }
        } else {
          if (on_seed_consumed) {
            std::move(on_seed_consumed).Run();
          }
          RecordFirstRunSeedImportResult(
              FirstRunSeedImportResult::kFailStoreFailed);
          LOG(WARNING) << "First run variations seed is invalid.";
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(on_seed_consumed));
  StoreSeedData(std::move(done_callback), std::move(initial_seed->data),
                std::move(initial_seed->signature),
                std::move(initial_seed->country),
                std::move(initial_seed->geo_level1), initial_seed->date,
                /*is_delta_compressed=*/false, initial_seed->is_gzip_compressed,
                /*require_synchronous=*/true);
}

// static
std::optional<std::string> VariationsSeedStore::SeedBytesToCompressedBase64Seed(
    const std::string& seed_bytes) {
  if (seed_bytes.empty()) {
    return std::nullopt;
  }

  std::string compressed_seed_data;
  if (!compression::GzipCompress(seed_bytes, &compressed_seed_data)) {
    return std::nullopt;
  }

  return base::Base64Encode(compressed_seed_data);
}

SeedReaderWriter* VariationsSeedStore::GetSeedReaderWriterForTesting() {
  return seed_reader_writer_.get();
}

void VariationsSeedStore::SetSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  seed_reader_writer_ = std::move(seed_reader_writer);
}

SeedReaderWriter* VariationsSeedStore::GetSafeSeedReaderWriterForTesting() {
  return safe_seed_store_->GetSeedReaderWriterForTesting();  // IN-TEST
}

void VariationsSeedStore::SetSafeSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  safe_seed_store_->SetSeedReaderWriterForTesting(  // IN-TEST
      std::move(seed_reader_writer));
}

void VariationsSeedStore::StoreLatestSerialNumber(
    std::string_view serial_number) {
  local_state_->SetString(prefs::kVariationsSeedSerialNumber, serial_number);
}

LoadSeedResult VariationsSeedStore::VerifyAndParseSeedImpl(
    VariationsSeed* seed,
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::optional<VerifySignatureResult>* verify_signature_result) {
  if (signature_verification_enabled_on_load_ &&
      !AcceptEmptySeedSignatureForTesting(base64_seed_signature)) {
    const bool legacy_key_distrusted =
        local_state_->GetBoolean(prefs::kVariationsLegacySeedKeyDistrusted);
    *verify_signature_result = VerifySeedSignature(
        seed_data, base64_seed_signature, legacy_key_distrusted);
    if (*verify_signature_result != VerifySignatureResult::kValidSignature) {
      return LoadSeedResult::kInvalidSignature;
    }
  }

  if (!seed->ParseFromString(seed_data)) {
    return LoadSeedResult::kCorruptProtobuf;
  }

  return LoadSeedResult::kSuccess;
}

// TODO: crbug.com/447171999 - Verification and parse of the seed can be done in
// a background thread after startup.
void VariationsSeedStore::VerifyAndParseSeedAndRunCallback(
    LoadSeedCallback done_callback,
    SeedType seed_type,
    SeedReaderWriter::ReadSeedDataResult read_result) {
  if (read_result.result != LoadSeedResult::kSuccess) {
    LogLoadSeedResult(seed_type, read_result.result);
    std::move(done_callback)
        .Run(/*seed_data=*/"",
             /*seed_signature=*/"", /*success=*/false, VariationsSeed());
    return;
  }
  VariationsSeed seed;
  std::optional<VerifySignatureResult> verify_signature_result;
  LoadSeedResult result =
      VerifyAndParseSeedImpl(&seed, read_result.seed_data,
                             read_result.signature, &verify_signature_result);
  if (verify_signature_result.has_value()) {
    VerifySignatureResult signature_result = verify_signature_result.value();
    if (seed_type == SeedType::LATEST) {
      base::UmaHistogramEnumeration("Variations.LoadSeedSignature",
                                    signature_result);
    } else {
      base::UmaHistogramEnumeration(
          "Variations.SafeMode.LoadSafeSeed.SignatureValidity",
          signature_result);
    }
    if (signature_result != VerifySignatureResult::kValidSignature) {
      ClearPrefs(seed_type);
    }
  }
  if (result == LoadSeedResult::kCorruptProtobuf) {
    ClearPrefs(seed_type);
  }
  if (seed_type == SeedType::LATEST && result == LoadSeedResult::kSuccess) {
    if (signature_verification_enabled_on_receive_ &&
        IsHybridSeedSignature(read_result.signature) &&
        !local_state_->GetBoolean(prefs::kVariationsLegacySeedKeyDistrusted)) {
      local_state_->SetBoolean(prefs::kVariationsLegacySeedKeyDistrusted, true);
    }
    StoreLatestSerialNumber(seed.serial_number());
  }
  LogLoadSeedResult(seed_type, result);
  std::move(done_callback)
      .Run(std::move(read_result.seed_data), std::move(read_result.signature),
           /*success=*/result == LoadSeedResult::kSuccess, std::move(seed));
}

void VariationsSeedStore::LogLoadSeedResult(SeedType seed_type,
                                            LoadSeedResult result) {
  if (seed_type == SeedType::LATEST) {
    RecordLoadSeedResult(result);
  } else {
    RecordLoadSafeSeedResult(result);
  }
}

LoadSeedResult VariationsSeedStore::ReadSeedData(
    SeedType seed_type,
    std::string* seed_data,
    std::string* base64_seed_signature) {
  LoadSeedResult load_seed_result =
      seed_type == SeedType::LATEST
          ? seed_reader_writer_->ReadSeedDataOnStartup(seed_data,
                                                       base64_seed_signature)
          : safe_seed_store_->ReadSeedData(seed_data, base64_seed_signature);
  if (load_seed_result != LoadSeedResult::kSuccess) {
    ClearPrefs(seed_type);
    return load_seed_result;
  }
  // As a space optimization, the latest seed might not be stored directly, but
  // rather aliased to the safe seed.
  if (seed_type == SeedType::LATEST &&
      seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    return ReadSeedData(SeedType::SAFE, seed_data, base64_seed_signature);
  }
  return LoadSeedResult::kSuccess;
}

void VariationsSeedStore::ReadSeedData(
    SeedReaderWriter::ReadSeedDataCallback done_callback,
    SeedType seed_type,
    bool require_synchronous) {
  const SeedType source_seed_type =
      ResolveSourceSeedType(seed_type, require_synchronous);
  auto cb = base::BindOnce(
      &VariationsSeedStore::CheckReadSeedDataResultAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(done_callback), seed_type,
      source_seed_type, require_synchronous);
  if (require_synchronous) {
    std::string seed_data;
    std::string base64_seed_signature;
    auto result = ReadSeedData(seed_type, &seed_data, &base64_seed_signature);
    std::move(cb).Run(SeedReaderWriter::ReadSeedDataResult{
        result, std::move(seed_data), std::move(base64_seed_signature)});
    return;
  }
  if (source_seed_type == SeedType::LATEST) {
    seed_reader_writer_->ReadSeedData(std::move(cb));
  } else {
    safe_seed_store_->ReadSeedData(std::move(cb));
  }
}

VariationsSeedStore::SeedType VariationsSeedStore::ResolveSourceSeedType(
    SeedType seed_type,
    bool require_synchronous) const {
  return !require_synchronous && seed_type == SeedType::LATEST &&
                 seed_reader_writer_->IsIdenticalToSafeSeedSentinel()
             ? SeedType::SAFE
             : seed_type;
}

void VariationsSeedStore::CheckReadSeedDataResultAndRunCallback(
    SeedReaderWriter::ReadSeedDataCallback done_callback,
    SeedType seed_type,
    SeedType source_seed_type,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult load_seed_result) {
  if (source_seed_type !=
      ResolveSourceSeedType(seed_type, require_synchronous)) {
    // Latest changed between its own payload and a safe alias while its source
    // was being read. Resolve the current source before using the result or
    // clearing prefs for an error from the old source.
    ReadSeedData(std::move(done_callback), seed_type, require_synchronous);
    return;
  }
  if (load_seed_result.result != LoadSeedResult::kSuccess) {
    ClearPrefs(seed_type);
  }
  std::move(done_callback).Run(std::move(load_seed_result));
}

void VariationsSeedStore::OnSeedDataProcessed(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    bool require_synchronous,
    SeedProcessingResult result) {
  if (result.result != StoreSeedResult::kSuccess) {
    RecordStoreSeedResult(result.result);
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }

  if (result.validate_result != StoreSeedResult::kSuccess) {
    RecordStoreSeedResult(result.validate_result);
    if (result.seed_data.is_delta_compressed) {
      RecordStoreSeedResult(StoreSeedResult::kFailedDeltaStore);
    }
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }

  auto store_validated_seed_cb = base::BindOnce(
      &VariationsSeedStore::StoreValidatedSeed, weak_ptr_factory_.GetWeakPtr(),
      std::move(done_callback), std::move(result.validated),
      result.seed_data.country_code, result.seed_data.geo_level1,
      result.seed_data.seed_date, require_synchronous);
  if (require_synchronous && !safe_seed_store_->IsInitialized()) {
    // Safe is only needed for the alias optimization. Keep the startup store
    // synchronous without initializing Safe solely to compare the payloads.
    std::move(store_validated_seed_cb)
        .Run(SeedReaderWriter::ReadSeedDataResult{LoadSeedResult::kEmpty, "",
                                                  ""});
    return;
  }
  ReadSeedData(std::move(store_validated_seed_cb), SeedType::SAFE,
               require_synchronous);
}

void VariationsSeedStore::ProcessAndStoreSeedData(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    SeedData seed_data,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult read_result) {
  if (read_result.result != LoadSeedResult::kSuccess) {
    RecordStoreSeedResult(StoreSeedResult::kFailedDeltaReadSeed);
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }
  seed_data.existing_seed_bytes = std::move(read_result.seed_data);
  const bool legacy_key_distrusted =
      local_state_->GetBoolean(prefs::kVariationsLegacySeedKeyDistrusted);
  if (require_synchronous) {
    SeedProcessingResult result =
        ProcessSeedData(signature_verification_enabled_on_receive_,
                        legacy_key_distrusted, std::move(seed_data));
    OnSeedDataProcessed(std::move(done_callback), require_synchronous,
                        std::move(result));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&VariationsSeedStore::ProcessSeedData,
                       signature_verification_enabled_on_receive_,
                       legacy_key_distrusted, std::move(seed_data)),
        base::BindOnce(&VariationsSeedStore::OnSeedDataProcessed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                       require_synchronous));
  }
}

void VariationsSeedStore::StoreValidatedSeed(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    ValidatedSeed seed,
    std::string country_code,
    std::string geo_level1,
    base::Time seed_date,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult safe_seed_read_result) {
#if BUILDFLAG(IS_ANDROID)
  // If currently we do not have any stored seed, then we mark seed storing as
  // successful on the Java side to avoid repeated seed fetches.
  if (use_first_run_prefs_ && seed_reader_writer_->IsEmpty()) {
    android::MarkVariationsSeedAsStored();
  }
#endif  // BUILDFLAG(IS_ANDROID)

  int milestone = version_info::GetMajorVersionNumberAsInt();

  LogSeedDayChange(seed_date);

  // As a space optimization, store an alias to the safe seed if both the
  // contents and the signature are identical. Comparing signatures prevents
  // aliasing a PQ-signed latest seed to a legacy-signed safe seed with the
  // same payload bytes.
  std::string_view seed_data;
  if (safe_seed_read_result.result == LoadSeedResult::kSuccess &&
      safe_seed_read_result.seed_data == seed.seed_data &&
      safe_seed_read_result.signature == seed.base64_seed_signature) {
    seed_data = kIdenticalToSafeSeedSentinel;
  } else {
    seed_data = seed.seed_data;
  }
  StoreSeedResult result =
      seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = seed.base64_seed_signature,
          .milestone = milestone,
          .seed_date = seed_date,
          .client_fetch_time = base::Time::Now(),
          .session_country_code = country_code,
          .session_geo_level1 = geo_level1,
      });
  if (result == StoreSeedResult::kSuccess) {
    // Stop trusting the legacy ECDSA P-256 key once a seed signed by one of
    // the pinned post-quantum hybrid keys (Key IDs 1, 2, or 3) is persisted.
    bool transitioned_to_distrust_legacy_key = false;
    if (signature_verification_enabled_on_receive_ &&
        IsHybridSeedSignature(seed.base64_seed_signature) &&
        !local_state_->GetBoolean(prefs::kVariationsLegacySeedKeyDistrusted)) {
      local_state_->SetBoolean(prefs::kVariationsLegacySeedKeyDistrusted, true);
      transitioned_to_distrust_legacy_key = true;
    }
    seed_purged_from_local_state_ = false;
    StoreLatestSerialNumber(seed.parsed.serial_number());
    if (require_synchronous || transitioned_to_distrust_legacy_key) {
      // Synchronous stores happen during startup (e.g. importing the initial
      // seed). Also flush immediately on the one-time transition to distrusting
      // the legacy key so a crash before the scheduled write cannot leave the
      // pref and SeedFile out of sync.
      seed_reader_writer_->CommitPendingWrite();
      if (transitioned_to_distrust_legacy_key) {
        local_state_->CommitPendingWrite();
      }
    }
  }
  RecordStoreSeedResult(result);
  std::move(done_callback).Run(true, std::move(seed.parsed));
}

void VariationsSeedStore::StoreValidatedSafeSeed(
    base::OnceCallback<void(StoreSeedResult)> done_callback,
    ValidatedSeed seed,
    int seed_milestone,
    base::Time reference_date,
    std::string session_consistency_country,
    std::string permanent_consistency_country,
    std::string locale,
    base::Time seed_fetch_time,
    SeedReaderWriter::ReadSeedDataResult safe_seed_read_result,
    SeedReaderWriter::ReadSeedDataResult latest_seed_read_result) {
  // Before updating the safe seed, update the latest seed if the latest
  // seed's value is |kIdenticalToSafeSeedSentinel|.
  //
  // It's theoretically possible for the client to be in the following state:
  // 1. The client has safe seed A.
  // 2. The client is applying seed B. In other words, seed B was the latest
  //    seed when Chrome was started.
  // 3. The client has just successfully fetched a new latest seed that
  //    happens to be seed A—perhaps due to a rollback. In this case,
  //    |kIdenticalToSafeSeedSentinel| is stored as the latest seed value to
  //    avoid duplicating seed A in storage.
  // 4. The client is promoting seed B to safe seed.
  const SeedInfo latest_seed_info = seed_reader_writer_->GetSeedInfo();
  if (safe_seed_read_result.result == LoadSeedResult::kSuccess &&
      (safe_seed_read_result.seed_data != seed.seed_data ||
       safe_seed_read_result.signature != seed.base64_seed_signature) &&
      seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    StoreSeedResult store_result =
        seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
            .seed_data = safe_seed_read_result.seed_data,
            .signature = latest_seed_info.signature,
            .milestone = latest_seed_info.milestone,
            .seed_date = latest_seed_info.seed_date,
            .client_fetch_time = latest_seed_info.client_fetch_time,
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }
  }

  {
    StoreSeedResult store_result =
        safe_seed_store_->SetCompressedSeed(ValidatedSeedInfo{
            .seed_data = seed.seed_data,
            .signature = seed.base64_seed_signature,
            .milestone = seed_milestone,
            .seed_date = reference_date,
            .client_fetch_time = seed_fetch_time,
            .session_country_code = session_consistency_country,
            .permanent_country_code = permanent_consistency_country,
            // The permanent version is not stored in the safe seed, only the
            // country.
            .permanent_country_version = "",
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }
  }
  safe_seed_store_->SetLocale(locale);

  // As a space optimization, overwrite the stored latest seed data with an
  // alias to the safe seed, if both the contents and signature are identical.
  if (latest_seed_read_result.result == LoadSeedResult::kSuccess &&
      latest_seed_read_result.seed_data == seed.seed_data &&
      latest_seed_read_result.signature == seed.base64_seed_signature) {
    StoreSeedResult store_result =
        seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
            .seed_data = kIdenticalToSafeSeedSentinel,
            .signature = latest_seed_info.signature,
            .milestone = latest_seed_info.milestone,
            .seed_date = latest_seed_info.seed_date,
            .client_fetch_time = latest_seed_info.client_fetch_time,
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }

    // Moreover, in this case, the last fetch time for the safe seed should
    // match the latest seed's.
    safe_seed_store_->SetFetchTime(latest_seed_info.client_fetch_time);
  }
  std::move(done_callback).Run(StoreSeedResult::kSuccess);
}

// static
VariationsSeedStore::SeedProcessingResult VariationsSeedStore::ProcessSeedData(
    bool signature_verification_enabled,
    bool legacy_key_distrusted,
    SeedData seed_data) {
  const std::string* data = &seed_data.data;

  // If the data is gzip compressed, first uncompress it.
  std::string ungzipped_data;
  if (seed_data.is_gzip_compressed) {
    StoreSeedResult result = Uncompress(*data, &ungzipped_data);
    if (result != StoreSeedResult::kSuccess) {
      return {std::move(seed_data), result};
    }
    data = &ungzipped_data;
  }

  // If the data is delta-compressed, apply the delta patch.
  std::string patched_data;
  if (seed_data.is_delta_compressed) {
    DCHECK(!seed_data.existing_seed_bytes.empty());
    if (!ApplyDeltaPatch(seed_data.existing_seed_bytes, *data, &patched_data)) {
      return {std::move(seed_data), StoreSeedResult::kFailedDeltaApply};
    }
    data = &patched_data;
  }

  ValidatedSeed validated;
  auto validate_result = VariationsSeedStore::ValidateSeedBytes(
      *data, seed_data.base64_seed_signature,
      VariationsSeedStore::SeedType::LATEST, signature_verification_enabled,
      legacy_key_distrusted, &validated);
  if (validate_result == StoreSeedResult::kSuccess) {
    MaybeDumpIfSeedSizeApproachingLimit(validated.seed_data.size());
  }
  // Important, this must come after the above call as `data` can point to a
  // member of `seed_data` which is being moved.
  SeedProcessingResult result(std::move(seed_data), StoreSeedResult::kSuccess);
  result.validate_result = validate_result;
  result.validated = std::move(validated);
  return result;
}

// static
StoreSeedResult VariationsSeedStore::ValidateSeedBytes(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature,
    SeedType seed_type,
    bool signature_verification_enabled,
    bool legacy_key_distrusted,
    ValidatedSeed* result) {
  DCHECK(result);
  if (seed_bytes.empty()) {
    return StoreSeedResult::kFailedEmptyGzipContents;
  }

  // Only store the seed data if it parses correctly.
  VariationsSeed seed;
  if (!seed.ParseFromString(seed_bytes)) {
    return StoreSeedResult::kFailedParse;
  }

  // TODO(crbug.com/40228403): get rid of |signature_verification_enabled| and
  // only support switches::kAcceptEmptySeedSignatureForTesting.
  if (signature_verification_enabled &&
      !AcceptEmptySeedSignatureForTesting(base64_seed_signature)) {
    const VerifySignatureResult verify_result = VerifySeedSignature(
        seed_bytes, base64_seed_signature, legacy_key_distrusted);
    switch (seed_type) {
      case SeedType::LATEST:
        base::UmaHistogramEnumeration("Variations.StoreSeedSignature",
                                      verify_result);
        break;
      case SeedType::SAFE:
        base::UmaHistogramEnumeration(
            "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
            verify_result);
        break;
    }

    if (verify_result != VerifySignatureResult::kValidSignature) {
      return StoreSeedResult::kFailedSignature;
    }
  }

  result->seed_data = seed_bytes;
  result->base64_seed_signature = base64_seed_signature;
  result->parsed.Swap(&seed);
  return StoreSeedResult::kSuccess;
}

// static
bool VariationsSeedStore::ApplyDeltaPatch(const std::string& existing_data,
                                          const std::string& patch,
                                          std::string* output) {
  output->clear();

  google::protobuf::io::CodedInputStream in(
      reinterpret_cast<const uint8_t*>(patch.data()), patch.length());
  // Temporary string declared outside the loop so it can be re-used between
  // different iterations (rather than allocating new ones).
  std::string temp;

  const uint32_t existing_data_size =
      static_cast<uint32_t>(existing_data.size());
  while (in.CurrentPosition() != static_cast<int>(patch.length())) {
    uint32_t value;
    if (!in.ReadVarint32(&value)) {
      return false;
    }

    if (value != 0) {
      // A non-zero value indicates the number of bytes to copy from the patch
      // stream to the output.

      // No need to guard against bad data (i.e. very large |value|) because the
      // call below will fail if |value| is greater than the size of the patch.
      if (!in.ReadString(&temp, value)) {
        return false;
      }
      output->append(temp);
    } else {
      // Otherwise, when it's zero, it indicates that it's followed by a pair of
      // numbers - |offset| and |length| that specify a range of data to copy
      // from |existing_data|.
      uint32_t offset;
      uint32_t length;
      if (!in.ReadVarint32(&offset) || !in.ReadVarint32(&length)) {
        return false;
      }

      // Check for |offset + length| being out of range and for overflow.
      base::CheckedNumeric<uint32_t> end_offset(offset);
      end_offset += length;
      if (!end_offset.IsValid() ||
          end_offset.ValueOrDie() > existing_data_size) {
        return false;
      }
      output->append(existing_data, offset, length);
    }
  }
  return true;
}

void VariationsSeedStore::AllowToPurgeSeedsDataFromMemory() {
  seed_reader_writer_->AllowToPurgeSeedDataFromMemory();
  safe_seed_store_->AllowToPurgeSeedDataFromMemory();
}

void VariationsSeedStore::PurgeSeedFromLocalStatePrefs() {
  seed_purged_from_local_state_ = true;
  local_state_->ClearPref(prefs::kVariationsCompressedSeed);
  local_state_->ClearPref(prefs::kVariationsSeedSignature);
}

void VariationsSeedStore::CommitPendingWrite() {
  seed_reader_writer_->CommitPendingWrite();
  safe_seed_store_->CommitPendingWrite();
}

void VariationsSeedStore::GetStoredSeedInfoForDebugging(
    base::OnceCallback<void(StoredSeedInfo)> done_callback,
    SeedType seed_type) {
  if (seed_type == SeedType::LATEST) {
    seed_reader_writer_->GetStoredSeedInfoForDebugging(
        std::move(done_callback));
  } else {
    safe_seed_store_->GetStoredSeedInfoForDebugging(std::move(done_callback));
  }
}

}  // namespace variations
