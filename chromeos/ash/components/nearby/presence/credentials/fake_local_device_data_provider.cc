// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/fake_local_device_data_provider.h"

#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "components/prefs/pref_service.h"

namespace ash::nearby::presence {

FakeLocalDeviceDataProvider::FakeLocalDeviceDataProvider() = default;
FakeLocalDeviceDataProvider::~FakeLocalDeviceDataProvider() = default;

bool FakeLocalDeviceDataProvider::HaveSharedCredentialsChanged(
    const std::vector<::nearby::internal::SharedCredential>&
        shared_credentials) {
  if (have_shared_credentials_changed_callback_) {
    std::move(have_shared_credentials_changed_callback_).Run();
  }

  return have_shared_credentials_changed_;
}

std::string FakeLocalDeviceDataProvider::GetDeviceId() {
  return device_id_;
}

::nearby::internal::DeviceIdentityMetaData
FakeLocalDeviceDataProvider::GetDeviceMetadata() {
  return metadata_;
}

std::string FakeLocalDeviceDataProvider::GetAccountName() {
  return account_name_;
}

bool FakeLocalDeviceDataProvider::IsRegistrationCompleteAndUserInfoSaved() {
  return is_registration_complete_ && user_info_saved_;
}

void FakeLocalDeviceDataProvider::SaveUserRegistrationInfo(
    const std::string& display_name,
    const std::string& image_url) {
  user_info_saved_ = true;
}

void FakeLocalDeviceDataProvider::SetRegistrationComplete(bool complete) {
  is_registration_complete_ = complete;
}

void FakeLocalDeviceDataProvider::SetHaveSharedCredentialsChanged(
    bool have_shared_credentials_changed) {
  have_shared_credentials_changed_ = have_shared_credentials_changed;
}

void FakeLocalDeviceDataProvider::SetDeviceId(std::string device_id) {
  device_id_ = device_id;
}

void FakeLocalDeviceDataProvider::SetDeviceMetadata(
    ::nearby::internal::DeviceIdentityMetaData metadata) {
  metadata_ = metadata;
}

void FakeLocalDeviceDataProvider::SetAccountName(std::string account_name) {
  account_name_ = account_name;
}

void FakeLocalDeviceDataProvider::SetUpdatePersistedSharedCredentialsCallback(
    base::OnceClosure callback) {
  on_persist_credentials_callback_ = std::move(callback);
}

void FakeLocalDeviceDataProvider::SetHaveSharedCredentialsChangedCallback(
    base::OnceClosure callback) {
  have_shared_credentials_changed_callback_ = std::move(callback);
}

void FakeLocalDeviceDataProvider::UpdatePersistedSharedCredentials(
    const std::vector<::nearby::internal::SharedCredential>&
        shared_credentials) {
  CHECK(on_persist_credentials_callback_);
  std::move(on_persist_credentials_callback_).Run();
}

}  // namespace ash::nearby::presence
