// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_

#include "base/macros.h"
#include "build/build_config.h"

// Handles the mocking of OSCrypt, such that it does not reach system level
// services.
class OSCryptMocker {
 public:
  // Inject mocking into OSCrypt.
  static void SetUp();

#if defined(OS_MACOSX)
  // Pretend that backend for storing keys is unavailable.
  static void SetBackendLocked(bool locked);
#endif

#if defined(OS_WIN)
  // Store data using the older DPAPI interface rather than session key.
  static void SetLegacyEncryption(bool legacy);

  // Reset OSCrypt so it can be initialized again with a new profile/key.
  static void ResetState();
#endif

  // Restore OSCrypt to its real behaviour.
  static void TearDown();

 private:
  DISALLOW_COPY_AND_ASSIGN(OSCryptMocker);
};

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_
