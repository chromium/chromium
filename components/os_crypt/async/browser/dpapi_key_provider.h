// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_DPAPI_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_DPAPI_KEY_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/os_crypt/async/browser/key_provider.h"

class PrefService;

namespace os_crypt_async {

class DPAPIKeyProviderTest;

// The DPAPI Key Provider provides forwards and backwards compatibility with
// OSCrypt's in-built DPAPI support on Windows. The Key provider will use the
// key from OSCrypt's storage which is encrypted with DPAPI. OSCrypt::Init
// should always be called before attempting to obtain a key from this provider.
class DPAPIKeyProvider : public KeyProvider {
 public:
  explicit DPAPIKeyProvider(PrefService* local_state);
  ~DPAPIKeyProvider() override;

 private:
  friend class DPAPIKeyProviderTestBase;
  FRIEND_TEST_ALL_PREFIXES(DPAPIKeyProviderTest, OSCryptNotInit);
  FRIEND_TEST_ALL_PREFIXES(DPAPIKeyProviderTest, OSCryptBadKeyHeader);
  FRIEND_TEST_ALL_PREFIXES(DPAPIKeyProviderTestBase, NoOSCrypt);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class KeyStatus {
    kSuccess = 0,
    kKeyNotFound = 1,
    kKeyDecodeFailure = 2,
    kKeyTooShort = 3,
    kInvalidKeyHeader = 4,
    kDPAPIDecryptFailure = 5,
    kInvalidKeyLength = 6,
    kMaxValue = kInvalidKeyLength,
  };

  // os_crypt_async::KeyProvider interface.
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

  // Attempt to retrieve `encrypted_key` from `pref_path`. If a key is found
  // that matches and has correct `key_prefix` then the raw encrypted key is
  // returned without the `key_prefix`.
  std::optional<std::vector<uint8_t>> RetrieveEncryptedKey(
      const std::string& pref_path,
      base::span<const uint8_t> key_prefix);

  base::expected<Encryptor::Key, KeyStatus> GetKeyInternal();

  raw_ptr<PrefService> local_state_;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_DPAPI_KEY_PROVIDER_H_
