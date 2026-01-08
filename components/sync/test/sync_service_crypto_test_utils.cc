// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/sync_service_crypto_test_utils.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace syncer {

os_crypt_async::Encryptor GetEncryptorForTest() {
  static base::NoDestructor<std::unique_ptr<os_crypt_async::OSCryptAsync>>
      os_crypt_async(os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true));
  base::test::TestFuture<os_crypt_async::Encryptor> future;
  (*os_crypt_async)
      ->GetInstance(future.GetCallback(),
                    os_crypt_async::Encryptor::Option::kNone);
  return future.Take();
}

}  // namespace syncer
