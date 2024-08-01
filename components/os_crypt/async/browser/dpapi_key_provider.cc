// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/os_crypt/async/browser/dpapi_key_provider.h"

#include <windows.h>

#include <wincrypt.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "base/win/scoped_localalloc.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/prefs/pref_service.h"

namespace os_crypt_async {

namespace {

// Legacy (OSCrypt) random key encrypted with DPAPI imported by this code.
// This should match the pref name defined in os_crypt_win.cc until sync is
// deprecated and the pref registration can be moved here.
constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";

// Data prefix for data encrypted with DPAPI. This must match
// kEncryptionVersionPrefix in os_crypt_win.cc to ensure data is compatible.
constexpr char kKeyTag[] = "v10";

// Key prefix for a key encrypted with DPAPI. This must match kDPAPIKeyPrefix in
// os_crypt_win.cc to ensure the same key can be decrypted successfully.
constexpr uint8_t kDPAPIKeyPrefix[] = {'D', 'P', 'A', 'P', 'I'};

std::optional<std::vector<uint8_t>> DecryptKeyWithDPAPI(
    base::span<const uint8_t> ciphertext) {
  DATA_BLOB input = {};
  input.pbData = const_cast<BYTE*>(ciphertext.data());
  input.cbData = static_cast<DWORD>(ciphertext.size());

  BOOL result = FALSE;
  DATA_BLOB output;
  result = ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                                &output);

  if (!result) {
    return std::nullopt;
  }

  auto local_alloc = base::win::TakeLocalAlloc(output.pbData);

  return std::vector<uint8_t>(local_alloc.get(),
                              local_alloc.get() + output.cbData);
}

}  // namespace

DPAPIKeyProvider::DPAPIKeyProvider(PrefService* local_state)
    : local_state_(local_state) {}
DPAPIKeyProvider::~DPAPIKeyProvider() = default;

base::expected<Encryptor::Key, DPAPIKeyProvider::KeyStatus>
DPAPIKeyProvider::GetKeyInternal() {
  if (!local_state_->HasPrefPath(kOsCryptEncryptedKeyPrefName)) {
    return base::unexpected(KeyStatus::kKeyNotFound);
  }

  const std::string base64_encrypted_key =
      local_state_->GetString(kOsCryptEncryptedKeyPrefName);

  std::optional<std::vector<uint8_t>> decoded =
      base::Base64Decode(base64_encrypted_key);

  if (!decoded) {
    return base::unexpected(KeyStatus::kKeyDecodeFailure);
  }

  if (decoded->size() < std::size(kDPAPIKeyPrefix)) {
    return base::unexpected(KeyStatus::kKeyTooShort);
  }

  if (!std::equal(decoded->begin(),
                  decoded->begin() + std::size(kDPAPIKeyPrefix),
                  kDPAPIKeyPrefix)) {
    return base::unexpected(KeyStatus::kInvalidKeyHeader);
  }

  auto encrypted_key_data = std::vector<uint8_t>(
      decoded->cbegin() + std::size(kDPAPIKeyPrefix), decoded->cend());

  auto decrypted_key = DecryptKeyWithDPAPI(encrypted_key_data);

  if (!decrypted_key) {
    return base::unexpected(KeyStatus::kDPAPIDecryptFailure);
  }

  if (decrypted_key->size() != Encryptor::Key::kAES256GCMKeySize) {
    return base::unexpected(KeyStatus::kInvalidKeyLength);
  }

  return Encryptor::Key(*decrypted_key, mojom::Algorithm::kAES256GCM);
}

void DPAPIKeyProvider::GetKey(KeyCallback callback) {
  auto result = GetKeyInternal();

  base::UmaHistogramEnumeration("OSCrypt.DPAPIProvider.Status",
                                result.error_or(KeyStatus::kSuccess));

  if (result.has_value()) {
    std::move(callback).Run(kKeyTag, std::move(result.value()));
  } else {
    std::move(callback).Run(std::string(), std::nullopt);
  }
}

bool DPAPIKeyProvider::UseForEncryption() {
  return true;
}

bool DPAPIKeyProvider::IsCompatibleWithOsCryptSync() {
  return true;
}

}  // namespace os_crypt_async
