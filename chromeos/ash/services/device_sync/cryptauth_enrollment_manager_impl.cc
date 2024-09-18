// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager_impl.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "chromeos/ash/services/device_sync/sync_scheduler_impl.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace device_sync {

namespace {

// The number of days that an enrollment is valid. Note that we try to refresh
// the enrollment well before this time elapses.
const int kValidEnrollmentPeriodDays = 45;

// The normal period between successful enrollments in days.
const int kEnrollmentRefreshPeriodDays = 30;

// A more aggressive period between enrollments to recover when the last
// enrollment fails, in minutes. This is a base time that increases for each
// subsequent failure.
const int kEnrollmentBaseRecoveryPeriodMinutes = 10;

// The bound on the amount to jitter the period between enrollments.
const double kEnrollmentMaxJitterRatio = 0.2;

// The value of the device_software_package field in the device info uploaded
// during enrollment. This value must be the same as the app id used for GCM
// registration.
const char kDeviceSoftwarePackage[] = "com.google.chrome.cryptauth";

std::unique_ptr<SyncScheduler> CreateSyncScheduler(
    SyncScheduler::Delegate* delegate) {
  return std::make_unique<SyncSchedulerImpl>(
      delegate, base::Days(kEnrollmentRefreshPeriodDays),
      base::Minutes(kEnrollmentBaseRecoveryPeriodMinutes),
      kEnrollmentMaxJitterRatio, "CryptAuth Enrollment");
}

std::string GenerateSupportedFeaturesString(
    const cryptauth::GcmDeviceInfo& info) {
  std::stringstream ss;
  ss << "[";

  bool logged_feature = false;
  for (int i = 0; i < info.supported_software_features_size(); ++i) {
    logged_feature = true;
    ss << info.supported_software_features(i) << ", ";
  }

  if (logged_feature)
    ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.

  ss << "]";
  return ss.str();
}

}  // namespace

// static
CryptAuthEnrollmentManagerImpl::Factory*
    CryptAuthEnrollmentManagerImpl::Factory::factory_instance_ = nullptr;

// static
// TODO: b/365057260 - This is now unused and can be removed.
std::unique_ptr<CryptAuthEnrollmentManager>
CryptAuthEnrollmentManagerImpl::Factory::Create(
    base::Clock* clock,
    std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate,
    const cryptauth::GcmDeviceInfo& device_info,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        clock, std::move(enroller_factory), std::move(secure_message_delegate),
        device_info, gcm_manager, pref_service);
  }

  return base::WrapUnique(new CryptAuthEnrollmentManagerImpl(
      clock, std::move(enroller_factory), std::move(secure_message_delegate),
      device_info, gcm_manager, pref_service));
}

// static
void CryptAuthEnrollmentManagerImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

CryptAuthEnrollmentManagerImpl::Factory::~Factory() = default;

// static
// TODO: b/365057260 - This is now unused and can be removed.
void CryptAuthEnrollmentManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kCryptAuthEnrollmentIsRecoveringFromFailure, false);
  registry->RegisterDoublePref(
      prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds, 0.0);
  registry->RegisterIntegerPref(prefs::kCryptAuthEnrollmentReason,
                                cryptauth::INVOCATION_REASON_UNKNOWN);
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPublicKey,
                               std::string());
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPrivateKey,
                               std::string());
}

CryptAuthEnrollmentManagerImpl::CryptAuthEnrollmentManagerImpl(
    base::Clock* clock,
    std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate,
    const cryptauth::GcmDeviceInfo& device_info,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service)
    : clock_(clock),
      enroller_factory_(std::move(enroller_factory)),
      secure_message_delegate_(std::move(secure_message_delegate)),
      device_info_(device_info),
      gcm_manager_(gcm_manager),
      pref_service_(pref_service),
      scheduler_(CreateSyncScheduler(this /* delegate */)) {}

CryptAuthEnrollmentManagerImpl::~CryptAuthEnrollmentManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthEnrollmentManagerImpl::Start() {
  gcm_manager_->AddObserver(this);

  bool is_recovering_from_failure =
      pref_service_->GetBoolean(
          prefs::kCryptAuthEnrollmentIsRecoveringFromFailure) ||
      !IsEnrollmentValid();

  base::Time last_successful_enrollment = GetLastEnrollmentTime();
  base::TimeDelta elapsed_time_since_last_sync =
      clock_->Now() - last_successful_enrollment;

  scheduler_->Start(elapsed_time_since_last_sync,
                    is_recovering_from_failure
                        ? SyncScheduler::Strategy::AGGRESSIVE_RECOVERY
                        : SyncScheduler::Strategy::PERIODIC_REFRESH);
}

