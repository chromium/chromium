// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/utils.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/no_destructor.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

namespace enterprise_obfuscation {

namespace {

// Default key size, nonce length and max tag length in BoringSSL's
// implementation of AES-256 GCM used by the crypto library.
// TODO(b/356473947): Consider switching to 128-bit key for performance.
static constexpr size_t kKeySize = 32u;
static constexpr size_t kNonceSize = 12u;
static constexpr size_t kAuthTagSize = 16u;

// Maximum size of a data chunk for obfuscation/deobfuscation.
//
// This size is chosen to be the default buffer size in bytes used for downloads
// (kDefaultDownloadFileBufferSize = 524288) plus the nonce and auth tag
// lengths.
static constexpr size_t kChunkSize = kNonceSize + 512 * 1024 + kAuthTagSize;

// Generates a random 256 bit AES key.
const std::vector<uint8_t>& GetSymmetricKey() {
  static const base::NoDestructor<std::vector<uint8_t>> kSymmetricKey([]() {
    std::vector<uint8_t> key(kKeySize);
    crypto::RandBytes(key);
    return key;
  }());

  return *kSymmetricKey;
}

}  // namespace

BASE_FEATURE(kEnterpriseFileObfuscation,
             "EnterpriseFileObfuscation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFileObfuscationEnabled() {
  return base::FeatureList::IsEnabled(kEnterpriseFileObfuscation);
}

base::expected<std::vector<uint8_t>, Error> ObfuscateDataChunk(
    base::span<const uint8_t> data) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  const std::vector<uint8_t>& key = GetSymmetricKey();
  aead.Init(key);

  // Random nonce generation.
  CHECK_EQ(aead.NonceLength(), kNonceSize);
  uint8_t nonce[kNonceSize];
  crypto::RandBytes(nonce);

  // WARNING INSECURE:
  // This scheme doesn't use an additional data value. This means that chunks
  // can be reordered by an attacker, dropped from a file, and moved between
  // files. That is deemed acceptable in this context but may not be in others.
  // Please check with security before using this pattern elsewhere.
  std::vector<uint8_t> ciphertext =
      aead.Seal(data, nonce, base::span<uint8_t>());

  // Prepend nonce to output data.
  // TODO(b/356473947): Consider changing prepend operation for performance.
  ciphertext.insert(ciphertext.begin(), std::begin(nonce), std::end(nonce));
  return base::ok(std::move(ciphertext));
}

base::expected<std::vector<uint8_t>, Error> DeobfuscateDataChunk(
    base::span<const uint8_t> data) {
  if (!IsFileObfuscationEnabled()) {
    return base::unexpected(Error::kDisabled);
  }
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  const std::vector<uint8_t>& key = GetSymmetricKey();
  aead.Init(key);

  // Get nonce at the head of data.
  CHECK_EQ(aead.NonceLength(), kNonceSize);
  if (data.size() < kNonceSize + kAuthTagSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }
  base::span<const uint8_t> nonce = data.first(kNonceSize);

  auto plaintext =
      aead.Open(data.subspan(kNonceSize), nonce, base::span<uint8_t>());
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

  std::vector<uint8_t> ciphertext(kChunkSize);

  // Initialize cipher.
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  const std::vector<uint8_t>& key = GetSymmetricKey();
  aead.Init(key);
  CHECK_EQ(aead.NonceLength(), kNonceSize);

  // Deobfuscate to temporary file.
  while (true) {
    std::optional<size_t> bytes_read =
        file.ReadAtCurrentPos(base::make_span(ciphertext));
    if (!bytes_read) {
      return base::unexpected(Error::kFileOperationError);
    }

    if (bytes_read.value() == 0) {
      break;  // Reached end of file
    }

    // The size of the data being smaller than the nonce + auth tag means that
    // it wasn't obfuscated.
    if (bytes_read.value() < kNonceSize + kAuthTagSize) {
      return base::unexpected(Error::kDeobfuscationFailed);
    }

    // Resize ciphertext to the actual number of bytes read.
    ciphertext.resize(bytes_read.value());

    base::span<const uint8_t> nonce =
        base::make_span(ciphertext).first(kNonceSize);

    auto plaintext = aead.Open(base::make_span(ciphertext).subspan(kNonceSize),
                               nonce, base::span<uint8_t>());
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
