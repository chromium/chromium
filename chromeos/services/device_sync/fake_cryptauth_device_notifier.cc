// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/services/device_sync/fake_cryptauth_device_notifier.h"

namespace chromeos {

namespace device_sync {

FakeCryptAuthDeviceNotifier::Request::Request(
    const base::flat_set<std::string>& device_ids,
    cryptauthv2::TargetService target_service,
    CryptAuthFeatureType feature_type,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : device_ids(device_ids),
      target_service(target_service),
      feature_type(feature_type),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

FakeCryptAuthDeviceNotifier::Request::Request(Request&& request)
    : device_ids(std::move(request.device_ids)),
      target_service(request.target_service),
      feature_type(request.feature_type),
      success_callback(std::move(request.success_callback)),
      error_callback(std::move(request.error_callback)) {}

FakeCryptAuthDeviceNotifier::Request::~Request() = default;

FakeCryptAuthDeviceNotifier::FakeCryptAuthDeviceNotifier() = default;

FakeCryptAuthDeviceNotifier::~FakeCryptAuthDeviceNotifier() = default;

void FakeCryptAuthDeviceNotifier::NotifyDevices(
    const base::flat_set<std::string>& device_ids,
    cryptauthv2::TargetService target_service,
    CryptAuthFeatureType feature_type,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  requests_.emplace_back(device_ids, target_service, feature_type,
                         std::move(success_callback),
                         std::move(error_callback));

  if (delegate_)
    delegate_->OnNotifyDevicesCalled();
}

FakeCryptAuthDeviceNotifierFactory::FakeCryptAuthDeviceNotifierFactory() =
    default;

FakeCryptAuthDeviceNotifierFactory::~FakeCryptAuthDeviceNotifierFactory() =
    default;

std::unique_ptr<CryptAuthDeviceNotifier>
FakeCryptAuthDeviceNotifierFactory::BuildInstance(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_client_app_metadata_provider_ = client_app_metadata_provider;
  last_client_factory_ = client_factory;
  last_gcm_manager_ = gcm_manager;

  auto instance = std::make_unique<FakeCryptAuthDeviceNotifier>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace chromeos
