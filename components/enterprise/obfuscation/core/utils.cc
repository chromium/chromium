// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/utils.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

namespace enterprise_obfuscation {

namespace {

// Generates a random 256 bit AES key.
const std::vector<uint8_t>& GetSymmetricKey() {
  static const base::NoDestructor<std::vector<uint8_t>> kSymmetricKey(
      crypto::RandBytesAsVector(kKeySize));

  return *kSymmetricKey;
}

// Computes nonce. The structure is: noncePrefix | counter (4 bytes) | b (1
// byte). The last byte is to ensure that the ciphertext is different for the
// last chunk of a file.
const std::vector<uint8_t> ComputeNonce(
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk) {
  std::array<uint8_t, sizeof(uint32_t)> encoded_counter =
      base::U32ToBigEndian(counter);

  std::vector<uint8_t> nonce;
  nonce.reserve(kNonceSize);
  nonce.insert(nonce.end(), nonce_prefix.begin(), nonce_prefix.end());
  nonce.insert(nonce.end(), encoded_counter.begin(), encoded_counter.end());
  nonce.push_back(is_last_chunk ? 0x01 : 0x00);

  return nonce;
}

}  // namespace

BASE_FEATURE(kEnterpriseFileObfuscation,
             "EnterpriseFileObfuscation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFileObfuscationEnabled() {
  return base::FeatureList::IsEnabled(kEnterpriseFileObfuscation);
}

base::expected<std::vector<uint8_t>, Error> CreateHeader(
    std::vector<uint8_t>* derived_key,
    std::vector<uint8_t>* nonce_prefix) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }

  if (!derived_key || !nonce_prefix) {
    return base::unexpected(Error::kObfuscationFailed);
  }

  // Create header and allocate space.
  std::vector<uint8_t> header;
  header.reserve(kHeaderSize);
  header.push_back(kHeaderSize);

  // Generate salt.
  std::vector<uint8_t> salt = crypto::RandBytesAsVector(kSaltSize);
  header.insert(header.end(), salt.begin(), salt.end());

  // Generate file-specific key.
  *derived_key = crypto::HkdfSha256(GetSymmetricKey(), salt,
                                    base::span<uint8_t>(), kKeySize);

  // Generate nonce prefix.
  *nonce_prefix = crypto::RandBytesAsVector(kNoncePrefixSize);
  header.insert(header.end(), nonce_prefix->begin(), nonce_prefix->end());

  if (header.size() != kHeaderSize) {
    return base::unexpected(Error::kObfuscationFailed);
  }

  return base::ok(std::move(header));
}

base::expected<std::vector<uint8_t>, Error> ObfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& derived_key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(derived_key);

  // Compute nonce.
  if (aead.NonceLength() != kNonceSize) {
    return base::unexpected(Error::kSchemeError);
  }
  std::vector<uint8_t> nonce =
      ComputeNonce(nonce_prefix, counter, is_last_chunk);

  // Encrypt the data and prepend the encrypted size.
  std::vector<uint8_t> encrypted_data =
      aead.Seal(data, nonce, base::span<uint8_t>());

  std::array<uint8_t, kChunkSizePrefixSize> size =
      base::U32ToBigEndian(static_cast<uint32_t>(encrypted_data.size()));
  encrypted_data.insert(encrypted_data.begin(), size.begin(), size.end());

  return base::ok(std::move(encrypted_data));
}

base::expected<size_t, Error> GetObfuscatedChunkSize(
    base::span<const uint8_t> data) {
  if (data.size() < kChunkSizePrefixSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  std::array<uint8_t, kChunkSizePrefixSize> size;
  std::copy_n(data.begin(), kChunkSizePrefixSize, size.begin());
  size_t chunk_size = base::U32FromBigEndian(size);
  if (chunk_size > kMaxChunkSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }
  return base::ok(chunk_size);
}

base::expected<std::pair</*derived key*/ std::vector<uint8_t>,
                         /*nonce prefix*/ std::vector<uint8_t>>,
               Error>
GetHeaderData(const std::vector<uint8_t>& header) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }

  if (header.size() != kHeaderSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  const uint8_t stored_header_size = header[0];
  if (stored_header_size != header.size()) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  // Extract salt.
  base::span<const uint8_t> salt = base::span(header).subspan(1, kSaltSize);

  // Extract nonce_prefix.
  std::vector<uint8_t> nonce_prefix(header.begin() + 1 + kSaltSize,
                                    header.end());

  // Generate file-specific key.
  std::vector<uint8_t> derived_key = crypto::HkdfSha256(
      GetSymmetricKey(), salt, base::span<uint8_t>(), kKeySize);

  return base::ok(std::pair(std::move(derived_key), std::move(nonce_prefix)));
}

