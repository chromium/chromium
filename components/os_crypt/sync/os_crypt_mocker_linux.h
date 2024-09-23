// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_LINUX_H_
#define COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_LINUX_H_

#include <optional>
#include <string>

#include "components/os_crypt/sync/key_storage_linux.h"

// Holds and serves a password from memory.
class OSCryptMockerLinux : public KeyStorageLinux {
 public:
  OSCryptMockerLinux() = default;

  OSCryptMockerLinux(const OSCryptMockerLinux&) = delete;
  OSCryptMockerLinux& operator=(const OSCryptMockerLinux&) = delete;

  ~OSCryptMockerLinux() override = default;

  // Get a pointer to the stored password. OSCryptMockerLinux owns the pointer.
  std::string* GetKeyPtr();

  // Inject the mocking scheme into OSCrypt.
  static void SetUp();

  // Restore OSCrypt to its real behaviour.
  static void TearDown();

 protected:
  // KeyStorageLinux
  bool Init() override;
  std::optional<std::string> GetKeyImpl() override;

 private:
  std::string key_;
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_MOCKER_LINUX_H_
