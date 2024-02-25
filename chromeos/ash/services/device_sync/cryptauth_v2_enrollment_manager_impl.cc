// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller_impl.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace device_sync {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UserKeyPairState {
  // No v1 key; no v2 key. (Not enrolled)
  kNoV1KeyNoV2Key = 0,
  // v1 key exists; no v2 key. (Only v1 enrolled)
  kYesV1KeyNoV2Key = 1,
  // No v1 key; v2 key exists. (Only v2 enrolled)
  kNoV1KeyYesV2Key = 2,
  // v1 and v2 keys exist and agree.
  kYesV1KeyYesV2KeyAgree = 3,
  // v1 and v2 keys exist and disagree. (Enrolled with v2, rolled back to v1,
  // enrolled with v1, rolled forward to v2)
  kYesV1KeyYesV2KeyDisagree = 4,
  kMaxValue = kYesV1KeyYesV2KeyDisagree
};

UserKeyPairState GetUserKeyPairState(const std::string& public_key_v1,
                                     const std::string& private_key_v1,
                                     const CryptAuthKey* key_v2) {
  bool v1_key_exists = !public_key_v1.empty() && !private_key_v1.empty();

  if (v1_key_exists && key_v2) {
    if (public_key_v1 == key_v2->public_key() &&
        private_key_v1 == key_v2->private_key()) {
      return UserKeyPairState::kYesV1KeyYesV2KeyAgree;
    } else {
      return UserKeyPairState::kYesV1KeyYesV2KeyDisagree;
    }
  } else if (v1_key_exists && !key_v2) {
    return UserKeyPairState::kYesV1KeyNoV2Key;
  } else if (!v1_key_exists && key_v2) {
    return UserKeyPairState::kNoV1KeyYesV2Key;
  } else {
    return UserKeyPairState::kNoV1KeyNoV2Key;
  }
}

cryptauthv2::ClientMetadata::InvocationReason ConvertInvocationReasonV1ToV2(
    cryptauth::InvocationReason invocation_reason_v1) {
  switch (invocation_reason_v1) {
    case cryptauth::InvocationReason::INVOCATION_REASON_UNKNOWN:
      return cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED;
    case cryptauth::InvocationReason::INVOCATION_REASON_INITIALIZATION:
      return cryptauthv2::ClientMetadata::INITIALIZATION;
    case cryptauth::InvocationReason::INVOCATION_REASON_PERIODIC:
      return cryptauthv2::ClientMetadata::PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_SLOW_PERIODIC:
      return cryptauthv2::ClientMetadata::SLOW_PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_FAST_PERIODIC:
      return cryptauthv2::ClientMetadata::FAST_PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_EXPIRATION:
      return cryptauthv2::ClientMetadata::EXPIRATION;
    case cryptauth::InvocationReason::INVOCATION_REASON_FAILURE_RECOVERY:
      return cryptauthv2::ClientMetadata::FAILURE_RECOVERY;
    case cryptauth::InvocationReason::INVOCATION_REASON_NEW_ACCOUNT:
      return cryptauthv2::ClientMetadata::NEW_ACCOUNT;
    case cryptauth::InvocationReason::INVOCATION_REASON_CHANGED_ACCOUNT:
      return cryptauthv2::ClientMetadata::CHANGED_ACCOUNT;
    case cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED:
      return cryptauthv2::ClientMetadata::FEATURE_TOGGLED;
    case cryptauth::InvocationReason::INVOCATION_REASON_SERVER_INITIATED:
      return cryptauthv2::ClientMetadata::SERVER_INITIATED;
    case cryptauth::InvocationReason::INVOCATION_REASON_ADDRESS_CHANGE:
      return cryptauthv2::ClientMetadata::ADDRESS_CHANGE;
    case cryptauth::InvocationReason::INVOCATION_REASON_SOFTWARE_UPDATE:
      return cryptauthv2::ClientMetadata::SOFTWARE_UPDATE;
    case cryptauth::InvocationReason::INVOCATION_REASON_MANUAL:
      return cryptauthv2::ClientMetadata::MANUAL;
    default:
      PA_LOG(WARNING) << "Unknown v1 invocation reason: "
                      << invocation_reason_v1;
      return cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED;
  }
}

