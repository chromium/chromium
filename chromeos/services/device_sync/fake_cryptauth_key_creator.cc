// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_key_creator.h"

#include <utility>

namespace chromeos {

namespace device_sync {

FakeCryptAuthKeyCreator::FakeCryptAuthKeyCreator() = default;

FakeCryptAuthKeyCreator::~FakeCryptAuthKeyCreator() = default;

void FakeCryptAuthKeyCreator::CreateKeys(
    const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
        keys_to_create,
    const base::Optional<CryptAuthKey>& server_ephemeral_dh,
    CreateKeysCallback create_keys_callback) {
  DCHECK(!keys_to_create.empty());
  DCHECK(keys_to_create_.empty());
  keys_to_create_ = keys_to_create;
  server_ephemeral_dh_ = server_ephemeral_dh;
  create_keys_callback_ = std::move(create_keys_callback);
}

FakeCryptAuthKeyCreatorFactory::FakeCryptAuthKeyCreatorFactory() = default;

FakeCryptAuthKeyCreatorFactory::~FakeCryptAuthKeyCreatorFactory() = default;

std::unique_ptr<CryptAuthKeyCreator>
FakeCryptAuthKeyCreatorFactory::BuildInstance() {
  auto instance = std::make_unique<FakeCryptAuthKeyCreator>();
  instance_ = instance.get();

  return instance;
}

}  // namespace device_sync

}  // namespace chromeos
