// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "crypto/random.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

// The device ID stored in `::nearby::internal::DeviceIdentityMetaData`
// is a 128-bit integer, represented by a byte (8-bits) array.
// This length of 16 is thus derived by dividing 128 by 8.
const size_t kDeviceIdLength = 16;

std::string GenerateDeviceId() {
  std::vector<uint8_t> device_id_bytes =
      crypto::RandBytesAsVector(kDeviceIdLength);
  return std::string(device_id_bytes.begin(), device_id_bytes.end());
}

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
    list.Append(base::NumberToString(credential.id()));
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
    new_shared_credential_ids.insert(base::NumberToString(credential.id()));
  }

  return new_shared_credential_ids != persisted_shared_credential_ids;
}

std::string LocalDeviceDataProviderImpl::GetDeviceId() {
  auto decoded_device_id = FetchAndDecodeDeviceId();

  if (!decoded_device_id) {
    decoded_device_id = GenerateDeviceId();
    EncodeAndPersistDeviceId(decoded_device_id.value());
  }

  return decoded_device_id.value();
}

::nearby::internal::DeviceIdentityMetaData
LocalDeviceDataProviderImpl::GetDeviceMetadata() {
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
      /*device_name=*/GetDeviceName(),
      /*mac_address=*/std::string(),
      /*device_id=*/GetDeviceId());
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

std::optional<std::string>
LocalDeviceDataProviderImpl::FetchAndDecodeDeviceId() {
  std::string encoded_device_id =
      pref_service_->GetString(prefs::kNearbyPresenceDeviceIdPrefName);
  if (encoded_device_id.empty()) {
    return std::nullopt;
  }

  std::string decoded_device_id;
  if (!base::Base64UrlDecode(encoded_device_id,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_device_id)) {
    return std::nullopt;
  }

  return decoded_device_id;
}

void LocalDeviceDataProviderImpl::EncodeAndPersistDeviceId(
    std::string raw_device_id_bytes) {
  std::string encoded_device_id;
  base::Base64UrlEncode(raw_device_id_bytes,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_device_id);
  pref_service_->SetString(prefs::kNearbyPresenceDeviceIdPrefName,
                           encoded_device_id);
}

}  // namespace ash::nearby::presence
