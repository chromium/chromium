// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"

namespace ash {

namespace device_sync {

class FakeCryptAuthKeyCreator : public CryptAuthKeyCreator {
 public:
  FakeCryptAuthKeyCreator();

  FakeCryptAuthKeyCreator(const FakeCryptAuthKeyCreator&) = delete;
  FakeCryptAuthKeyCreator& operator=(const FakeCryptAuthKeyCreator&) = delete;

  ~FakeCryptAuthKeyCreator() override;

  // CryptAuthKeyCreator:
  void CreateKeys(const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
                      keys_to_create,
                  const std::optional<CryptAuthKey>& server_ephemeral_dh,
                  CreateKeysCallback create_keys_callback) override;

  const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
  keys_to_create() const {
    return keys_to_create_;
  }

  const std::optional<CryptAuthKey>& server_ephemeral_dh() const {
    return server_ephemeral_dh_;
  }

  CreateKeysCallback& create_keys_callback() { return create_keys_callback_; }

 private:
  base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData> keys_to_create_;
  std::optional<CryptAuthKey> server_ephemeral_dh_;
  CreateKeysCallback create_keys_callback_;
};

class FakeCryptAuthKeyCreatorFactory : public CryptAuthKeyCreatorImpl::Factory {
 public:
  FakeCryptAuthKeyCreatorFactory();

  FakeCryptAuthKeyCreatorFactory(const FakeCryptAuthKeyCreatorFactory&) =
      delete;
  FakeCryptAuthKeyCreatorFactory& operator=(
      const FakeCryptAuthKeyCreatorFactory&) = delete;

  ~FakeCryptAuthKeyCreatorFactory() override;

  FakeCryptAuthKeyCreator* instance() { return instance_; }

 private:
  // CryptAuthKeyCreatorImpl::Factory:
  std::unique_ptr<CryptAuthKeyCreator> CreateInstance() override;

  raw_ptr<FakeCryptAuthKeyCreator, DanglingUntriaged> instance_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_CREATOR_H_
