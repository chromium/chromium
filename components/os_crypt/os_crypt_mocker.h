// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_

#include <memory>

#include "build/build_config.h"

#include "base/memory/raw_ptr.h"
#include "components/os_crypt/os_crypt.h"

class OSCryptMockerLinux;

class OSCrypt;

// Handles the mocking of OSCrypt, such that it does not reach system level
// services.
class OSCryptMocker {
 public:
  explicit OSCryptMocker(OSCrypt* os_crypt = OSCrypt::GetInstance());
  OSCryptMocker(const OSCryptMocker&) = delete;
  OSCryptMocker& operator=(const OSCryptMocker&) = delete;
  OSCryptMocker(OSCryptMocker&&) = delete;
  OSCryptMocker& operator=(OSCryptMocker&&) = delete;
  ~OSCryptMocker();

#if BUILDFLAG(IS_APPLE)
  // Pretend that backend for storing keys is unavailable.
  void SetBackendLocked(bool locked);
#endif

#if BUILDFLAG(IS_WIN)
  // Store data using the older DPAPI interface rather than session key.
  void SetLegacyEncryption(bool legacy);

  // Reset OSCrypt so it can be initialized again with a new profile/key.
  void ResetState();
#endif

 private:
  raw_ptr<OSCrypt> os_crypt_;
  std::unique_ptr<OSCryptMockerLinux> os_crypt_mocker_linux_;
};

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_H_
