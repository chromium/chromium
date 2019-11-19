// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_mocker.h"

#include "components/os_crypt/os_crypt.h"

#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
#include "components/os_crypt/os_crypt_mocker_linux.h"
#endif

// static
void OSCryptMocker::SetUp() {
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychainForTesting(true);
#elif defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  OSCryptMockerLinux::SetUp();
#elif defined(OS_WIN)
  OSCrypt::UseMockKeyForTesting(true);
#endif
}

#if defined(OS_MACOSX)
// static
void OSCryptMocker::SetBackendLocked(bool locked) {
  OSCrypt::UseLockedMockKeychainForTesting(locked);
}
#endif

#if defined(OS_WIN)
// static
void OSCryptMocker::SetLegacyEncryption(bool legacy) {
  OSCrypt::SetLegacyEncryptionForTesting(legacy);
}

void OSCryptMocker::ResetState() {
  OSCrypt::ResetStateForTesting();
}

#endif

// static
void OSCryptMocker::TearDown() {
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychainForTesting(false);
#elif defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  OSCryptMockerLinux::TearDown();
#elif defined(OS_WIN)
  OSCrypt::UseMockKeyForTesting(false);
#endif
}
