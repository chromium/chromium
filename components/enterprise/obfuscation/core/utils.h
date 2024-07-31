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
};

// Obfuscate data chunk using crypto::Aead (https://crsrc.org/c/crypto/aead.h)
// in an insecure way to act as a file access deterrent. Key is stored in memory
// and can be leaked.
// TODO(b/351151997): Change to add padding and support for data chunks of
// variable size.
base::expected<std::vector<uint8_t>, Error> ObfuscateDataChunk(
    base::span<const uint8_t> data);

// Deobfuscate data chunk using crypto::Aead (https://crsrc.org/c/crypto/aead.h)
// in an insecure way to act as a file access deterrent. Key is stored in memory
// and can be leaked.
base::expected<std::vector<uint8_t>, Error> DeobfuscateDataChunk(
    base::span<const uint8_t> data);

// Insecurely deobfuscate a file by replacing the original file with the
// deobfuscated data. Key is stored in memory and can be leaked.
base::expected<void, Error> DeobfuscateFileInPlace(
    const base::FilePath& file_path);

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_UTILS_H_
