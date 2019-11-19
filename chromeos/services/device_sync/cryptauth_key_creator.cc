// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_creator.h"

namespace chromeos {

namespace device_sync {

CryptAuthKeyCreator::CreateKeyData::CreateKeyData(
    CryptAuthKey::Status status,
    cryptauthv2::KeyType type,
    base::Optional<std::string> handle)
    : status(status), type(type), handle(handle) {}

CryptAuthKeyCreator::CreateKeyData::CreateKeyData(
    CryptAuthKey::Status status,
    cryptauthv2::KeyType type,
    const std::string& handle,
    const std::string& public_key,
    const std::string& private_key)
    : status(status),
      type(type),
      handle(handle),
      public_key(public_key),
      private_key(private_key) {
  DCHECK(!handle.empty());
  DCHECK(!public_key.empty());
  DCHECK(!private_key.empty());
}

CryptAuthKeyCreator::CreateKeyData::~CreateKeyData() = default;

CryptAuthKeyCreator::CreateKeyData::CreateKeyData(const CreateKeyData&) =
    default;

CryptAuthKeyCreator::CryptAuthKeyCreator() = default;

CryptAuthKeyCreator::~CryptAuthKeyCreator() = default;

}  // namespace device_sync

}  // namespace chromeos
