// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/software_feature_manager_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/proto/enum_util.h"

namespace chromeos {

namespace device_sync {

// static
SoftwareFeatureManagerImpl::Factory*
    SoftwareFeatureManagerImpl::Factory::test_factory_instance_ = nullptr;

// static
std::unique_ptr<SoftwareFeatureManager>
SoftwareFeatureManagerImpl::Factory::NewInstance(
    CryptAuthClientFactory* cryptauth_client_factory) {
  if (test_factory_instance_)
    return test_factory_instance_->BuildInstance(cryptauth_client_factory);

  static base::NoDestructor<Factory> factory;
  return factory->BuildInstance(cryptauth_client_factory);
}

void SoftwareFeatureManagerImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_instance_ = test_factory;
}

SoftwareFeatureManagerImpl::Factory::~Factory() = default;

std::unique_ptr<SoftwareFeatureManager>
SoftwareFeatureManagerImpl::Factory::BuildInstance(
    CryptAuthClientFactory* cryptauth_client_factory) {
  return base::WrapUnique(
      new SoftwareFeatureManagerImpl(cryptauth_client_factory));
}

SoftwareFeatureManagerImpl::Request::Request(
    std::unique_ptr<cryptauth::ToggleEasyUnlockRequest> toggle_request,
    const base::Closure& set_software_success_callback,
    const base::Callback<void(NetworkRequestError)> error_callback)
    : error_callback(error_callback),
      toggle_request(std::move(toggle_request)),
      set_software_success_callback(set_software_success_callback) {}

SoftwareFeatureManagerImpl::Request::Request(
    std::unique_ptr<cryptauth::FindEligibleUnlockDevicesRequest> find_request,
    const base::Callback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>
        find_hosts_success_callback,
    const base::Callback<void(NetworkRequestError)> error_callback)
    : error_callback(error_callback),
      find_request(std::move(find_request)),
      find_hosts_success_callback(find_hosts_success_callback) {}

SoftwareFeatureManagerImpl::Request::~Request() = default;

SoftwareFeatureManagerImpl::SoftwareFeatureManagerImpl(
    CryptAuthClientFactory* cryptauth_client_factory)
    : crypt_auth_client_factory_(cryptauth_client_factory) {}

SoftwareFeatureManagerImpl::~SoftwareFeatureManagerImpl() = default;

void SoftwareFeatureManagerImpl::SetSoftwareFeatureState(
    const std::string& public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    const base::Closure& success_callback,
    const base::Callback<void(NetworkRequestError)>& error_callback,
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

  pending_requests_.emplace(std::make_unique<Request>(
      std::move(request), success_callback, error_callback));
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    const base::Callback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>&
        success_callback,
    const base::Callback<void(NetworkRequestError)>& error_callback) {
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

  pending_requests_.emplace(std::make_unique<Request>(
      std::move(request), success_callback, error_callback));
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::ProcessRequestQueue() {
  if (current_request_ || pending_requests_.empty())
    return;

  current_request_ = std::move(pending_requests_.front());
  pending_requests_.pop();

  if (current_request_->toggle_request)
    ProcessSetSoftwareFeatureStateRequest();
  else
    ProcessFindEligibleDevicesRequest();
}

void SoftwareFeatureManagerImpl::ProcessSetSoftwareFeatureStateRequest() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();

  current_cryptauth_client_->ToggleEasyUnlock(
      *current_request_->toggle_request,
      base::Bind(&SoftwareFeatureManagerImpl::OnToggleEasyUnlockResponse,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&SoftwareFeatureManagerImpl::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SoftwareFeatureManagerImpl::ProcessFindEligibleDevicesRequest() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();

  current_cryptauth_client_->FindEligibleUnlockDevices(
      *current_request_->find_request,
      base::Bind(
          &SoftwareFeatureManagerImpl::OnFindEligibleUnlockDevicesResponse,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&SoftwareFeatureManagerImpl::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SoftwareFeatureManagerImpl::OnToggleEasyUnlockResponse(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->set_software_success_callback.Run();
  current_request_.reset();
  ProcessRequestQueue();
}

void SoftwareFeatureManagerImpl::OnFindEligibleUnlockDevicesResponse(
    const cryptauth::FindEligibleUnlockDevicesResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->find_hosts_success_callback.Run(
      std::vector<cryptauth::ExternalDeviceInfo>(
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
  current_request_->error_callback.Run(error);
  current_request_.reset();
  ProcessRequestQueue();
}

}  // namespace device_sync

}  // namespace chromeos
