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
    keys.emplace(kDefaultTestKeyPrefix,
                 Encryptor::Key(crypto::RandBytesAsVector(
                                    Encryptor::Key::kAES256GCMKeySize),
                                mojom::Algorithm::kAES256GCM));
    Encryptor::Key key(
        crypto::RandBytesAsVector(Encryptor::Key::kAES256GCMKeySize),
        mojom::Algorithm::kAES256GCM);
    // This test keyring has a second key that is OS Crypt Sync compatible.
    // When a test requests an Encryptor that is OS Crypt Sync compatible, the
    // k2 key will be picked, instead of the default k1 key. This allows
    // testing of key upgrade scenarios.
    key.is_os_crypt_sync_compatible_ = true;
    keys.emplace(kOsCryptSyncCompatibleTestKeyPrefix, std::move(key));
    Encryptor encryptor(std::move(keys), kDefaultTestKeyPrefix);
    return encryptor;
  }

  static Encryptor CloneEncryptorForTesting(Encryptor::Option option) {
    return GetTestEncryptorForTesting().Clone(option);
  }

 private:
  Encryptor encryptor_;
  const bool is_sync_for_unittests_;
};

std::unique_ptr<OSCryptAsync> GetTestOSCryptAsyncForTesting(
    bool is_sync_for_unittests) {
  return std::make_unique<TestOSCryptAsync>(is_sync_for_unittests);
}

Encryptor GetTestEncryptorForTesting(Encryptor::Option option) {
  return TestOSCryptAsync::CloneEncryptorForTesting(option);
}

}  // namespace os_crypt_async
