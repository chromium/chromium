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
  return have_credentials_changed_;
}

std::string FakeLocalDeviceDataProvider::GetDeviceId() {
  return device_id_;
}

::nearby::internal::Metadata FakeLocalDeviceDataProvider::GetDeviceMetadata() {
  return metadata_;
}

std::string FakeLocalDeviceDataProvider::GetAccountName() {
  return account_name_;
}

bool FakeLocalDeviceDataProvider::IsUserRegistrationInfoSaved() {
  return is_user_registration_info_saved_;
}

void FakeLocalDeviceDataProvider::SaveUserRegistrationInfo(
    const std::string& display_name,
    const std::string& image_url) {
  is_user_registration_info_saved_ = true;
}

void FakeLocalDeviceDataProvider::SetHaveSharedCredentialsChanged(
    bool have_credentials_changed) {
  have_credentials_changed_ = have_credentials_changed;
}

void FakeLocalDeviceDataProvider::SetDeviceId(std::string device_id) {
  device_id_ = device_id;
}

void FakeLocalDeviceDataProvider::SetDeviceMetadata(
    ::nearby::internal::Metadata metadata) {
  metadata_ = metadata;
}

void FakeLocalDeviceDataProvider::SetAccountName(std::string account_name) {
  account_name_ = account_name;
}

void FakeLocalDeviceDataProvider::SetIsUserRegistrationInfoSaved(
    bool is_user_registration_info_saved) {
  is_user_registration_info_saved_ = is_user_registration_info_saved;
}

// Not implemented
void FakeLocalDeviceDataProvider::UpdatePersistedSharedCredentials(
    const std::vector<::nearby::internal::SharedCredential>&
        shared_credentials) {}

}  // namespace ash::nearby::presence