void CryptAuthEnrollmentManagerImpl::ForceEnrollmentNow(
    cryptauth::InvocationReason invocation_reason,
    const std::optional<std::string>& session_id) {
  // We store the invocation reason in a preference so that it can persist
  // across browser restarts. If the sync fails, the next retry should still use
  // this original reason instead of
  // cryptauth::INVOCATION_REASON_FAILURE_RECOVERY.
  pref_service_->SetInteger(prefs::kCryptAuthEnrollmentReason,
                            invocation_reason);
  scheduler_->ForceSync();
}

bool CryptAuthEnrollmentManagerImpl::IsEnrollmentValid() const {
  base::Time last_enrollment_time = GetLastEnrollmentTime();
  return !last_enrollment_time.is_null() &&
         (clock_->Now() - last_enrollment_time) <
             base::Days(kValidEnrollmentPeriodDays);
}

base::Time CryptAuthEnrollmentManagerImpl::GetLastEnrollmentTime() const {
  return base::Time::FromSecondsSinceUnixEpoch(pref_service_->GetDouble(
      prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds));
}

base::TimeDelta CryptAuthEnrollmentManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextSync();
}

bool CryptAuthEnrollmentManagerImpl::IsEnrollmentInProgress() const {
  return scheduler_->GetSyncState() ==
         SyncScheduler::SyncState::SYNC_IN_PROGRESS;
}

bool CryptAuthEnrollmentManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetStrategy() ==
         SyncScheduler::Strategy::AGGRESSIVE_RECOVERY;
}

void CryptAuthEnrollmentManagerImpl::OnEnrollmentFinished(bool success) {
  if (success) {
    pref_service_->SetDouble(
        prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds,
        clock_->Now().InSecondsFSinceUnixEpoch());
    pref_service_->SetInteger(prefs::kCryptAuthEnrollmentReason,
                              cryptauth::INVOCATION_REASON_UNKNOWN);
  }

  pref_service_->SetBoolean(prefs::kCryptAuthEnrollmentIsRecoveringFromFailure,
                            !success);

  sync_request_->OnDidComplete(success);
  cryptauth_enroller_.reset();
  sync_request_.reset();

  NotifyEnrollmentFinished(success);
}

std::string CryptAuthEnrollmentManagerImpl::GetUserPublicKey() const {
  std::optional<std::string> public_key = util::DecodeFromValueString(
      &pref_service_->GetValue(prefs::kCryptAuthEnrollmentUserPublicKey));
  if (!public_key) {
    PA_LOG(ERROR) << "Invalid public key stored in user prefs.";
    return std::string();
  }

  return *public_key;
}

std::string CryptAuthEnrollmentManagerImpl::GetUserPrivateKey() const {
  std::optional<std::string> private_key = util::DecodeFromValueString(
      &pref_service_->GetValue(prefs::kCryptAuthEnrollmentUserPrivateKey));
  if (!private_key) {
    PA_LOG(ERROR) << "Invalid private key stored in user prefs.";
    return std::string();
  }

  return *private_key;
}

void CryptAuthEnrollmentManagerImpl::SetSyncSchedulerForTest(
    std::unique_ptr<SyncScheduler> sync_scheduler) {
  scheduler_ = std::move(sync_scheduler);
}

void CryptAuthEnrollmentManagerImpl::OnGCMRegistrationResult(bool success) {
  if (!sync_request_)
    return;

  PA_LOG(VERBOSE) << "GCM registration for CryptAuth Enrollment completed: "
                  << success;
  if (success)
    DoCryptAuthEnrollment();
  else
    OnEnrollmentFinished(false);
}