void RecordEnrollmentResult(const CryptAuthEnrollmentResult& result) {
  base::UmaHistogramBoolean("CryptAuth.EnrollmentV2.Result.Success",
                            result.IsSuccess());
  base::UmaHistogramEnumeration("CryptAuth.EnrollmentV2.Result.ResultCode",
                                result.result_code());
}

}  // namespace

// static
CryptAuthV2EnrollmentManagerImpl::Factory*
    CryptAuthV2EnrollmentManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthEnrollmentManager>
CryptAuthV2EnrollmentManagerImpl::Factory::Create(
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateInstance(client_app_metadata, key_registry,
                                         client_factory, gcm_manager, scheduler,
                                         pref_service, clock);
  }

  return base::WrapUnique(new CryptAuthV2EnrollmentManagerImpl(
      client_app_metadata, key_registry, client_factory, gcm_manager, scheduler,
      pref_service, clock));
}

// static
void CryptAuthV2EnrollmentManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthV2EnrollmentManagerImpl::Factory::~Factory() = default;

// static
void CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kCryptAuthLastEnrolledClientAppMetadataHash, std::string());

  // TODO(nohle): Remove when v1 Enrollment is deprecated.
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPublicKey,
                               std::string());
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPrivateKey,
                               std::string());
}

CryptAuthV2EnrollmentManagerImpl::CryptAuthV2EnrollmentManagerImpl(
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    base::Clock* clock)
    : client_app_metadata_(client_app_metadata),
      key_registry_(key_registry),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      scheduler_(scheduler),
      pref_service_(pref_service),
      clock_(clock) {
  // TODO(nohle): Remove when v1 Enrollment is deprecated.
  AddV1UserKeyPairToRegistryIfNecessary();

  gcm_manager_->AddObserver(this);
}

CryptAuthV2EnrollmentManagerImpl::~CryptAuthV2EnrollmentManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthV2EnrollmentManagerImpl::Start() {
  scheduler_->StartEnrollmentScheduling(
      scheduler_weak_ptr_factory_.GetWeakPtr());

  std::string last_enrolled_client_app_metadata_hash = pref_service_->GetString(
      prefs::kCryptAuthLastEnrolledClientAppMetadataHash);
  if (!last_enrolled_client_app_metadata_hash.empty() &&
      GetClientAppMetadataHash() != last_enrolled_client_app_metadata_hash) {
    // Re-enroll if the ClientAppMetadata has changed since the last successful
    // enrollment. NOTE: Do not force an enrollment if the ClientAppMetadata
    // hash has never been set.
    ForceEnrollmentNow(
        cryptauth::InvocationReason::INVOCATION_REASON_SOFTWARE_UPDATE,
        std::nullopt /* session_id */);
  } else if (initial_v1_and_v2_user_key_pairs_disagree_) {
    // If the v1 and v2 user key pairs initially disagreed, force a
    // re-enrollment with the v1 user key pair that replaced the v2 user key
    // pair.
    ForceEnrollmentNow(
        cryptauth::InvocationReason::INVOCATION_REASON_INITIALIZATION,
        std::nullopt /* session_id */);
  } else if (scheduler_->GetLastSuccessfulEnrollmentTime() &&
             (GetUserPublicKey().empty() || GetUserPrivateKey().empty())) {
    // It is possible, though unlikely, that |scheduler_| has previously
    // enrolled successfully but |key_registry_| no longer holds the enrolled
    // keys, for example, if keys are deleted from the key registry or if the
    // persisted key registry pref cannot be parsed due to an encoding change.
    // In this case, force a re-enrollment.
    ForceEnrollmentNow(
        cryptauth::InvocationReason::INVOCATION_REASON_FAILURE_RECOVERY,
        std::nullopt /* session_id */);
  }
}

void CryptAuthV2EnrollmentManagerImpl::ForceEnrollmentNow(
    cryptauth::InvocationReason invocation_reason,
    const std::optional<std::string>& session_id) {
  scheduler_->RequestEnrollment(
      ConvertInvocationReasonV1ToV2(invocation_reason), session_id);
}

