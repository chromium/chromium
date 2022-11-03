// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_feature_status_setter.h"

namespace ash {

namespace device_sync {

FakeCryptAuthFeatureStatusSetter::Request::Request(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : device_id(device_id),
      feature(feature),
      status_change(status_change),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

FakeCryptAuthFeatureStatusSetter::Request::Request(Request&& request)
    : device_id(request.device_id),
      feature(request.feature),
      status_change(request.status_change),
      success_callback(std::move(request.success_callback)),
      error_callback(std::move(request.error_callback)) {}

FakeCryptAuthFeatureStatusSetter::Request::~Request() = default;

FakeCryptAuthFeatureStatusSetter::FakeCryptAuthFeatureStatusSetter() = default;

FakeCryptAuthFeatureStatusSetter::~FakeCryptAuthFeatureStatusSetter() = default;

void FakeCryptAuthFeatureStatusSetter::SetFeatureStatus(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  requests_.emplace_back(device_id, feature, status_change,
                         std::move(success_callback),
                         std::move(error_callback));

  if (delegate_)
    delegate_->OnSetFeatureStatusCalled();
}

FakeCryptAuthFeatureStatusSetterFactory::
    FakeCryptAuthFeatureStatusSetterFactory() = default;

FakeCryptAuthFeatureStatusSetterFactory::
    ~FakeCryptAuthFeatureStatusSetterFactory() = default;

std::unique_ptr<CryptAuthFeatureStatusSetter>
FakeCryptAuthFeatureStatusSetterFactory::CreateInstance(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_instance_id_ = instance_id;
  last_instance_id_token_ = instance_id_token;
  last_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeCryptAuthFeatureStatusSetter>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace ash
