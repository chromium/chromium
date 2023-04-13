// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/rand_util.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Using the alphanumeric characters below, this provides 36^10 unique device
// IDs. Note that the uniqueness requirement is not global; the IDs are only
// used to differentiate between devices associated with a single GAIA account.
const size_t kDeviceIdLength = 10;

// Possible characters used in a randomly generated device ID.
constexpr std::array<char, 36> kAlphaNumericChars = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

const std::string& kPlaceHolderString = "0123456789";

}  // namespace

namespace ash::nearby::presence {

LocalDeviceDataProviderImpl::LocalDeviceDataProviderImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service) {
  CHECK(identity_manager);
  CHECK(pref_service_);
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
  std::string id =
      pref_service_->GetString(prefs::kNearbyPresenceDeviceIdPrefName);

  // If the local device ID has already been generated, then return it. If this
  // this is the first time `GetDeviceID` has been called, then generate the
  // local device ID, persist it, and return it to callers.
  if (!id.empty()) {
    return id;
  }

  for (size_t i = 0; i < kDeviceIdLength; ++i) {
    id += kAlphaNumericChars[base::RandGenerator(kAlphaNumericChars.size())];
  }

  pref_service_->SetString(prefs::kNearbyPresenceDeviceIdPrefName, id);
  return id;
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
