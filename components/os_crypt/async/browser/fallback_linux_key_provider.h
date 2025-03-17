// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FALLBACK_LINUX_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FALLBACK_LINUX_KEY_PROVIDER_H_

#include "components/os_crypt/async/browser/key_provider.h"

namespace os_crypt_async {

class FallbackLinuxKeyProvider : public KeyProvider {
 public:
  explicit FallbackLinuxKeyProvider(bool use_for_encryption);

  FallbackLinuxKeyProvider(const FallbackLinuxKeyProvider&) = delete;
  FallbackLinuxKeyProvider& operator=(const FallbackLinuxKeyProvider&) = delete;

  ~FallbackLinuxKeyProvider() override;

  // KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

 private:
  const bool use_for_encryption_;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FALLBACK_LINUX_KEY_PROVIDER_H_
