// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_sync_preference.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/pref_names.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace {

const char kVapidECPrivateKey[] = "vapid_private_key";
const char kVapidCreationTimestamp[] = "vapid_creation_timestamp";

const char kDeviceFcmToken[] = "device_fcm_token";
const char kDeviceP256dh[] = "device_p256dh";
const char kDeviceAuthSecret[] = "device_auth_secret";

const char kRegistrationAuthorizedEntity[] = "registration_authorized_entity";
const char kRegistrationTimestamp[] = "registration_timestamp";

const char kSharingInfoVapidTargetInfo[] = "vapid_target_info";
const char kSharingInfoSenderIdTargetInfo[] = "sender_id_target_info";
const char kSharingInfoEnabledFeatures[] = "enabled_features";

base::Value::Dict TargetInfoToValue(
    const syncer::DeviceInfo::SharingTargetInfo& target_info) {
  std::string base64_p256dh = base::Base64Encode(target_info.p256dh);
  std::string base64_auth_secret = base::Base64Encode(target_info.auth_secret);

  base::Value::Dict result;
  result.Set(kDeviceFcmToken, target_info.fcm_token);
  result.Set(kDeviceP256dh, base64_p256dh);
  result.Set(kDeviceAuthSecret, base64_auth_secret);
  return result;
}

std::optional<syncer::DeviceInfo::SharingTargetInfo> ValueToTargetInfo(
    const base::Value::Dict& dict) {
  const std::string* fcm_token = dict.FindString(kDeviceFcmToken);
  if (!fcm_token) {
    return std::nullopt;
  }

  const std::string* base64_p256dh = dict.FindString(kDeviceP256dh);
  const std::string* base64_auth_secret = dict.FindString(kDeviceAuthSecret);

  std::string p256dh, auth_secret;
  if (!base64_p256dh || !base64_auth_secret ||
      !base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret)) {
    return std::nullopt;
  }

  return syncer::DeviceInfo::SharingTargetInfo{*fcm_token, std::move(p256dh),
                                               std::move(auth_secret)};
}

}  // namespace

using sync_pb::SharingSpecificFields;

SharingSyncPreference::FCMRegistration::FCMRegistration(
    std::optional<std::string> authorized_entity,
    base::Time timestamp)
    : authorized_entity(std::move(authorized_entity)), timestamp(timestamp) {}

SharingSyncPreference::FCMRegistration::FCMRegistration(
    FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration&
SharingSyncPreference::FCMRegistration::operator=(FCMRegistration&& other) =
    default;

SharingSyncPreference::FCMRegistration::~FCMRegistration() = default;

SharingSyncPreference::SharingSyncPreference(
    PrefService* prefs,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : prefs_(prefs), device_info_sync_service_(device_info_sync_service) {
  DCHECK(prefs_);
  DCHECK(device_info_sync_service_);
  local_device_info_provider_ =
      device_info_sync_service_->GetLocalDeviceInfoProvider();
  pref_change_registrar_.Init(prefs);
}

SharingSyncPreference::~SharingSyncPreference() = default;

// static
void SharingSyncPreference::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kSharingVapidKey, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kSharingFCMRegistration);
  registry->RegisterDictionaryPref(prefs::kSharingLocalSharingInfo);
}

std::optional<std::vector<uint8_t>> SharingSyncPreference::GetVapidKey() const {
  const base::Value::Dict& vapid_key = prefs_->GetDict(prefs::kSharingVapidKey);
  const std::string* base64_private_key =
      vapid_key.FindString(kVapidECPrivateKey);

  if (!base64_private_key) {
    return std::nullopt;
  }

  std::string private_key;
  if (base::Base64Decode(*base64_private_key, &private_key)) {
    return std::vector<uint8_t>(private_key.begin(), private_key.end());
  } else {
    LOG(ERROR) << "Could not decode stored vapid keys.";
    return std::nullopt;
  }
}

void SharingSyncPreference::SetVapidKey(
    const std::vector<uint8_t>& vapid_key) const {
  base::Time creation_timestamp = base::Time::Now();
  std::string base64_vapid_key = base::Base64Encode(vapid_key);
  ScopedDictPrefUpdate update(prefs_, prefs::kSharingVapidKey);
  update->Set(kVapidECPrivateKey, base64_vapid_key);
  update->Set(kVapidCreationTimestamp, base::TimeToValue(creation_timestamp));
}

void SharingSyncPreference::SetVapidKeyChangeObserver(
    const base::RepeatingClosure& obs) {
  ClearVapidKeyChangeObserver();
  pref_change_registrar_.Add(prefs::kSharingVapidKey, obs);
}

void SharingSyncPreference::ClearVapidKeyChangeObserver() {
  if (pref_change_registrar_.IsObserved(prefs::kSharingVapidKey)) {
    pref_change_registrar_.Remove(prefs::kSharingVapidKey);
  }
}

