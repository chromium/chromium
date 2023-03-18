// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace os_crypt_async {

class EncryptorTestBase;
class OSCryptAsync;

// This class is used for data encryption. A thread-safe instance can be
// obtained by calling `os_crypt_async::OSCryptAsync::GetInstance`.
class Encryptor {
 public:
  ~Encryptor();

  // Moveable, not copyable.
  Encryptor(Encryptor&& other);
  Encryptor& operator=(Encryptor&& other);
  Encryptor(const Encryptor&) = delete;
  Encryptor& operator=(const Encryptor&) = delete;

  // Encrypt a string with the current Encryptor configuration. This can be
  // called on any thread.
  [[nodiscard]] absl::optional<std::vector<uint8_t>> EncryptString(
      const std::string& data) const;

  // Decrypt data previously encrypted using `EncryptData`. This can be called
  // on any thread.
  [[nodiscard]] absl::optional<std::string> DecryptData(
      base::span<const uint8_t> data) const;

  // These two APIs are provided for backwards compatibility with OSCrypt. They
  // just call the above functions. The two sets of functions are compatible
  // with each other.
  [[nodiscard]] bool EncryptString(const std::string& plaintext,
                                   std::string* ciphertext) const;
  [[nodiscard]] bool DecryptString(const std::string& ciphertext,
                                   std::string* plaintext) const;

 private:
  friend class EncryptorTestBase;
  friend class OSCryptAsync;

  // Used for cloning and creation of the template instance.
  Encryptor();

  // Clone is used by the factory to vend instances.
  Encryptor Clone() const;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_
