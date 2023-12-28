// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_H_
#define COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_H_

#include "build/build_config.h"

#include <string>

// Handles the mocking of OSCrypt, such that it does not reach system level
// services.
class OSCryptMocker {
 public:
  OSCryptMocker(const OSCryptMocker&) = delete;
  OSCryptMocker& operator=(const OSCryptMocker&) = delete;

  // Inject mocking into OSCrypt.
  static void SetUp();

  // Obtain the raw encryption key from OSCrypt. This is used to e.g. initialize
  // the mock key in another process.
  static std::string GetRawEncryptionKey();

#if BUILDFLAG(IS_APPLE)
  // Pretend that backend for storing keys is unavailable.
  static void SetBackendLocked(bool locked);
#endif

#if BUILDFLAG(IS_WIN)
  // Store data using the older DPAPI interface rather than session key.
  static void SetLegacyEncryption(bool legacy);

  // Reset OSCrypt so it can be initialized again with a new profile/key.
  static void ResetState();
#endif

  // Restore OSCrypt to its real behaviour.
  static void TearDown();
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_H_
