// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_

#include "base/feature_list.h"
#include "base/types/expected.h"

namespace enterprise_obfuscation {

// Feature to enable insecure obfuscation and deobfuscation of files sent to
// WebProtect deep scanning service for enterprise users.
BASE_DECLARE_FEATURE(kEnterpriseFileObfuscation);

// Returns true if `kEnterpriseFileObfuscation` feature is enabled.
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
base::expected<std::vector<uint8_t>, Error> CreateHeader(
    std::vector<uint8_t>* derived_key,
    std::vector<uint8_t>* nonce_prefix);

// Obfuscate data chunk using crypto::Aead
// (https://crsrc.org/c/crypto/aead.h) in an insecure way to act as a file
// access deterrent. Master key is stored in memory and can be leaked.
// Counter increments every chunk to protect against reordering/truncation.
// TODO(b/351151997): Change to add padding and support for data chunks of
// variable size.
base::expected<std::vector<uint8_t>, Error> ObfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk);

// Computes the derived key and extracts the nonce prefix from the header.
base::expected<std::pair</*derived key*/ std::vector<uint8_t>,
                         /*nonce prefix*/ std::vector<uint8_t>>,
               Error>
GetHeaderData(const std::vector<uint8_t>& header);

// Deobfuscate data chunk using crypto::Aead (https://crsrc.org/c/crypto/aead.h)
// in an insecure way to act as a file access deterrent. Master key is stored in
// memory and can be leaked. Counter increments every chunk to protect against
// reordering/truncation.
base::expected<std::vector<uint8_t>, Error> DeobfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk);

// Insecurely deobfuscate a file by replacing the original file with the
// deobfuscated data. Master key is stored in memory and can be leaked.
base::expected<void, Error> DeobfuscateFileInPlace(
    const base::FilePath& file_path);

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_
