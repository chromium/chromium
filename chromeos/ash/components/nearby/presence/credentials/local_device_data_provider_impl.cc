// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

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

}  // namespace

namespace ash::nearby::presence {

LocalDeviceDataProviderImpl::LocalDeviceDataProviderImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {
  CHECK(identity_manager_);
  CHECK(pref_service_);
}

LocalDeviceDataProviderImpl::~LocalDeviceDataProviderImpl() = default;

void LocalDeviceDataProviderImpl::UpdatePersistedSharedCredentials(
    const std::vector<::nearby::internal::SharedCredential>&
        new_shared_credentials) {
  base::Value::List list;
  for (const auto& credential : new_shared_credentials) {
    list.Append(credential.secret_id());
  }
  pref_service_->SetList(prefs::kNearbyPresenceSharedCredentialIdListPrefName,
                         std::move(list));
}

bool LocalDeviceDataProviderImpl::HaveSharedCredentialsChanged(
    const std::vector<::nearby::internal::SharedCredential>&
        new_shared_credentials) {
  std::set<std::string> persisted_shared_credential_ids;
  const base::Value::List& list = pref_service_->GetList(
      prefs::kNearbyPresenceSharedCredentialIdListPrefName);
  for (const auto& id : list) {
    persisted_shared_credential_ids.insert(id.GetString());
  }

  std::set<std::string> new_shared_credential_ids;
  for (const auto& credential : new_shared_credentials) {
    new_shared_credential_ids.insert(credential.secret_id());
  }

  return new_shared_credential_ids != persisted_shared_credential_ids;
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

std::string LocalDeviceDataProviderImpl::GetDeviceName() const {
  // TODO(b/283987579): When NP Settings page is implemented, check for any
  // changes to the user set device name.
  std::u16string device_type = ui::GetChromeOSDeviceName();

  const CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  std::string given_name =
      identity_manager_->FindExtendedAccountInfo(account_info).given_name;

  if (given_name.empty()) {
    return base::UTF16ToUTF8(device_type);
  }

  std::string device_name =
      l10n_util::GetStringFUTF8(IDS_NEARBY_PRESENCE_DEVICE_NAME,
                                base::UTF8ToUTF16(given_name), device_type);
  return device_name;
}

::nearby::internal::Metadata LocalDeviceDataProviderImpl::GetDeviceMetadata() {
  std::string user_name =
      pref_service_->GetString(prefs::kNearbyPresenceUserNamePrefName);
  std::string profile_url =
      pref_service_->GetString(prefs::kNearbyPresenceProfileUrlPrefName);

  // At this point in the Nearby Presence flow, if the `user_name` and
  // `profile_url` are not available, something is wrong. The `user_name` and
  // `profile_url` are persisted to prefs during first time registration flow,
  // which happens before the Device Metadata is needed to construct
  // credentials.
  CHECK(!user_name.empty());
  CHECK(!profile_url.empty());

  // `mac_address` is empty for Nearby Presence MVP on ChromeOS since
  // broadcasting is not supported.
  return proto::BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS,
      /*account_name=*/GetAccountName(),
      /*device_name=*/GetDeviceName(),
      /*user_name=*/user_name,
      /*profile_url=*/profile_url,
      /*mac_address=*/std::string());
}

std::string LocalDeviceDataProviderImpl::GetAccountName() {
  const std::string& email =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  return gaia::CanonicalizeEmail(email);
}

void LocalDeviceDataProviderImpl::SaveUserRegistrationInfo(
    const std::string& display_name,
    const std::string& image_url) {
  pref_service_->SetString(prefs::kNearbyPresenceUserNamePrefName,
                           display_name);
  pref_service_->SetString(prefs::kNearbyPresenceProfileUrlPrefName, image_url);
}

bool LocalDeviceDataProviderImpl::IsRegistrationCompleteAndUserInfoSaved() {
  // The user name pref and image url are set during first time registration
  // flow with the server. If they are not set, that means that the first time
  // registration flow (and therefore Nearby Presence initialization) has not
  // occurred. These fields are both set in the same step via
  // |SaveUserRegistrationInfo|. Additionally, check for the full
  // registration flow to be completed.
  std::string user_name =
      pref_service_->GetString(prefs::kNearbyPresenceUserNamePrefName);
  std::string image_url =
      pref_service_->GetString(prefs::kNearbyPresenceProfileUrlPrefName);
  bool registration_complete = pref_service_->GetBoolean(
      prefs::kNearbyPresenceFirstTimeRegistrationComplete);
  return (!user_name.empty() && !image_url.empty()) && registration_complete;
}

void LocalDeviceDataProviderImpl::SetRegistrationComplete(bool completed) {
  pref_service_->SetBoolean(prefs::kNearbyPresenceFirstTimeRegistrationComplete,
                            completed);
}

}  // namespace ash::nearby::presence
