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

  void GetInstance(InitCallback callback, Encryptor::Option option) override {
    if (is_sync_for_unittests_) {
      std::move(callback).Run(encryptor_.Clone(option));
      return;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](Encryptor encryptor, InitCallback callback) {
                         std::move(callback).Run(std::move(encryptor));
                       },
                       encryptor_.Clone(option), std::move(callback)));
  }

  static TestEncryptor GetTestEncryptorForTesting() {
    Encryptor::KeyRing keys;
    keys.emplace(kDefaultTestKeyPrefix,
                 Encryptor::Key(crypto::RandBytesAsVector(
                                    Encryptor::Key::kAES256GCMKeySize),
                                mojom::Algorithm::kAES256GCM));
    Encryptor::Key key(
        crypto::RandBytesAsVector(Encryptor::Key::kAES256GCMKeySize),
        mojom::Algorithm::kAES256GCM);
    keys.emplace(kOsCryptSyncCompatibleTestKeyPrefix, std::move(key));
    TestEncryptor encryptor(std::move(keys), kDefaultTestKeyPrefix,
                            kOsCryptSyncCompatibleTestKeyPrefix);
    return encryptor;
  }

  static TestEncryptor CloneEncryptorForTesting(Encryptor::Option option) {
    return GetTestEncryptorForTesting().Clone(option);
  }

 private:
  TestEncryptor encryptor_;
  const bool is_sync_for_unittests_;
};

std::unique_ptr<OSCryptAsync> GetTestOSCryptAsyncForTesting(
    bool is_sync_for_unittests) {
  return std::make_unique<TestOSCryptAsync>(is_sync_for_unittests);
}

TestEncryptor GetTestEncryptorForTesting(Encryptor::Option option) {
  return TestOSCryptAsync::CloneEncryptorForTesting(option);
}

}  // namespace os_crypt_async
