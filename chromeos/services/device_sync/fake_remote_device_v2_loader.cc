// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_remote_device_v2_loader.h"

#include <utility>

namespace chromeos {

namespace device_sync {

FakeRemoteDeviceV2Loader::FakeRemoteDeviceV2Loader() = default;

FakeRemoteDeviceV2Loader::~FakeRemoteDeviceV2Loader() = default;

void FakeRemoteDeviceV2Loader::Load(
    const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
    const std::string& user_email,
    const std::string& user_private_key,
    LoadCallback callback) {
  id_to_device_map_ = id_to_device_map;
  user_email_ = user_email;
  user_private_key_ = user_private_key;
  callback_ = std::move(callback);
}

FakeRemoteDeviceV2LoaderFactory::FakeRemoteDeviceV2LoaderFactory() = default;

FakeRemoteDeviceV2LoaderFactory::~FakeRemoteDeviceV2LoaderFactory() = default;

std::unique_ptr<RemoteDeviceV2Loader>
FakeRemoteDeviceV2LoaderFactory::CreateInstance() {
  auto instance = std::make_unique<FakeRemoteDeviceV2Loader>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace chromeos
