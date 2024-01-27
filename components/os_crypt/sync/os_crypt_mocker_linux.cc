// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt_mocker_linux.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"

namespace {

std::unique_ptr<KeyStorageLinux> CreateNewMock() {
  return std::make_unique<OSCryptMockerLinux>();
}

}

std::optional<std::string> OSCryptMockerLinux::GetKeyImpl() {
  return key_;
}

std::string* OSCryptMockerLinux::GetKeyPtr() {
  return &key_;
}

// static
void OSCryptMockerLinux::SetUp() {
  OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(&CreateNewMock));
}

// static
void OSCryptMockerLinux::TearDown() {
  OSCrypt::UseMockKeyStorageForTesting(base::NullCallback());
  OSCrypt::ClearCacheForTesting();
}

bool OSCryptMockerLinux::Init() {
  key_ = "the_encryption_key";
  return true;
}