std::optional<SharingSyncPreference::FCMRegistration>
SharingSyncPreference::GetFCMRegistration() const {
  const base::Value::Dict& registration =
      prefs_->GetDict(prefs::kSharingFCMRegistration);
  const std::string* authorized_entity_ptr =
      registration.FindString(kRegistrationAuthorizedEntity);
  const base::Value* timestamp_value =
      registration.Find(kRegistrationTimestamp);
  if (!timestamp_value) {
    return std::nullopt;
  }

  std::optional<std::string> authorized_entity;
  if (authorized_entity_ptr) {
    authorized_entity = *authorized_entity_ptr;
  }

  std::optional<base::Time> timestamp = base::ValueToTime(timestamp_value);
  if (!timestamp) {
    return std::nullopt;
  }

  return FCMRegistration(authorized_entity, *timestamp);
}

void SharingSyncPreference::SetFCMRegistration(FCMRegistration registration) {
  ScopedDictPrefUpdate update(prefs_, prefs::kSharingFCMRegistration);
  if (registration.authorized_entity) {
    update->Set(kRegistrationAuthorizedEntity,
                std::move(*registration.authorized_entity));
  } else {
    update->Remove(kRegistrationAuthorizedEntity);
  }
  update->Set(kRegistrationTimestamp,
              base::TimeToValue(registration.timestamp));
}

void SharingSyncPreference::ClearFCMRegistration() {
  prefs_->ClearPref(prefs::kSharingFCMRegistration);
}

void SharingSyncPreference::SetLocalSharingInfo(
    syncer::DeviceInfo::SharingInfo sharing_info) {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info) {
    return;
  }

  // Update prefs::kSharingLocalSharingInfo to cache value locally.
  if (device_info->sharing_info() == sharing_info) {
    return;
  }

  base::Value::Dict vapid_target_info =
      TargetInfoToValue(sharing_info.vapid_target_info);
  base::Value::Dict sender_id_target_info =
      TargetInfoToValue(sharing_info.sender_id_target_info);

  base::Value::List list_value;
  for (SharingSpecificFields::EnabledFeatures feature :
       sharing_info.enabled_features) {
    list_value.Append(feature);
  }

  ScopedDictPrefUpdate local_sharing_info_update(
      prefs_, prefs::kSharingLocalSharingInfo);
  local_sharing_info_update->Set(kSharingInfoVapidTargetInfo,
                                 std::move(vapid_target_info));
  local_sharing_info_update->Set(kSharingInfoSenderIdTargetInfo,
                                 std::move(sender_id_target_info));
  local_sharing_info_update->Set(kSharingInfoEnabledFeatures,
                                 std::move(list_value));

  device_info_sync_service_->RefreshLocalDeviceInfo();
}

void SharingSyncPreference::ClearLocalSharingInfo() {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info) {
    return;
  }

  // Update prefs::kSharingLocalSharingInfo to clear local cache.
  prefs_->ClearPref(prefs::kSharingLocalSharingInfo);

  if (device_info->sharing_info()) {
    device_info_sync_service_->RefreshLocalDeviceInfo();
  }
}

// static
std::optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetLocalSharingInfoForSync(PrefService* prefs) {
  const base::Value::Dict& registration =
      prefs->GetDict(prefs::kSharingLocalSharingInfo);

  const base::Value::Dict* vapid_target_info_value =
      registration.FindDict(kSharingInfoVapidTargetInfo);
  const base::Value::Dict* sender_id_target_info_value =
      registration.FindDict(kSharingInfoSenderIdTargetInfo);
  const base::Value::List* enabled_features_value =
      registration.FindList(kSharingInfoEnabledFeatures);
  if (!vapid_target_info_value || !sender_id_target_info_value ||
      !enabled_features_value) {
    return std::nullopt;
  }

  auto vapid_target_info = ValueToTargetInfo(*vapid_target_info_value);
  auto sender_id_target_info = ValueToTargetInfo(*sender_id_target_info_value);
  if (!vapid_target_info || !sender_id_target_info) {
    return std::nullopt;
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  for (auto& value : *enabled_features_value) {
    DCHECK(value.is_int());
    int feature_value = value.GetInt();
    // Filter invalid enums from other browser versions.
    if (!sync_pb::SharingSpecificFields::EnabledFeatures_IsValid(
            feature_value)) {
      continue;
    }

    enabled_features.insert(
        static_cast<SharingSpecificFields::EnabledFeatures>(feature_value));
  }

  // Pass in null for chime_representative_target_id field because currently
  // only iOS devices register with Chime.
  return syncer::DeviceInfo::SharingInfo(
      std::move(*vapid_target_info), std::move(*sender_id_target_info),
      /*chime_representative_target_id=*/std::string(),
      std::move(enabled_features));
}
