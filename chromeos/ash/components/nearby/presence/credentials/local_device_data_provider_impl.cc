// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

constexpr char kPlaceHolderString[] = "0123456789";

}  // namespace

namespace ash::nearby::presence {

LocalDeviceDataProviderImpl::LocalDeviceDataProviderImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager);
  CHECK(pref_service);
}

LocalDeviceDataProviderImpl::~LocalDeviceDataProviderImpl() = default;

void LocalDeviceDataProviderImpl::UpdatePersistedSharedCredentials(
    const std::vector<::nearby::internal::SharedCredential>&
        shared_credentials) {}

bool LocalDeviceDataProviderImpl::HaveSharedCredentialsChanged(
    const std::vector<::nearby::internal::SharedCredential>&
        shared_credentials) {
  // TODO (b/276307539): Implement `HavePublicCredentialsChanged`, this
  // default implementation is to get the skeleton class to compile.
  return true;
}

std::string LocalDeviceDataProviderImpl::GetDeviceId() {
  // TODO (b/276307539): Implement `GetDeviceId`, this
  // default implementation is to get the skeleton class to compile.
  return kPlaceHolderString;
}

::nearby::internal::Metadata LocalDeviceDataProviderImpl::GetDeviceMetadata() {
  // TODO (b/276307539): Implement `GetDeviceMetadata`, this
  // default implementation is to get the skeleton class to compile.
  return BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*account_name=*/kPlaceHolderString,
      /*device_name=*/kPlaceHolderString,
      /*user_name=*/kPlaceHolderString,
      /*profile_url=*/kPlaceHolderString,
      /*mac_address=*/kPlaceHolderString);
}

std::string LocalDeviceDataProviderImpl::GetAccountName() {
  // TODO (b/276307539): Implement `GetAccountName`, this
  // default implementation is to get the skeleton class to compile.
  return kPlaceHolderString;
}

void LocalDeviceDataProviderImpl::SaveUserRegistrationInfo(
    const std::string& display_name,
    const std::string& image_url) {}

}  // namespace ash::nearby::presence
