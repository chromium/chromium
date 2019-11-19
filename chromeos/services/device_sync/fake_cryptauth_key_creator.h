// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_creator.h"
#include "chromeos/services/device_sync/cryptauth_key_creator_impl.h"

namespace chromeos {

namespace device_sync {

class FakeCryptAuthKeyCreator : public CryptAuthKeyCreator {
 public:
  FakeCryptAuthKeyCreator();
  ~FakeCryptAuthKeyCreator() override;

  // CryptAuthKeyCreator:
  void CreateKeys(const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
                      keys_to_create,
                  const base::Optional<CryptAuthKey>& server_ephemeral_dh,
                  CreateKeysCallback create_keys_callback) override;

  const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
  keys_to_create() const {
    return keys_to_create_;
  }

  const base::Optional<CryptAuthKey>& server_ephemeral_dh() const {
    return server_ephemeral_dh_;
  }

  CreateKeysCallback& create_keys_callback() { return create_keys_callback_; }

 private:
  base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData> keys_to_create_;
  base::Optional<CryptAuthKey> server_ephemeral_dh_;
  CreateKeysCallback create_keys_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthKeyCreator);
};

class FakeCryptAuthKeyCreatorFactory : public CryptAuthKeyCreatorImpl::Factory {
 public:
  FakeCryptAuthKeyCreatorFactory();
  ~FakeCryptAuthKeyCreatorFactory() override;

  FakeCryptAuthKeyCreator* instance() { return instance_; }

 private:
  // CryptAuthKeyCreatorImpl::Factory:
  std::unique_ptr<CryptAuthKeyCreator> BuildInstance() override;

  FakeCryptAuthKeyCreator* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthKeyCreatorFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_
