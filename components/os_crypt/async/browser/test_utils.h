// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_UTILS_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_UTILS_H_

#include <memory>

#include "components/os_crypt/async/browser/os_crypt_async.h"

namespace os_crypt_async {

// These key prefixes can be used, if necessary, by tests to verify that the
// data encrypted with OSCrypt Async matches the correct key expected.
inline constexpr char kDefaultTestKeyPrefix[] = "k1";
inline constexpr char kOsCryptSyncCompatibleTestKeyPrefix[] = "k2";

// Obtain a test OSCryptAsync. This OSCryptAsync will vend test Encryptors that
// perform encryption/decryption using a random test key. In unit tests without
// a full task environment, `is_sync_for_unittests` can be set to true.
std::unique_ptr<OSCryptAsync> GetTestOSCryptAsyncForTesting(
    bool is_sync_for_unittests = false);

// Obtain a test Encryptor. This Encryptor will perform encryption using a
// random key. The key for test Encryptors is different each time this function
// is called, and different from the ones vended from the test OSCryptAsync
// above. An `option` can be specified in the same way as calling `GetInstance`
// on `OSCryptAsync`.
Encryptor GetTestEncryptorForTesting(
    Encryptor::Option option = Encryptor::Option::kNone);

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_UTILS_H_
