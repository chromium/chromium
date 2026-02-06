// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt_mocker.h"

#include <memory>

#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"

#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
#include "components/os_crypt/sync/os_crypt_mocker_linux.h"
#endif

// static
#if !BUILDFLAG(IS_APPLE)
void OSCryptMocker::SetUp() {
#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
  OSCryptMockerLinux::SetUp();
#elif BUILDFLAG(IS_WIN)
  OSCrypt::UseMockKeyForTesting(true);
#endif
}
#endif

// static
std::string OSCryptMocker::GetRawEncryptionKey() {
  return OSCrypt::GetRawEncryptionKey();
}

#if BUILDFLAG(IS_WIN)
// static
void OSCryptMocker::SetLegacyEncryption(bool legacy) {
  OSCrypt::SetLegacyEncryptionForTesting(legacy);
}

void OSCryptMocker::ResetState() {
  OSCrypt::ResetStateForTesting();
}

#endif

// static
#if !BUILDFLAG(IS_APPLE)
void OSCryptMocker::TearDown() {
#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
  OSCryptMockerLinux::TearDown();
#elif BUILDFLAG(IS_WIN)
  OSCrypt::UseMockKeyForTesting(false);
#endif
}
#endif