base::expected<std::vector<uint8_t>, Error> DeobfuscateDataChunk(
    base::span<const uint8_t> data,
    const std::vector<uint8_t>& derived_key,
    const std::vector<uint8_t>& nonce_prefix,
    uint32_t counter,
    bool is_last_chunk) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(derived_key);

  if (data.size() < kAuthTagSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  // Construct nonce.
  if (aead.NonceLength() != kNonceSize) {
    return base::unexpected(Error::kSchemeError);
  }
  std::vector<uint8_t> nonce =
      ComputeNonce(nonce_prefix, counter, is_last_chunk);

  auto plaintext = aead.Open(data, nonce, base::span<uint8_t>());
  if (!plaintext) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }
  return base::ok(std::move(plaintext.value()));
}

base::expected<void, Error> DeobfuscateFileInPlace(
    const base::FilePath& file_path) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }

  // Open the obfuscated file in read-only mode.
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid() || file.GetLength() == 0) {
    return base::unexpected(Error::kFileOperationError);
  }

  // Create and open a temporary file for deobfuscation.
  base::ScopedTempFile temp_file;
  if (!temp_file.Create()) {
    return base::unexpected(Error::kFileOperationError);
  }
  base::File deobfuscated_file(temp_file.path(),
                               base::File::FLAG_OPEN | base::File::FLAG_APPEND);

  // Get header data
  if (static_cast<size_t>(file.GetLength()) < kHeaderSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }
  std::vector<uint8_t> header(kHeaderSize);
  std::optional<size_t> header_read = file.ReadAtCurrentPos(header);
  if (!header_read || header_read != kHeaderSize) {
    return base::unexpected(Error::kFileOperationError);
  }
  auto header_data = GetHeaderData(header);
  if (!header_data.has_value()) {
    return base::unexpected(header_data.error());
  }

  // Initialize cipher.
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(header_data.value().first);
  if (aead.NonceLength() != kNonceSize) {
    return base::unexpected(Error::kSchemeError);
  }
  uint32_t counter = 0;
  size_t total_bytes_read = header_read.value();

  int64_t file_length = file.GetLength();
  if (file_length < 0) {
    return base::unexpected(Error::kFileOperationError);
  }
  const size_t file_size = static_cast<size_t>(file_length);

  // Deobfuscate to temporary file.
  while (total_bytes_read < file_size) {
    // Get the size of the next obfuscated chunk.
    std::array<uint8_t, kChunkSizePrefixSize> size;
    std::optional<size_t> size_read = file.ReadAtCurrentPos(size);
    if (!size_read || size_read.value() != kChunkSizePrefixSize) {
      return base::unexpected(Error::kFileOperationError);
    }

    auto chunk_size = GetObfuscatedChunkSize(size);
    if (!chunk_size.has_value()) {
      return base::unexpected(chunk_size.error());
    }
    total_bytes_read += kChunkSizePrefixSize;

    // Read in obfuscated chunk.
    std::vector<uint8_t> ciphertext(chunk_size.value());
    std::optional<size_t> bytes_read = file.ReadAtCurrentPos(ciphertext);
    if (!bytes_read) {
      return base::unexpected(Error::kFileOperationError);
    }
    if (bytes_read.value() != chunk_size) {
      return base::unexpected(Error::kDeobfuscationFailed);
    }

    total_bytes_read += bytes_read.value();

    std::vector<uint8_t> nonce = ComputeNonce(
        header_data.value().second, counter++, total_bytes_read == file_size);

    auto plaintext = aead.Open(ciphertext, nonce, base::span<uint8_t>());
    if (!plaintext) {
      return base::unexpected(Error::kDeobfuscationFailed);
    }
    deobfuscated_file.WriteAtCurrentPos(plaintext.value());
  }
  file.Close();
  deobfuscated_file.Close();

  // If deobfuscation is successful, replace the original file.
  if (!base::ReplaceFile(temp_file.path(), file_path, /*error=*/nullptr)) {
    return base::unexpected(Error::kFileOperationError);
  }
  return base::ok();
}

}  // namespace enterprise_obfuscation
