// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_gcm_manager_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/public/cpp/gcm_constants.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

namespace {

// The 'registrationTickleType' key-value pair is present in GCM push
// messages. The values correspond to a server-side enum.
const char kRegistrationTickleTypeKey[] = "registrationTickleType";
const char kRegistrationTickleTypeForceEnrollment[] = "1";
const char kRegistrationTickleTypeUpdateEnrollment[] = "2";
const char kRegistrationTickleTypeDevicesSync[] = "3";

// Used in GCM messages sent by CryptAuth v2 DeviceSync. The value corresponding
// to this key specifies the service to notify, 1 for Enrollment and 2 for
// DeviceSync, as enumerated in cryptauthv2::TargetService.
const char kTargetServiceKey[] = "S";

// Only used in GCM messages sent by CryptAuth v2 DeviceSync. The session_id
// field of ClientAppMetadata should be set to the value corresponding to this
// key.
const char kSessionIdKey[] = "I";

// Used in GCM messages triggered by a BatchNofityGroupDevices request. The
// value corresponding to this key is the base64url-encoded, SHA-256 8-byte hash
// of the feature_type field forwarded from the BatchNotifyGroupDevicesRequest.
// CryptAuth chooses this hashing scheme to accommodate the limited bandwidth of
// GCM messages.
const char kFeatureTypeHashKey[] = "F";

// Only used in GCM messages sent by CryptAuth v2 DeviceSync. The value
// corresponding to this key specifies the relevant DeviceSync group. Currently,
// the value should always be "DeviceSync:BetterTogether".
const char kDeviceSyncGroupNameKey[] = "K";

// Determine the target service based on the keys "registrationTickleType" and
// "S". In practice, one and only one of these keys should exist in a GCM
// message. Return null if neither is set to a valid value. If both are set for
// some reason, arbitrarily prefer a valid "S" value.
base::Optional<cryptauthv2::TargetService> TargetServiceFromMessage(
    const gcm::IncomingMessage& message) {
  base::Optional<cryptauthv2::TargetService>
      target_from_registration_tickle_type;
  base::Optional<cryptauthv2::TargetService> target_from_target_service;

  auto it = message.data.find(kRegistrationTickleTypeKey);
  if (it != message.data.end()) {
    if (it->second == kRegistrationTickleTypeForceEnrollment ||
        it->second == kRegistrationTickleTypeUpdateEnrollment) {
      target_from_registration_tickle_type =
          cryptauthv2::TargetService::ENROLLMENT;
    } else if (it->second == kRegistrationTickleTypeDevicesSync) {
      target_from_registration_tickle_type =
          cryptauthv2::TargetService::DEVICE_SYNC;
    } else {
      // TODO(https://crbug.com/956592): Add metrics.
      PA_LOG(WARNING) << "Unknown tickle type in GCM message: " << it->second;
    }
  }

  it = message.data.find(kTargetServiceKey);
  if (it != message.data.end()) {
    if (it->second ==
        base::NumberToString(cryptauthv2::TargetService::ENROLLMENT)) {
      target_from_target_service = cryptauthv2::TargetService::ENROLLMENT;
    } else if (it->second ==
               base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC)) {
      target_from_target_service = cryptauthv2::TargetService::DEVICE_SYNC;
    } else {
      // TODO(https://crbug.com/956592): Add metrics.
      PA_LOG(WARNING) << "Invalid TargetService in GCM message: " << it->second;
    }
  }

  if (target_from_registration_tickle_type && target_from_target_service) {
    // TODO(https://crbug.com/956592): Add metrics.
    PA_LOG(WARNING) << "Registration tickle type, "
                    << *target_from_registration_tickle_type
                    << ", and target service, " << *target_from_target_service
                    << ", are both set in the same GCM message";
  }

  return target_from_target_service ? target_from_target_service
                                    : target_from_registration_tickle_type;
}

// Returns null if |key| doesn't exist in the |message.data| map.
base::Optional<std::string> StringValueFromMessage(
    const std::string& key,
    const gcm::IncomingMessage& message) {
  auto it = message.data.find(key);
  if (it == message.data.end())
    return base::nullopt;

  return it->second;
}

// Returns null if |message| does not contain the feature type key-value pair or
// if the value does not correspond to one of the CryptAuthFeatureType enums.
base::Optional<CryptAuthFeatureType> FeatureTypeFromMessage(
    const gcm::IncomingMessage& message) {
  base::Optional<std::string> feature_type_hash =
      StringValueFromMessage(kFeatureTypeHashKey, message);

  if (!feature_type_hash)
    return base::nullopt;

  base::Optional<CryptAuthFeatureType> feature_type =
      CryptAuthFeatureTypeFromGcmHash(*feature_type_hash);

  if (!feature_type) {
    // TODO(https://crbug.com/956592): Add metrics.
    PA_LOG(WARNING) << "GCM message contains unknown feature type hash: "
                    << *feature_type_hash;
  }

  return feature_type;
}

// If the DeviceSync group name is provided in the GCM message, verify that the
// value agrees with the name of the corresponding enrolled key. On Chrome OS,
// the only relevant DeviceSync group name is "DeviceSync:BetterTogether".
bool IsDeviceSyncGroupNameValid(const gcm::IncomingMessage& message) {
  base::Optional<std::string> group_name =
      StringValueFromMessage(kDeviceSyncGroupNameKey, message);
  return !group_name ||
         *group_name ==
             CryptAuthKeyBundle::KeyBundleNameEnumToString(
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
}

}  // namespace

