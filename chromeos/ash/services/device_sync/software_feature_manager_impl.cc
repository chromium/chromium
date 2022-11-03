// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/software_feature_manager_impl.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_setter_impl.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"

namespace ash {

namespace device_sync {

// static
SoftwareFeatureManagerImpl::Factory*
    SoftwareFeatureManagerImpl::Factory::test_factory_instance_ = nullptr;

// static
std::unique_ptr<SoftwareFeatureManager>
SoftwareFeatureManagerImpl::Factory::Create(
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthFeatureStatusSetter* feature_status_setter) {
  if (test_factory_instance_)
    return test_factory_instance_->CreateInstance(cryptauth_client_factory,
                                                  feature_status_setter);

  return base::WrapUnique(new SoftwareFeatureManagerImpl(
      cryptauth_client_factory, feature_status_setter));
}

void SoftwareFeatureManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_instance_ = test_factory;
}

SoftwareFeatureManagerImpl::Factory::~Factory() = default;

SoftwareFeatureManagerImpl::Request::Request(
    std::unique_ptr<cryptauth::ToggleEasyUnlockRequest> toggle_request,
    base::OnceClosure set_software_success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : request_type(RequestType::kSetSoftwareFeature),
      error_callback(std::move(error_callback)),
      toggle_request(std::move(toggle_request)),
      set_software_success_callback(std::move(set_software_success_callback)) {}

SoftwareFeatureManagerImpl::Request::Request(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure set_feature_status_success_callback,
    base::OnceCallback<void(NetworkRequestError)>
        set_feature_status_error_callback)
    : request_type(RequestType::kSetFeatureStatus),
      device_id(device_id),
      feature(feature),
      status_change(status_change),
      set_feature_status_success_callback(
          std::move(set_feature_status_success_callback)),
      set_feature_status_error_callback(
          std::move(set_feature_status_error_callback)) {}

SoftwareFeatureManagerImpl::Request::Request(
    std::unique_ptr<cryptauth::FindEligibleUnlockDevicesRequest> find_request,
    base::OnceCallback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                            const std::vector<cryptauth::IneligibleDevice>&)>
        find_hosts_success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : request_type(RequestType::kFindEligibleMultideviceHosts),
      error_callback(std::move(error_callback)),
      find_request(std::move(find_request)),
      find_hosts_success_callback(std::move(find_hosts_success_callback)) {}

SoftwareFeatureManagerImpl::Request::~Request() = default;

SoftwareFeatureManagerImpl::SoftwareFeatureManagerImpl(
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthFeatureStatusSetter* feature_status_setter)
    : crypt_auth_client_factory_(cryptauth_client_factory),
      feature_status_setter_(feature_status_setter) {}

SoftwareFeatureManagerImpl::~SoftwareFeatureManagerImpl() = default;

void SoftwareFeatureManagerImpl::SetSoftwareFeatureState(
    const std::string& public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback,
    bool is_exclusive) {
  // Note: For legacy reasons, this proto message mentions "ToggleEasyUnlock"
  // instead of "SetSoftwareFeature" in its name.
  auto request = std::make_unique<cryptauth::ToggleEasyUnlockRequest>();
  request->set_feature(SoftwareFeatureEnumToString(
      multidevice::ToCryptAuthFeature(software_feature)));
  request->set_enable(enabled);
  request->set_is_exclusive(enabled && is_exclusive);

  // Special case for EasyUnlock: if EasyUnlock is being disabled, set the
  // apply_to_all property to true, and do not set the public_key field.
  bool turn_off_easy_unlock_special_case =
      !enabled &&
      software_feature == multidevice::SoftwareFeature::kSmartLockHost;
  request->set_apply_to_all(turn_off_easy_unlock_special_case);
  if (!turn_off_easy_unlock_special_case)
    request->set_public_key(public_key);

  pending_requests_.emplace(
      std::make_unique<Request>(std::move(request), std::move(success_callback),
                                std::move(error_callback)));
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::SetFeatureStatus(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  pending_requests_.emplace(std::make_unique<Request>(
      device_id, feature, status_change, std::move(success_callback),
      std::move(error_callback)));
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    base::OnceCallback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                            const std::vector<cryptauth::IneligibleDevice>&)>
        success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  // Note: For legacy reasons, this proto message mentions "UnlockDevices"
  // instead of "MultiDeviceHosts" in its name.
  auto request =
      std::make_unique<cryptauth::FindEligibleUnlockDevicesRequest>();
  request->set_feature(SoftwareFeatureEnumToString(
      multidevice::ToCryptAuthFeature(software_feature)));

  // For historical reasons, the Bluetooth address is abused to mark a which
  // feature should receive a GCM callback. Read more at
  // https://crbug.com/883915.
  request->set_callback_bluetooth_address(SoftwareFeatureEnumToStringAllCaps(
      multidevice::ToCryptAuthFeature(software_feature)));

  pending_requests_.emplace(
      std::make_unique<Request>(std::move(request), std::move(success_callback),
                                std::move(error_callback)));
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::ProcessRequestQueue() {
  if (current_request_ || pending_requests_.empty())
    return;

  current_request_ = std::move(pending_requests_.front());
  pending_requests_.pop();

  switch (current_request_->request_type) {
    case RequestType::kSetSoftwareFeature:
      ProcessSetSoftwareFeatureStateRequest();
      break;
    case RequestType::kSetFeatureStatus:
      ProcessSetFeatureStatusRequest();
      break;
    case RequestType::kFindEligibleMultideviceHosts:
      ProcessFindEligibleDevicesRequest();
      break;
  }
}

void SoftwareFeatureManagerImpl::ProcessSetSoftwareFeatureStateRequest() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();

  current_cryptauth_client_->ToggleEasyUnlock(
      *current_request_->toggle_request,
      base::BindOnce(&SoftwareFeatureManagerImpl::OnToggleEasyUnlockResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SoftwareFeatureManagerImpl::OnErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SoftwareFeatureManagerImpl::ProcessSetFeatureStatusRequest() {
  DCHECK(feature_status_setter_);

  feature_status_setter_->SetFeatureStatus(
      current_request_->device_id, current_request_->feature,
      current_request_->status_change,
      base::BindOnce(&SoftwareFeatureManagerImpl::OnSetFeatureStatusSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SoftwareFeatureManagerImpl::OnSetFeatureStatusError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SoftwareFeatureManagerImpl::ProcessFindEligibleDevicesRequest() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();

  current_cryptauth_client_->FindEligibleUnlockDevices(
      *current_request_->find_request,
      base::BindOnce(
          &SoftwareFeatureManagerImpl::OnFindEligibleUnlockDevicesResponse,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SoftwareFeatureManagerImpl::OnErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SoftwareFeatureManagerImpl::OnToggleEasyUnlockResponse(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  current_cryptauth_client_.reset();
  std::move(current_request_->set_software_success_callback).Run();
  current_request_.reset();
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::OnSetFeatureStatusSuccess() {
  std::move(current_request_->set_feature_status_success_callback).Run();
  current_request_.reset();
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::OnSetFeatureStatusError(
    NetworkRequestError error) {
  std::move(current_request_->set_feature_status_error_callback).Run(error);
  current_request_.reset();
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::OnFindEligibleUnlockDevicesResponse(
    const cryptauth::FindEligibleUnlockDevicesResponse& response) {
  current_cryptauth_client_.reset();
  std::move(current_request_->find_hosts_success_callback)
      .Run(std::vector<cryptauth::ExternalDeviceInfo>(
               response.eligible_devices().begin(),
               response.eligible_devices().end()),
           std::vector<cryptauth::IneligibleDevice>(
               response.ineligible_devices().begin(),
               response.ineligible_devices().end()));
  current_request_.reset();
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::OnErrorResponse(NetworkRequestError error) {
  current_cryptauth_client_.reset();
  std::move(current_request_->error_callback).Run(error);
  current_request_.reset();
  ProcessRequestQueue();
}

}  // namespace device_sync

}  // namespace ash
