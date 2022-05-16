// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_mocker_linux.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/key_storage_linux.h"
#include "components/os_crypt/os_crypt.h"

namespace {
class KeyStorageLinuxMock : public KeyStorageLinux {
 protected:
  // KeyStorageLinux
  bool Init() override {
    key_ = "the_encryption_key";
    return true;
  }

  absl::optional<std::string> GetKeyImpl() override { return key_; }

 private:
  std::string key_;
};

std::unique_ptr<KeyStorageLinux> CreateNewMock() {
  return std::make_unique<KeyStorageLinuxMock>();
}
}  // namespace

OSCryptMockerLinux::OSCryptMockerLinux(OSCrypt* os_crypt)
    : os_crypt_(os_crypt) {}

void OSCryptMockerLinux::SetUp() {
  os_crypt_->UseMockKeyStorageForTesting(  // IN-TEST
      base::BindOnce(&CreateNewMock));
}

void OSCryptMockerLinux::TearDown() {
  os_crypt_->UseMockKeyStorageForTesting(base::NullCallback());  // IN-TEST
  os_crypt_->ClearCacheForTesting();                             // IN-TEST
}