// static
CryptAuthGCMManagerImpl::Factory*
    CryptAuthGCMManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<CryptAuthGCMManager>
CryptAuthGCMManagerImpl::Factory::NewInstance(gcm::GCMDriver* gcm_driver,
                                              PrefService* pref_service) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(gcm_driver, pref_service);
}

// static
void CryptAuthGCMManagerImpl::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

CryptAuthGCMManagerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthGCMManager>
CryptAuthGCMManagerImpl::Factory::BuildInstance(gcm::GCMDriver* gcm_driver,
                                                PrefService* pref_service) {
  return base::WrapUnique(
      new CryptAuthGCMManagerImpl(gcm_driver, pref_service));
}

CryptAuthGCMManagerImpl::CryptAuthGCMManagerImpl(gcm::GCMDriver* gcm_driver,
                                                 PrefService* pref_service)
    : gcm_driver_(gcm_driver),
      pref_service_(pref_service),
      registration_in_progress_(false) {}

CryptAuthGCMManagerImpl::~CryptAuthGCMManagerImpl() {
  if (gcm_driver_->GetAppHandler(kCryptAuthGcmAppId) == this)
    gcm_driver_->RemoveAppHandler(kCryptAuthGcmAppId);
}

void CryptAuthGCMManagerImpl::StartListening() {
  if (gcm_driver_->GetAppHandler(kCryptAuthGcmAppId) == this) {
    PA_LOG(VERBOSE) << "GCM app handler already added";
    return;
  }

  gcm_driver_->AddAppHandler(kCryptAuthGcmAppId, this);
}

void CryptAuthGCMManagerImpl::RegisterWithGCM() {
  if (registration_in_progress_) {
    PA_LOG(VERBOSE) << "GCM Registration is already in progress";
    return;
  }

  PA_LOG(VERBOSE) << "Beginning GCM registration...";
  registration_in_progress_ = true;

  std::vector<std::string> sender_ids(1, kCryptAuthGcmSenderId);
  gcm_driver_->Register(
      kCryptAuthGcmAppId, sender_ids,
      base::Bind(&CryptAuthGCMManagerImpl::OnRegistrationCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::string CryptAuthGCMManagerImpl::GetRegistrationId() {
  return pref_service_->GetString(prefs::kCryptAuthGCMRegistrationId);
}

void CryptAuthGCMManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthGCMManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthGCMManagerImpl::ShutdownHandler() {}

void CryptAuthGCMManagerImpl::OnStoreReset() {
  // We will automatically re-register to GCM and re-enroll the new registration
  // ID to Cryptauth during the next scheduled sync.
  pref_service_->ClearPref(prefs::kCryptAuthGCMRegistrationId);
}

void CryptAuthGCMManagerImpl::OnMessage(const std::string& app_id,
                                        const gcm::IncomingMessage& message) {
  std::vector<std::string> fields;
  for (const auto& kv : message.data)
    fields.push_back(std::string(kv.first) + ": " + std::string(kv.second));

  PA_LOG(VERBOSE) << "GCM message received:\n"
                  << "  sender_id: " << message.sender_id << "\n"
                  << "  collapse_key: " << message.collapse_key << "\n"
                  << "  data:\n    " << base::JoinString(fields, "\n    ");

  base::Optional<cryptauthv2::TargetService> target_service =
      TargetServiceFromMessage(message);
  if (!target_service) {
    // TODO(https://crbug.com/956592): Add metrics.
    PA_LOG(ERROR) << "GCM message does not specify a valid target service.";
    return;
  }

  if (!IsDeviceSyncGroupNameValid(message)) {
    // TODO(https://crbug.com/956592): Add metrics.
    PA_LOG(ERROR) << "GCM message contains unexpected DeviceSync group name: "
                  << *StringValueFromMessage(kDeviceSyncGroupNameKey, message);
    return;
  }

  base::Optional<std::string> session_id =
      StringValueFromMessage(kSessionIdKey, message);
  base::Optional<CryptAuthFeatureType> feature_type =
      FeatureTypeFromMessage(message);

  if (target_service == cryptauthv2::TargetService::ENROLLMENT) {
    for (auto& observer : observers_)
      observer.OnReenrollMessage(session_id, feature_type);

    return;
  }

  DCHECK(target_service == cryptauthv2::TargetService::DEVICE_SYNC);
  for (auto& observer : observers_)
    observer.OnResyncMessage(session_id, feature_type);
}

void CryptAuthGCMManagerImpl::OnMessagesDeleted(const std::string& app_id) {}

void CryptAuthGCMManagerImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  NOTREACHED();
}

void CryptAuthGCMManagerImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED();
}

void CryptAuthGCMManagerImpl::OnRegistrationCompleted(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  registration_in_progress_ = false;
  if (result != gcm::GCMClient::SUCCESS) {
    // TODO(https://crbug.com/956592): Add metrics.
    PA_LOG(WARNING) << "GCM registration failed with result="
                    << static_cast<int>(result);
    for (auto& observer : observers_)
      observer.OnGCMRegistrationResult(false);
    return;
  }

  PA_LOG(VERBOSE) << "GCM registration success, registration_id="
                  << registration_id;
  pref_service_->SetString(prefs::kCryptAuthGCMRegistrationId, registration_id);
  for (auto& observer : observers_)
    observer.OnGCMRegistrationResult(true);
}

}  // namespace device_sync

}  // namespace chromeos
