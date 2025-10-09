// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_POSIX_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_POSIX_KEY_PROVIDER_H_

#include "components/os_crypt/async/browser/key_provider.h"

namespace os_crypt_async {

class PosixKeyProvider : public KeyProvider {
 public:
  PosixKeyProvider();

  PosixKeyProvider(const PosixKeyProvider&) = delete;
  PosixKeyProvider& operator=(const PosixKeyProvider&) = delete;

  ~PosixKeyProvider() override;

  // KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_POSIX_KEY_PROVIDER_H_