void CryptAuthEnrollmentManagerImpl::OnKeyPairGenerated(
    const std::string& public_key,
    const std::string& private_key) {
  if (!public_key.empty() && !private_key.empty()) {
    PA_LOG(VERBOSE) << "Key pair generated for CryptAuth enrollment";

    // Pref values must be UTF-8 valid base::Value strings.
    pref_service_->Set(prefs::kCryptAuthEnrollmentUserPublicKey,
                       util::EncodeAsValueString(public_key));
    pref_service_->Set(prefs::kCryptAuthEnrollmentUserPrivateKey,
                       util::EncodeAsValueString(private_key));
    DoCryptAuthEnrollment();
  } else {
    OnEnrollmentFinished(false);
  }
}

void CryptAuthEnrollmentManagerImpl::OnReenrollMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {
  ForceEnrollmentNow(cryptauth::INVOCATION_REASON_SERVER_INITIATED,
                     std::nullopt /* session_id */);
}

void CryptAuthEnrollmentManagerImpl::OnSyncRequested(
    std::unique_ptr<SyncScheduler::SyncRequest> sync_request) {
  NotifyEnrollmentStarted();

  sync_request_ = std::move(sync_request);
  const std::string& registration_id = gcm_manager_->GetRegistrationId();
  if (registration_id.empty() ||
      CryptAuthGCMManager::IsRegistrationIdDeprecated(registration_id) ||
      pref_service_->GetInteger(prefs::kCryptAuthEnrollmentReason) ==
          cryptauth::INVOCATION_REASON_MANUAL) {
    gcm_manager_->RegisterWithGCM();
  } else {
    DoCryptAuthEnrollment();
  }
}

void CryptAuthEnrollmentManagerImpl::DoCryptAuthEnrollment() {
  if (GetUserPublicKey().empty() || GetUserPrivateKey().empty()) {
    secure_message_delegate_->GenerateKeyPair(
        base::BindOnce(&CryptAuthEnrollmentManagerImpl::OnKeyPairGenerated,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    DoCryptAuthEnrollmentWithKeys();
  }
}

void CryptAuthEnrollmentManagerImpl::DoCryptAuthEnrollmentWithKeys() {
  DCHECK(sync_request_);
  cryptauth::InvocationReason invocation_reason =
      cryptauth::INVOCATION_REASON_UNKNOWN;

  int reason_stored_in_prefs =
      pref_service_->GetInteger(prefs::kCryptAuthEnrollmentReason);

  if (cryptauth::InvocationReason_IsValid(reason_stored_in_prefs) &&
      reason_stored_in_prefs != cryptauth::INVOCATION_REASON_UNKNOWN) {
    invocation_reason =
        static_cast<cryptauth::InvocationReason>(reason_stored_in_prefs);
  } else if (GetLastEnrollmentTime().is_null()) {
    invocation_reason = cryptauth::INVOCATION_REASON_INITIALIZATION;
  } else if (!IsEnrollmentValid()) {
    invocation_reason = cryptauth::INVOCATION_REASON_EXPIRATION;
  } else if (scheduler_->GetStrategy() ==
             SyncScheduler::Strategy::PERIODIC_REFRESH) {
    invocation_reason = cryptauth::INVOCATION_REASON_PERIODIC;
  } else if (scheduler_->GetStrategy() ==
             SyncScheduler::Strategy::AGGRESSIVE_RECOVERY) {
    invocation_reason = cryptauth::INVOCATION_REASON_FAILURE_RECOVERY;
  }

  // Fill in the current GCM registration id before enrolling, and explicitly
  // make sure that the software package is the same as the GCM app id.
  cryptauth::GcmDeviceInfo device_info(device_info_);
  device_info.set_gcm_registration_id(gcm_manager_->GetRegistrationId());
  device_info.set_device_software_package(kDeviceSoftwarePackage);

  PA_LOG(VERBOSE) << "Making enrollment:\n"
                  << "  public_key: "
                  << util::EncodeAsValueString(GetUserPublicKey()) << "\n"
                  << "  invocation_reason: " << invocation_reason << "\n"
                  << "  gcm_registration_id: "
                  << device_info.gcm_registration_id()
                  << "  supported features: "
                  << GenerateSupportedFeaturesString(device_info);

  cryptauth_enroller_ = enroller_factory_->CreateInstance();
  cryptauth_enroller_->Enroll(
      GetUserPublicKey(), GetUserPrivateKey(), device_info, invocation_reason,
      base::BindOnce(&CryptAuthEnrollmentManagerImpl::OnEnrollmentFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace device_sync

}  // namespace ash
