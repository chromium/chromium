// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/test_utils.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "crypto/random.h"

namespace os_crypt_async {

class TestOSCryptAsync : public OSCryptAsync {
 public:
  explicit TestOSCryptAsync(bool is_sync_for_unittests)
      : OSCryptAsync(
            std::vector<std::pair<Precedence, std::unique_ptr<KeyProvider>>>()),
        encryptor_(GetTestEncryptorForTesting()),
        is_sync_for_unittests_(is_sync_for_unittests) {}

  [[nodiscard]] base::CallbackListSubscription GetInstance(
      InitCallback callback,
      Encryptor::Option option) override {
    if (is_sync_for_unittests_) {
      std::move(callback).Run(encryptor_.Clone(option), true);
      return base::CallbackListSubscription();
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](Encryptor encryptor, InitCallback callback) {
              std::move(callback).Run(std::move(encryptor), true);
            },
            encryptor_.Clone(option), std::move(callback)));
    return base::CallbackListSubscription();
  }

  static Encryptor GetTestEncryptorForTesting() {
    Encryptor::KeyRing keys;
    std::vector<uint8_t> key_data(Encryptor::Key::kAES256GCMKeySize);
    crypto::RandBytes(key_data);
    Encryptor::Key key(key_data, mojom::Algorithm::kAES256GCM);
    // The test key used here indicates it is compatible with OSCrypt Sync
    // because otherwise tests that ask for instances with the
    // kEncryptSyncCompat option would fall back to OSCrypt Sync, and this
    // requires the OSCrypt mocker to be installed, which should not be needed
    // in tests and code ported to OSCrypt Async.
    key.is_os_crypt_sync_compatible_ = true;
    keys.emplace("_", std::move(key));
    Encryptor encryptor(std::move(keys), "_");
    return encryptor;
  }

 private:
  Encryptor encryptor_;
  const bool is_sync_for_unittests_;
};

std::unique_ptr<OSCryptAsync> GetTestOSCryptAsyncForTesting(
    bool is_sync_for_unittests) {
  return std::make_unique<TestOSCryptAsync>(is_sync_for_unittests);
}

Encryptor GetTestEncryptorForTesting() {
  return TestOSCryptAsync::GetTestEncryptorForTesting();
}

}  // namespace os_crypt_async
