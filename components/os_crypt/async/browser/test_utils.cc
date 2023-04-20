// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/test_utils.h"

#include <memory>

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "crypto/random.h"

namespace os_crypt_async {

class TestOSCryptAsync : public OSCryptAsync {
 public:
  TestOSCryptAsync()
      : OSCryptAsync(
            std::vector<std::pair<Precedence, std::unique_ptr<KeyProvider>>>()),
        encryptor_(GetTestEncryptorForTesting()) {}

  [[nodiscard]] base::CallbackListSubscription GetInstance(
      InitCallback callback) override {
    std::move(callback).Run(encryptor_.Clone(), true);
    return base::CallbackListSubscription();
  }

  static Encryptor GetTestEncryptorForTesting() {
    Encryptor::KeyRing keys;
    std::vector<uint8_t> key(Encryptor::Key::kAES256GCMKeySize);
    crypto::RandBytes(key);
    keys.emplace("_", Encryptor::Key(key, mojom::Algorithm::kAES256GCM));
    Encryptor encryptor(std::move(keys), "_");
    return encryptor;
  }

 private:
  Encryptor encryptor_;
};

std::unique_ptr<OSCryptAsync> GetTestOSCryptAsyncForTesting() {
  return std::make_unique<TestOSCryptAsync>();
}

Encryptor GetTestEncryptorForTesting() {
  return TestOSCryptAsync::GetTestEncryptorForTesting();
}

}  // namespace os_crypt_async
