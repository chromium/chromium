// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/types/expected.h"

namespace enterprise_obfuscation {

// Default key and derived key size, nonce length and max tag length in
// BoringSSL's implementation of AES-256 GCM used by the crypto library.
// TODO(b/356473947): Consider switching to 128-bit key for performance.
static constexpr size_t kKeySize = 32u;
static constexpr size_t kNonceSize = 12u;
static constexpr size_t kAuthTagSize = 16u;

// Nonce prefix and header size based on Tink streaming AEAD implementation
// (https://developers.google.com/tink/streaming-aead/aes_gcm_hkdf_streaming).
static constexpr size_t kNoncePrefixSize = 7u;
static constexpr size_t kSaltSize = kKeySize;
static constexpr size_t kHeaderSize = 1u + kSaltSize + kNoncePrefixSize;

// Maximum size of a data chunk for obfuscation/deobfuscation.
//
// This size is chosen to be the default buffer size in bytes used for downloads
// (kDefaultDownloadFileBufferSize = 524288) plus the auth tag length.
static constexpr size_t kMaxChunkSize = 512 * 1024 + kAuthTagSize;

// Size of the chunk size prefix for variable size.
static constexpr size_t kChunkSizePrefixSize = 4u;

// Feature to enable insecure obfuscation and deobfuscation of files sent to
// WebProtect deep scanning service for enterprise users.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
BASE_DECLARE_FEATURE(kEnterpriseFileObfuscation);

// Returns true if `kEnterpriseFileObfuscation` feature is enabled.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
bool IsFileObfuscationEnabled();

// Error types for insecure obfuscation and deobfuscation operations.
enum class Error {
  kObfuscationFailed,    // Obfuscation process could not be completed
  kDeobfuscationFailed,  // Deobfuscation process could not be completed
  kFileOperationError,   // Error during file read/write operations
  kDisabled,             // Obfuscation/deobfuscation is not enabled
  kSchemeError,          // Error with obfuscation scheme.
};

// Returns the header and populates the derived key and nonce prefix values used
// for obfuscating each chunk.
// The header structure is: size of header (1 byte) | salt | noncePrefix.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<std::vector<uint8_t>, Error> CreateHeader(
    std::vector<uint8_t>* derived_key,
    std::vector<uint8_t>* nonce_prefix);

// Obfuscate data chunk using crypto::Aead
// (https://crsrc.org/c/crypto/aead.h) in an insecure way to act as a file
// access deterrent. Master key is stored in memory and can be leaked.
// Counter increments every chunk to protect against reordering/truncation.
// The size of the encrypted chunk is prepended to the returned encrypted chunk.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<std::vector<uint8_t>, Error> ObfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk);

// Extracts the size of the obfuscated data chunk from the beginning of the
// provided data.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<size_t, Error> GetObfuscatedChunkSize(
    base::span<const uint8_t> data);

// Computes the derived key and extracts the nonce prefix from the header.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<std::pair</*derived key*/ std::vector<uint8_t>,
                         /*nonce prefix*/ std::vector<uint8_t>>,
               Error>
GetHeaderData(const std::vector<uint8_t>& header);

// Deobfuscate data chunk using crypto::Aead (https://crsrc.org/c/crypto/aead.h)
// in an insecure way to act as a file access deterrent. Master key is stored in
// memory and can be leaked. Counter increments every chunk to protect against
// reordering/truncation.
// The size of the encrypted chunk is expected to be prepended to the input
// data.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<std::vector<uint8_t>, Error> DeobfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk);

// Insecurely deobfuscate a file by replacing the original file with the
// deobfuscated data. Master key is stored in memory and can be leaked.
COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION)
base::expected<void, Error> DeobfuscateFileInPlace(
    const base::FilePath& file_path);

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_
