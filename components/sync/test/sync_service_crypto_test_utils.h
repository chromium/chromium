// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SYNC_SERVICE_CRYPTO_TEST_UTILS_H_
#define COMPONENTS_SYNC_TEST_SYNC_SERVICE_CRYPTO_TEST_UTILS_H_

#include "components/os_crypt/async/common/encryptor.h"

namespace syncer {

os_crypt_async::Encryptor GetEncryptorForTest();

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SYNC_SERVICE_CRYPTO_TEST_UTILS_H_
