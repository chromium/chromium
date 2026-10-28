// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_KEY_PROVIDER_H_

#include <optional>
#include <string>

#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

// A test key provider that takes a name and produces a deterministic key based
// on that name, or uses a provided fixed key.
class TestKeyProvider : public KeyProvider {
 public:
  TestKeyProvider(const std::string& name,
                  bool use_for_encryption,
                  std::optional<Encryptor::Key> key = std::nullopt);
  ~TestKeyProvider() override;

 protected:
  void GetKey(KeyCallback callback) override;
  const std::string& name() const { return name_; }

 private:
  bool UseForEncryption() override;

  const std::string name_;
  const bool use_for_encryption_;
  const Encryptor::Key key_;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_KEY_PROVIDER_H_
