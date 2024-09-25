// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_constants.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace device_sync {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class TargetServiceForMetrics {
  kUnknown = 0,
  kEnrollment = 1,
  kDeviceSync = 2,
  // Used for UMA logs.
  kMaxValue = kDeviceSync
};

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
std::optional<cryptauthv2::TargetService> TargetServiceFromMessage(
    const gcm::IncomingMessage& message) {
  std::optional<cryptauthv2::TargetService>
      target_from_registration_tickle_type;
  std::optional<cryptauthv2::TargetService> target_from_target_service;

  auto it = message.data.find(kRegistrationTickleTypeKey);
  if (it != message.data.end()) {
    TargetServiceForMetrics target_service_for_metrics;
    if (it->second == kRegistrationTickleTypeForceEnrollment ||
        it->second == kRegistrationTickleTypeUpdateEnrollment) {
      target_service_for_metrics = TargetServiceForMetrics::kEnrollment;
      target_from_registration_tickle_type =
          cryptauthv2::TargetService::ENROLLMENT;
    } else if (it->second == kRegistrationTickleTypeDevicesSync) {
      target_service_for_metrics = TargetServiceForMetrics::kDeviceSync;
      target_from_registration_tickle_type =
          cryptauthv2::TargetService::DEVICE_SYNC;
    } else {
      target_service_for_metrics = TargetServiceForMetrics::kUnknown;
      PA_LOG(WARNING) << "Unknown tickle type in GCM message: " << it->second;
    }
    base::UmaHistogramEnumeration(
        "CryptAuth.Gcm.Message.TargetService.FromRegistrationTickleType",
        target_service_for_metrics);
  }

  it = message.data.find(kTargetServiceKey);
  if (it != message.data.end()) {
    TargetServiceForMetrics target_service_for_metrics;
    if (it->second ==
        base::NumberToString(cryptauthv2::TargetService::ENROLLMENT)) {
      target_service_for_metrics = TargetServiceForMetrics::kEnrollment;
      target_from_target_service = cryptauthv2::TargetService::ENROLLMENT;
    } else if (it->second ==
               base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC)) {
      target_service_for_metrics = TargetServiceForMetrics::kDeviceSync;
      target_from_target_service = cryptauthv2::TargetService::DEVICE_SYNC;
    } else {
      target_service_for_metrics = TargetServiceForMetrics::kUnknown;
      PA_LOG(WARNING) << "Invalid TargetService in GCM message: " << it->second;
    }
    base::UmaHistogramEnumeration(
        "CryptAuth.Gcm.Message.TargetService.FromTargetServiceValue",
        target_service_for_metrics);
  }

  bool are_tickle_type_and_target_service_both_specified =
      target_from_registration_tickle_type && target_from_target_service;
  base::UmaHistogramBoolean(
      "CryptAuth.Gcm.Message.TargetService."
      "AreTickleTypeAndTargetServiceBothSpecified",
      are_tickle_type_and_target_service_both_specified);
  if (are_tickle_type_and_target_service_both_specified) {
    PA_LOG(WARNING) << "Registration tickle type, "
                    << *target_from_registration_tickle_type
                    << ", and target service, " << *target_from_target_service
                    << ", are both set in the same GCM message";
  }

  // If the target service is specified via both the CryptAuth v1 registration
  // tickle type field and the v2 target service field, prefer the v2 value.
  // That said, we do not expect both to be used in the same GCM message.
  return target_from_target_service ? target_from_target_service
                                    : target_from_registration_tickle_type;
}

// Returns null if |key| doesn't exist in the |message.data| map.
std::optional<std::string> StringValueFromMessage(
    const std::string& key,
    const gcm::IncomingMessage& message) {
  auto it = message.data.find(key);
  if (it == message.data.end())
    return std::nullopt;

  return it->second;
}

// Returns null if |message| does not contain the feature type key-value pair or
// if the value does not correspond to one of the CryptAuthFeatureType enums.
std::optional<CryptAuthFeatureType> FeatureTypeFromMessage(
    const gcm::IncomingMessage& message) {
  std::optional<std::string> feature_type_hash =
      StringValueFromMessage(kFeatureTypeHashKey, message);

  if (!feature_type_hash)
    return std::nullopt;

  std::optional<CryptAuthFeatureType> feature_type =
      CryptAuthFeatureTypeFromGcmHash(*feature_type_hash);
  base::UmaHistogramBoolean("CryptAuth.Gcm.Message.IsKnownFeatureType",
                            feature_type.has_value());
  if (feature_type) {
    base::UmaHistogramEnumeration("CryptAuth.Gcm.Message.FeatureType",
                                  *feature_type);
  } else {
    PA_LOG(WARNING) << "GCM message contains unknown feature type hash: "
                    << *feature_type_hash;
  }

  return feature_type;
}

