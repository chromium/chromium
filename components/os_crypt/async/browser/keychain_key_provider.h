// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEYCHAIN_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEYCHAIN_KEY_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/os_crypt/async/browser/key_provider.h"

namespace crypto::apple {
class Keychain;
}

namespace os_crypt_async {

// Provides an encryption key derived from a password stored in the macOS/iOS
// Keychain. This is compatible with the synchronous OSCrypt implementation.
class COMPONENT_EXPORT(OS_CRYPT_ASYNC) KeychainKeyProvider
    : public KeyProvider {
 public:
  KeychainKeyProvider();
  KeychainKeyProvider(const KeychainKeyProvider&) = delete;
  KeychainKeyProvider& operator=(const KeychainKeyProvider&) = delete;
  ~KeychainKeyProvider() override;

 private:
  friend class KeychainKeyProviderCompatTest;
  FRIEND_TEST_ALL_PREFIXES(KeychainKeyProviderTest, GetKey_Success);
  FRIEND_TEST_ALL_PREFIXES(KeychainKeyProviderTest, GetKey_NotFound);
  FRIEND_TEST_ALL_PREFIXES(KeychainKeyProviderTest, GetKey_Failure_AuthFailed);
  FRIEND_TEST_ALL_PREFIXES(KeychainKeyProviderTest, GetKey_Failure_OtherError);

  // For testing.
  explicit KeychainKeyProvider(crypto::apple::Keychain* keychain);

  // os_crypt_async::KeyProvider interface.
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

  raw_ptr<crypto::apple::Keychain> keychain_for_testing_ = nullptr;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEYCHAIN_KEY_PROVIDER_H_