bool CryptAuthV2EnrollmentManagerImpl::IsEnrollmentValid() const {
  std::optional<base::Time> last_successful_enrollment_time =
      scheduler_->GetLastSuccessfulEnrollmentTime();

  if (!last_successful_enrollment_time)
    return false;

  if (GetUserPublicKey().empty() || GetUserPrivateKey().empty())
    return false;

  return (clock_->Now() - *last_successful_enrollment_time) <
         scheduler_->GetRefreshPeriod();
}

base::Time CryptAuthV2EnrollmentManagerImpl::GetLastEnrollmentTime() const {
  std::optional<base::Time> last_successful_enrollment_time =
      scheduler_->GetLastSuccessfulEnrollmentTime();

  if (!last_successful_enrollment_time)
    return base::Time();

  return *last_successful_enrollment_time;
}

base::TimeDelta CryptAuthV2EnrollmentManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextEnrollmentRequest().value_or(
      base::TimeDelta::Max());
}

bool CryptAuthV2EnrollmentManagerImpl::IsEnrollmentInProgress() const {
  return scheduler_->IsWaitingForEnrollmentResult();
}

bool CryptAuthV2EnrollmentManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetNumConsecutiveEnrollmentFailures() > 0;
}

std::string CryptAuthV2EnrollmentManagerImpl::GetUserPublicKey() const {
  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);

  // If a v1 key exists, it should have been added to the v2 registry already by
  // AddV1UserKeyPairToRegistryIfNecessary().
  DCHECK(
      GetV1UserPublicKey().empty() ||
      (user_key_pair && user_key_pair->public_key() == GetV1UserPublicKey()));

  if (!user_key_pair)
    return std::string();

  return user_key_pair->public_key();
}

std::string CryptAuthV2EnrollmentManagerImpl::GetUserPrivateKey() const {
  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  std::string private_key_v1 = GetV1UserPrivateKey();

  // If a v1 key exists, it should have been added to the v2 registry already by
  // AddV1UserKeyPairToRegistryIfNecessary().
  DCHECK(
      GetV1UserPrivateKey().empty() ||
      (user_key_pair && user_key_pair->private_key() == GetV1UserPrivateKey()));

  if (!user_key_pair)
    return std::string();

  return user_key_pair->private_key();
}

void CryptAuthV2EnrollmentManagerImpl::OnEnrollmentRequested(
    const cryptauthv2::ClientMetadata& client_metadata,
    const std::optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  NotifyEnrollmentStarted();

  current_client_metadata_ = client_metadata;
  client_directive_policy_reference_ = client_directive_policy_reference;

  base::UmaHistogramExactLinear(
      "CryptAuth.EnrollmentV2.InvocationReason",
      current_client_metadata_->invocation_reason(),
      cryptauthv2::ClientMetadata::InvocationReason_ARRAYSIZE);

  Enroll();
}

void CryptAuthV2EnrollmentManagerImpl::OnReenrollMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {
  ForceEnrollmentNow(cryptauth::INVOCATION_REASON_SERVER_INITIATED, session_id);
}