// If the DeviceSync group name is provided in the GCM message, verify that the
// value agrees with the name of the corresponding enrolled key. On Chrome OS,
// the only relevant DeviceSync group name is "DeviceSync:BetterTogether".
bool IsDeviceSyncGroupNameValid(const gcm::IncomingMessage& message) {
  std::optional<std::string> group_name =
      StringValueFromMessage(kDeviceSyncGroupNameKey, message);
  if (!group_name)
    return true;

  bool is_device_sync_group_name_valid =
      *group_name == CryptAuthKeyBundle::KeyBundleNameEnumToString(
                         CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  base::UmaHistogramBoolean("CryptAuth.Gcm.Message.IsDeviceSyncGroupNameValid",
                            is_device_sync_group_name_valid);

  return is_device_sync_group_name_valid;
}

void RecordGCMRegistrationMetrics(instance_id::InstanceID::Result result,
                                  base::TimeDelta execution_time) {
  base::UmaHistogramCustomTimes(
      "CryptAuth.Gcm.Registration.AttemptTimeWithRetries", execution_time,
      base::Seconds(1) /* min */, base::Minutes(10) /* max */,
      100 /* buckets */);

  base::UmaHistogramEnumeration("CryptAuth.Gcm.Registration.Result2", result);
}

}  // namespace

// static
CryptAuthGCMManagerImpl::Factory*
    CryptAuthGCMManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<CryptAuthGCMManager> CryptAuthGCMManagerImpl::Factory::Create(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(gcm_driver, instance_id_driver,
                                             pref_service);

  return base::WrapUnique(new CryptAuthGCMManagerImpl(
      gcm_driver, instance_id_driver, pref_service));
}

// static
void CryptAuthGCMManagerImpl::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

CryptAuthGCMManagerImpl::Factory::~Factory() = default;

CryptAuthGCMManagerImpl::CryptAuthGCMManagerImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      pref_service_(pref_service),
      registration_in_progress_(false) {}

CryptAuthGCMManagerImpl::~CryptAuthGCMManagerImpl() {
  if (IsListening())
    gcm_driver_->RemoveAppHandler(kCryptAuthGcmAppId);
}

void CryptAuthGCMManagerImpl::StartListening() {
  if (IsListening()) {
    PA_LOG(VERBOSE) << "GCM app handler already added";
    return;
  }

  gcm_driver_->AddAppHandler(kCryptAuthGcmAppId, this);
}

bool CryptAuthGCMManagerImpl::IsListening() {
  return gcm_driver_->GetAppHandler(kCryptAuthGcmAppId) == this;
}

void CryptAuthGCMManagerImpl::RegisterWithGCM() {
  if (registration_in_progress_) {
    PA_LOG(VERBOSE) << "GCM Registration is already in progress";
    return;
  }

  PA_LOG(VERBOSE) << "Beginning GCM registration...";
  registration_in_progress_ = true;
  gcm_registration_start_timestamp_ = base::TimeTicks::Now();

  instance_id_driver_->GetInstanceID(kCryptAuthGcmAppId)
      ->GetToken(
          kCryptAuthGcmSenderId, instance_id::kGCMScope,
          /*time_to_live=*/base::TimeDelta(), /*flags=*/{},
          base::BindOnce(&CryptAuthGCMManagerImpl::OnRegistrationCompleted,
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

  std::optional<cryptauthv2::TargetService> target_service =
      TargetServiceFromMessage(message);
  if (!target_service) {
    PA_LOG(ERROR) << "GCM message does not specify a valid target service.";
    return;
  }

  if (!IsDeviceSyncGroupNameValid(message)) {
    PA_LOG(ERROR) << "GCM message contains unexpected DeviceSync group name: "
                  << *StringValueFromMessage(kDeviceSyncGroupNameKey, message);
    return;
  }

  std::optional<std::string> session_id =
      StringValueFromMessage(kSessionIdKey, message);
  std::optional<CryptAuthFeatureType> feature_type =
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
  NOTREACHED_IN_MIGRATION();
}

void CryptAuthGCMManagerImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED_IN_MIGRATION();
}

void CryptAuthGCMManagerImpl::OnRegistrationCompleted(
    const std::string& registration_id,
    instance_id::InstanceID::Result result) {
  registration_in_progress_ = false;
  RecordGCMRegistrationMetrics(
      result, base::TimeTicks::Now() -
                  gcm_registration_start_timestamp_ /* execution_time */);

  if (result != instance_id::InstanceID::Result::SUCCESS) {
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

}  // namespace ash
