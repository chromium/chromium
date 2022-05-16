// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class OSCrypt;

// Holds and serves a password from memory.
class OSCryptMockerLinux {
 public:
  explicit OSCryptMockerLinux(OSCrypt* os_crypt);

  OSCryptMockerLinux(const OSCryptMockerLinux&) = delete;
  OSCryptMockerLinux& operator=(const OSCryptMockerLinux&) = delete;
  OSCryptMockerLinux(OSCryptMockerLinux&&) = delete;
  OSCryptMockerLinux& operator=(OSCryptMockerLinux&&) = delete;

  ~OSCryptMockerLinux() = default;

  // Inject the mocking scheme into OSCrypt.
  void SetUp();

  // Restore OSCrypt to its real behaviour.
  void TearDown();

 private:
  raw_ptr<OSCrypt> os_crypt_;
};

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_