void CryptAuthV2EnrollmentManagerImpl::Enroll() {
  DCHECK(current_client_metadata_);

  PA_LOG(VERBOSE) << "Starting CryptAuth v2 Enrollment attempt.";
  enroller_ =
      CryptAuthV2EnrollerImpl::Factory::Create(key_registry_, client_factory_);
  enroller_->Enroll(
      *current_client_metadata_, client_app_metadata_,
      client_directive_policy_reference_,
      base::BindOnce(&CryptAuthV2EnrollmentManagerImpl::OnEnrollmentFinished,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthV2EnrollmentManagerImpl::OnEnrollmentFinished(
    const CryptAuthEnrollmentResult& enrollment_result) {
  // Once an enrollment attempt finishes, no other callbacks should be
  // invoked. This is particularly relevant for timeout failures.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();

  // The enrollment result might be owned by the enroller, so we copy the result
  // here before destroying the enroller.
  CryptAuthEnrollmentResult enrollment_result_copy = enrollment_result;
  enroller_.reset();

  if (enrollment_result_copy.IsSuccess()) {
    PA_LOG(INFO) << "Enrollment attempt with invocation reason "
                 << current_client_metadata_->invocation_reason()
                 << " succeeded with result code "
                 << enrollment_result_copy.result_code();
    pref_service_->SetString(prefs::kCryptAuthLastEnrolledClientAppMetadataHash,
                             GetClientAppMetadataHash());
  } else {
    PA_LOG(WARNING) << "Enrollment attempt with invocation reason "
                    << current_client_metadata_->invocation_reason()
                    << " failed with result code "
                    << enrollment_result_copy.result_code();
  }

  current_client_metadata_.reset();

  RecordEnrollmentResult(enrollment_result_copy);

  scheduler_->HandleEnrollmentResult(enrollment_result_copy);

  PA_LOG(INFO) << "Time until next enrollment attempt: "
               << GetTimeToNextAttempt();

  if (!enrollment_result_copy.IsSuccess()) {
    PA_LOG(INFO) << "Number of consecutive Enrollment failures: "
                 << scheduler_->GetNumConsecutiveEnrollmentFailures();
  }

  NotifyEnrollmentFinished(enrollment_result_copy.IsSuccess());
}

std::string CryptAuthV2EnrollmentManagerImpl::GetClientAppMetadataHash() const {
  // NOTE: SerializeAsString() is not guaranteed to be stable; it could change
  // if the protobuf serialization algorithm changes or if the field
  // serialization is inherently nondeterministic. However, because we only have
  // MessageLite protocol buffers in Chrome, MessageDifferencer is not
  // available. So, we either need to compare field-by-field (maintenance heavy)
  // or compare the serializations. We choose the latter and risk a spurious
  // re-enrollment if the serialization algorithm changes. We assume the
  // ClientAppMetadata fields are serialized deterministically; unit tests will
  // fail if they are not.
  return base::NumberToString(
      base::PersistentHash(client_app_metadata_.SerializeAsString()));
}

std::string CryptAuthV2EnrollmentManagerImpl::GetV1UserPublicKey() const {
  std::optional<std::string> public_key = util::DecodeFromValueString(
      &pref_service_->GetValue(prefs::kCryptAuthEnrollmentUserPublicKey));
  if (!public_key) {
    PA_LOG(ERROR) << "Invalid public key stored in user prefs.";
    return std::string();
  }

  return *public_key;
}

std::string CryptAuthV2EnrollmentManagerImpl::GetV1UserPrivateKey() const {
  std::optional<std::string> private_key = util::DecodeFromValueString(
      &pref_service_->GetValue(prefs::kCryptAuthEnrollmentUserPrivateKey));
  if (!private_key) {
    PA_LOG(ERROR) << "Invalid private key stored in user prefs.";
    return std::string();
  }

  return *private_key;
}

void CryptAuthV2EnrollmentManagerImpl::AddV1UserKeyPairToRegistryIfNecessary() {
  std::string public_key_v1 = GetV1UserPublicKey();
  std::string private_key_v1 = GetV1UserPrivateKey();
  const CryptAuthKey* key_v2 =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  UserKeyPairState user_key_pair_state =
      GetUserKeyPairState(public_key_v1, private_key_v1, key_v2);

  base::UmaHistogramEnumeration("CryptAuth.EnrollmentV2.UserKeyPairState",
                                user_key_pair_state);

  initial_v1_and_v2_user_key_pairs_disagree_ =
      user_key_pair_state == UserKeyPairState::kYesV1KeyYesV2KeyDisagree;

  switch (user_key_pair_state) {
    case (UserKeyPairState::kNoV1KeyNoV2Key):
      [[fallthrough]];
    case (UserKeyPairState::kNoV1KeyYesV2Key):
      [[fallthrough]];
    case (UserKeyPairState::kYesV1KeyYesV2KeyAgree):
      return;
    case (UserKeyPairState::kYesV1KeyNoV2Key):
      [[fallthrough]];
    case (UserKeyPairState::kYesV1KeyYesV2KeyDisagree):
      key_registry_->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                            CryptAuthKey(public_key_v1, private_key_v1,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256,
                                         kCryptAuthFixedUserKeyPairHandle));
  };
}

}  // namespace device_sync

}  // namespace ash
