// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_mocker.h"

#include "build/build_config.h"
#include "components/os_crypt/os_crypt.h"

#include "components/os_crypt/os_crypt_mocker_linux.h"

OSCryptMocker::OSCryptMocker(OSCrypt* os_crypt) : os_crypt_(os_crypt) {
#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  os_crypt_mocker_linux_ = std::make_unique<OSCryptMockerLinux>(os_crypt_);
  os_crypt_mocker_linux_->SetUp();
#endif
#if BUILDFLAG(IS_APPLE)
  os_crypt_->UseMockKeychainForTesting(true);  // IN-TEST
#elif BUILDFLAG(IS_WIN)
  os_crypt_->UseMockKeyForTesting(true);  // IN-TEST
#endif
}

OSCryptMocker::~OSCryptMocker() {
#if BUILDFLAG(IS_APPLE)
  os_crypt_->UseMockKeychainForTesting(false);  // IN-TEST
#elif defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  os_crypt_mocker_linux_->TearDown();
#elif BUILDFLAG(IS_WIN)
  os_crypt_->UseMockKeyForTesting(false);  // IN-TEST
#endif
}

#if BUILDFLAG(IS_APPLE)
void OSCryptMocker::SetBackendLocked(bool locked) {
  os_crypt_->UseLockedMockKeychainForTesting(locked);  // IN-TEST
}
#endif

#if BUILDFLAG(IS_WIN)
void OSCryptMocker::SetLegacyEncryption(bool legacy) {
  os_crypt_->SetLegacyEncryptionForTesting(legacy);  // IN-TEST
}

void OSCryptMocker::ResetState() {
  os_crypt_->ResetStateForTesting();  // IN-TEST
}
#endif
