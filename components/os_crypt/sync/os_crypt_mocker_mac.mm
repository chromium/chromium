// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt_mocker.h"

#include <memory>

#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/keychain_v2.h"

// static
void OSCryptMocker::SetUp() {
  OSCrypt::SetKeychainForTesting(
      std::make_unique<crypto::apple::FakeKeychainV2>("test-access-group"));
}

// static
void OSCryptMocker::SetBackendLocked(bool locked) {
  if (locked) {
    OSCrypt::SetKeychainForTesting(OSCrypt::MockLockedKeychain());
  } else {
    OSCrypt::SetKeychainForTesting(
        std::make_unique<crypto::apple::FakeKeychainV2>("test-access-group"));
  }
}

// static
void OSCryptMocker::TearDown() {
  OSCrypt::SetKeychainForTesting(
      std::unique_ptr<crypto::apple::FakeKeychainV2>());
}
