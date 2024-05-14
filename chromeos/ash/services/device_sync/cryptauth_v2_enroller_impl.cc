// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_constants.h"

namespace ash {

namespace device_sync {

namespace {

using cryptauthv2::SyncKeysRequest;
using SyncSingleKeyRequest = cryptauthv2::SyncKeysRequest::SyncSingleKeyRequest;

using cryptauthv2::SyncKeysResponse;
using SyncSingleKeyResponse =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse;
using KeyAction =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse::KeyAction;
using KeyCreation =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse::KeyCreation;

using cryptauthv2::EnrollKeysRequest;
using EnrollSingleKeyRequest =
    cryptauthv2::EnrollKeysRequest::EnrollSingleKeyRequest;

using cryptauthv2::EnrollKeysResponse;
using EnrollSingleKeyResponse =
    cryptauthv2::EnrollKeysResponse::EnrollSingleKeyResponse;

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// these timeout values. For now, set these timeouts to the max execution time
// recorded by the metrics.
constexpr base::TimeDelta kWaitingForSyncKeysResponseTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForKeyCreationTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForEnrollKeysResponseTimeout =
    kMaxAsyncExecutionTime;

CryptAuthEnrollmentResult::ResultCode SyncKeysNetworkRequestErrorToResultCode(
    NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorSyncKeysApiCallUnknownError;
  }
}

CryptAuthEnrollmentResult::ResultCode EnrollKeysNetworkRequestErrorToResultCode(
    NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorEnrollKeysApiCallUnknownError;
  }
}

bool DoesDeviceSoftwarePackageWithExpectedNameExist(
    const google::protobuf::RepeatedPtrField<
        cryptauthv2::ApplicationSpecificMetadata>& app_specific_metadata_list,
    const std::string& expected_name) {
  for (const cryptauthv2::ApplicationSpecificMetadata& metadata :
       app_specific_metadata_list) {
    if (metadata.device_software_package() == expected_name)
      return true;
  }
  return false;
}

// The v2 Enrollment protocol states that the order of the received
// SyncSingleKeyResponses will correspond to the order of the
// SyncSingleKeyRequests. That order is defined here.
const std::vector<CryptAuthKeyBundle::Name>& GetKeyBundleOrder() {
  static const base::NoDestructor<std::vector<CryptAuthKeyBundle::Name>> order(
      [] {
        std::vector<CryptAuthKeyBundle::Name> order;
        for (const CryptAuthKeyBundle::Name& bundle_name :
             CryptAuthKeyBundle::AllEnrollableNames()) {
          order.push_back(bundle_name);
        }
        return order;
      }());

  return *order;
}

CryptAuthKey::Status ConvertKeyCreationToKeyStatus(KeyCreation key_creation) {
  switch (key_creation) {
    case SyncSingleKeyResponse::ACTIVE:
      return CryptAuthKey::Status::kActive;
    case SyncSingleKeyResponse::INACTIVE:
      return CryptAuthKey::Status::kInactive;
    default:
      NOTREACHED_IN_MIGRATION();
      return CryptAuthKey::Status::kInactive;
  }
}

// Return an error code if the SyncKeysResponse is invalid and null otherwise.
std::optional<CryptAuthEnrollmentResult::ResultCode> CheckSyncKeysResponse(
    const SyncKeysResponse& response,
    size_t expected_num_key_responses) {
  if (response.random_session_id().empty()) {
    PA_LOG(ERROR) << "Missing SyncKeysResponse::random_session_id.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorSyncKeysResponseMissingRandomSessionId;
  }

  if (!response.has_client_directive() ||
      response.client_directive().checkin_delay_millis() <= 0 ||
      response.client_directive().retry_attempts() < 0 ||
      response.client_directive().retry_period_millis() <= 0) {
    PA_LOG(ERROR) << "Invalid SyncKeysResponse::client_directive.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorSyncKeysResponseInvalidClientDirective;
  }

  size_t num_single_responses =
      static_cast<size_t>(response.sync_single_key_responses_size());
  if (num_single_responses != expected_num_key_responses) {
    PA_LOG(ERROR) << "Expected " << expected_num_key_responses << " "
                  << "SyncKeysResponse::sync_single_key_responses but "
                  << "received " << num_single_responses << ".";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorWrongNumberOfSyncSingleKeyResponses;
  }

  return std::nullopt;
}

// Given the key actions for the existing keys in the bundle, find the key to
// activate and the keys to delete, setting |handle_to_activate| and
// |handles_to_delete|, respectively. Returns an error code if the key actions
// are invalid and null otherwise.
//
// Note: The v2 Enrollment protocol states, "If the client has at least one
// enrolled key, there must be exactly one ACTIVATE key action (unless the
// server wants to delete all keys currently held by the client). This is
// because there must be exactly one 'active' key after processing these
// actions."
std::optional<CryptAuthEnrollmentResult::ResultCode> ProcessKeyActions(
    const google::protobuf::RepeatedField<int>& key_actions,
    const std::vector<std::string>& handle_order,
    std::optional<std::string>* handle_to_activate,
    std::vector<std::string>* handles_to_delete) {
  // Check that the number of key actions agrees with the number of key
  // handles sent in the SyncSingleKeysRequest.
  if (static_cast<size_t>(key_actions.size()) != handle_order.size()) {
    PA_LOG(ERROR) << "Key bundle has " << handle_order.size() << " keys but "
                  << "SyncSingleKeyResponse::key_actions has size "
                  << key_actions.size();
    return CryptAuthEnrollmentResult::ResultCode::kErrorWrongNumberOfKeyActions;
  }

  // Find all keys that CryptAuth requests be deleted, and find the handle
  // of the key that will be active, if any. Note: The order of the key
  // actions is assumed to agree with the order of the key handles sent in
  // the SyncSingleKeyRequest.
  for (size_t i = 0; i < handle_order.size(); ++i) {
    if (!SyncSingleKeyResponse::KeyAction_IsValid(key_actions[i])) {
      PA_LOG(ERROR) << "Invalid KeyAction enum value " << key_actions[i];
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorInvalidKeyActionEnumValue;
    }

    KeyAction key_action = static_cast<KeyAction>(key_actions[i]);

    if (key_action == SyncSingleKeyResponse::DELETE) {
      handles_to_delete->emplace_back(handle_order[i]);
      continue;
    }

    if (key_action == SyncSingleKeyResponse::ACTIVATE) {
      // There cannot be more than one active handle.
      if (handle_to_activate->has_value()) {
        PA_LOG(ERROR) << "KeyActions specify two active handles: "
                      << **handle_to_activate << " and " << handle_order[i];
        return CryptAuthEnrollmentResult::ResultCode::
            kErrorKeyActionsSpecifyMultipleActiveKeys;
      }

      *handle_to_activate = handle_order[i];
    }
  }

  // The v2 Enrollment protocol states that, unless the server wants to
  // delete all keys currently held by the client, there should be exactly
  // one active key in the key bundle.
  if (!handle_to_activate->has_value() &&
      handles_to_delete->size() != handle_order.size()) {
    PA_LOG(ERROR) << "KeyActions do not specify an active handle.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorKeyActionsDoNotSpecifyAnActiveKey;
  }

  return std::nullopt;
}

bool IsSupportedKeyType(const cryptauthv2::KeyType& key_type) {
  return key_type == cryptauthv2::KeyType::RAW128 ||
         key_type == cryptauthv2::KeyType::RAW256 ||
         key_type == cryptauthv2::KeyType::P256;
}

// The key bundle kUserKeyPair has special standing in order to 1) accommodate
// any existing key from v1 Enrollment and 2) enforce that the key is not
// rotated. As such, only one user key pair should exist in the key bundle, and
// it should be an active, P-256 key with handle
// kCryptAuthFixedUserKeyPairHandle.
//
// It is possible that CryptAuth could request the creation of a new user key
// pair even if the client sends information about an existing key in the
// SyncKeysRequest. If this happens, the client should re-use the existing user
// key pair key material when creating a new key. At the end of the enrollment
// flow, the existing key will be replaced with this new key that has the same
// public/private keys.
//
// Returns an error code if the key-creation instructions are invalid and null
// otherwise.
std::optional<CryptAuthEnrollmentResult::ResultCode>
ProcessNewUserKeyPairInstructions(
    CryptAuthKey::Status status,
    cryptauthv2::KeyType type,
    const CryptAuthKey* current_active_key,
    std::optional<CryptAuthKeyCreator::CreateKeyData>* new_key_to_create) {
  if (type != cryptauthv2::KeyType::P256) {
    PA_LOG(ERROR) << "User key pair must have KeyType P256.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorUserKeyPairCreationInstructionsInvalid;
  }

  // Because no more than one user key pair can exist in the bundle, the newly
  // created key must be active.
  if (status != CryptAuthKey::Status::kActive) {
    PA_LOG(ERROR) << "New user key pair must be active.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorUserKeyPairCreationInstructionsInvalid;
  }

  // If a user key pair already exists in the registry, reuse the same key data.
  if (current_active_key && current_active_key->IsAsymmetricKey() &&
      !current_active_key->private_key().empty()) {
    PA_LOG(WARNING) << "Received request to create new user key pair while one "
                    << "already exists in the key registry. Reusing existing "
                    << "key material.";

    *new_key_to_create = CryptAuthKeyCreator::CreateKeyData(
        status, type, kCryptAuthFixedUserKeyPairHandle,
        current_active_key->public_key(), current_active_key->private_key());

    return std::nullopt;
  }

  // If there is no user key pair in the registry, then the user has never
  // successfully enrolled via v1 or v2 Enrollment. Generate a new key pair.
  *new_key_to_create = CryptAuthKeyCreator::CreateKeyData(
      status, type, kCryptAuthFixedUserKeyPairHandle);

  return std::nullopt;
}

void RecordSyncKeysMetrics(const base::TimeDelta& execution_time,
                           CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric("CryptAuth.EnrollmentV2.ExecutionTime.SyncKeys",
                              execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.EnrollmentV2.ApiCallResult.SyncKeys", result);
}

void RecordKeyCreationMetrics(const base::TimeDelta& execution_time,
                              CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.EnrollmentV2.ExecutionTime.KeyCreation", execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.EnrollmentV2.AsyncTaskResult.KeyCreation", result);
}

void RecordEnrollKeysMetrics(const base::TimeDelta& execution_time,
                             CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric("CryptAuth.EnrollmentV2.ExecutionTime.EnrollKeys",
                              execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.EnrollmentV2.ApiCallResult.EnrollKeys", result);
}

}  // namespace

// static
CryptAuthV2EnrollerImpl::Factory*
    CryptAuthV2EnrollerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthV2Enroller> CryptAuthV2EnrollerImpl::Factory::Create(
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(key_registry, client_factory,
                                         std::move(timer));
  }

  return base::WrapUnique(new CryptAuthV2EnrollerImpl(
      key_registry, client_factory, std::move(timer)));
}

// static
void CryptAuthV2EnrollerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthV2EnrollerImpl::Factory::~Factory() = default;

CryptAuthV2EnrollerImpl::CryptAuthV2EnrollerImpl(
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : key_registry_(key_registry),
      client_factory_(client_factory),
      timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthV2EnrollerImpl::~CryptAuthV2EnrollerImpl() = default;

// static
std::optional<base::TimeDelta> CryptAuthV2EnrollerImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForSyncKeysResponse:
      return kWaitingForSyncKeysResponseTimeout;
    case State::kWaitingForKeyCreation:
      return kWaitingForKeyCreationTimeout;
    case State::kWaitingForEnrollKeysResponse:
      return kWaitingForEnrollKeysResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return std::nullopt;
  }
}

// static
std::optional<CryptAuthEnrollmentResult::ResultCode>
CryptAuthV2EnrollerImpl::ResultCodeErrorFromTimeoutDuringState(State state) {
  switch (state) {
    case State::kWaitingForSyncKeysResponse:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorTimeoutWaitingForSyncKeysResponse;
    case State::kWaitingForKeyCreation:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorTimeoutWaitingForKeyCreation;
    case State::kWaitingForEnrollKeysResponse:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorTimeoutWaitingForEnrollKeysResponse;
    default:
      return std::nullopt;
  }
}

void CryptAuthV2EnrollerImpl::OnAttemptStarted(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    const std::optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  DCHECK(state_ == State::kNotStarted);

  SetState(State::kWaitingForSyncKeysResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->SyncKeys(
      BuildSyncKeysRequest(client_metadata, client_app_metadata,
                           client_directive_policy_reference),
      base::BindOnce(&CryptAuthV2EnrollerImpl::OnSyncKeysSuccess,
                     base::Unretained(this)),
      base::BindOnce(&CryptAuthV2EnrollerImpl::OnSyncKeysFailure,
                     base::Unretained(this)));
}

void CryptAuthV2EnrollerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthV2EnrollerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthV2EnrollerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  std::optional<CryptAuthEnrollmentResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForSyncKeysResponse:
      RecordSyncKeysMetrics(execution_time, CryptAuthApiCallResult::kTimeout);
      break;
    case State::kWaitingForKeyCreation:
      RecordKeyCreationMetrics(execution_time,
                               CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForEnrollKeysResponse:
      RecordEnrollKeysMetrics(execution_time, CryptAuthApiCallResult::kTimeout);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  FinishAttempt(*error_code);
}

SyncKeysRequest CryptAuthV2EnrollerImpl::BuildSyncKeysRequest(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    const std::optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  SyncKeysRequest request;
  request.set_application_name(kCryptAuthGcmAppId);

  // ApplicationSpecificMetadata::device_software_package must agree with
  // the SyncKeysRequest::application_name.
  DCHECK(DoesDeviceSoftwarePackageWithExpectedNameExist(
      client_app_metadata.application_specific_metadata(),
      request.application_name()));

  request.set_client_version(kCryptAuthClientVersion);
  request.mutable_client_metadata()->CopyFrom(client_metadata);
  request.set_client_app_metadata(client_app_metadata.SerializeAsString());

  if (client_directive_policy_reference) {
    request.mutable_policy_reference()->CopyFrom(
        *client_directive_policy_reference);
  }

  // Note: The v2 Enrollment protocol states that the order of the received
  // SyncSingleKeyResponses will correspond to the order of the
  // SyncSingleKeyRequests.
  for (const CryptAuthKeyBundle::Name& bundle_name : GetKeyBundleOrder()) {
    request.add_sync_single_key_requests()->CopyFrom(BuildSyncSingleKeyRequest(
        bundle_name, key_registry_->GetKeyBundle(bundle_name)));
  }

  return request;
}

SyncSingleKeyRequest CryptAuthV2EnrollerImpl::BuildSyncSingleKeyRequest(
    const CryptAuthKeyBundle::Name& bundle_name,
    const CryptAuthKeyBundle* key_bundle) {
  SyncSingleKeyRequest request;
  request.set_key_name(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name));
  // Note: Use of operator[] here adds an entry to the map if no entry currently
  // exists for |bundle_name|. If keys exist in the bundle, the empty handle
  // list will be populated below.
  std::vector<std::string>& handle_order = key_handle_orders_[bundle_name];

  if (!key_bundle)
    return request;

  // Note: The order of key_actions sent in the SyncSingleKeyResponse will
  // align with the order of the handles used here, which we store in
  // |key_handle_orders_|.
  for (const std::pair<std::string, CryptAuthKey>& handle_key_pair :
       key_bundle->handle_to_key_map()) {
    request.add_key_handles(handle_key_pair.first);

    handle_order.emplace_back(handle_key_pair.first);
  }

  if (key_bundle->key_directive() &&
      key_bundle->key_directive()->has_policy_reference()) {
    request.mutable_policy_reference()->CopyFrom(
        key_bundle->key_directive()->policy_reference());
  }

  return request;
}

void CryptAuthV2EnrollerImpl::OnSyncKeysSuccess(
    const SyncKeysResponse& response) {
  DCHECK(state_ == State::kWaitingForSyncKeysResponse);

  RecordSyncKeysMetrics(base::TimeTicks::Now() - last_state_change_timestamp_,
                        CryptAuthApiCallResult::kSuccess);

  if (response.server_status() == SyncKeysResponse::SERVER_OVERLOADED) {
    FinishAttempt(
        CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded);
    return;
  }

  std::optional<CryptAuthEnrollmentResult::ResultCode> error_code =
      CheckSyncKeysResponse(response, GetKeyBundleOrder().size());
  if (error_code) {
    FinishAttempt(*error_code);
    return;
  }

  new_client_directive_ = response.client_directive();

  // Note: The server's Diffie-Hellman public key is only required if symmetric
  // keys need to be created.
  std::optional<CryptAuthKey> server_ephemeral_dh;
  if (!response.server_ephemeral_dh().empty()) {
    server_ephemeral_dh = CryptAuthKey(
        response.server_ephemeral_dh(), std::string() /* private_key */,
        CryptAuthKey::Status::kInactive, cryptauthv2::KeyType::P256);
  }

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      new_keys_to_create;
  base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>
      new_key_directives;
  error_code = ProcessSingleKeyResponses(response, &new_keys_to_create,
                                         &new_key_directives);
  if (error_code) {
    FinishAttempt(*error_code);
    return;
  }

  // If CryptAuth did not request any new keys, the enrollment flow ends here.
  if (new_keys_to_create.empty()) {
    FinishAttempt(
        CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded);
    return;
  }

  SetState(State::kWaitingForKeyCreation);

  key_creator_ = CryptAuthKeyCreatorImpl::Factory::Create();
  key_creator_->CreateKeys(
      new_keys_to_create, server_ephemeral_dh,
      base::BindOnce(&CryptAuthV2EnrollerImpl::OnKeysCreated,
                     base::Unretained(this), response.random_session_id(),
                     new_key_directives));
}

std::optional<CryptAuthEnrollmentResult::ResultCode>
CryptAuthV2EnrollerImpl::ProcessSingleKeyResponses(
    const SyncKeysResponse& sync_keys_response,
    base::flat_map<CryptAuthKeyBundle::Name,
                   CryptAuthKeyCreator::CreateKeyData>* new_keys_to_create,
    base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>*
        new_key_directives) {
  // Starts as null but is overwritten with the ResultCode of the first error,
  // if any errors occur. If an error occurs for a single key bundle, proceed to
  // the next key bundle instead of exiting immediately.
  std::optional<CryptAuthEnrollmentResult::ResultCode> error_code;

  for (size_t i = 0; i < GetKeyBundleOrder().size(); ++i) {
    // Note: The SyncSingleKeyRequests were ordered according to
    // GetKeyBundleOrder(), and the v2 Enrollment protocol specifies that the
    // SyncSingleKeyResponses will obey the same ordering as the requests.
    const SyncSingleKeyResponse& single_response =
        sync_keys_response.sync_single_key_responses(i);
    CryptAuthKeyBundle::Name bundle_name = GetKeyBundleOrder()[i];

    // Apply the key actions.
    // Important Note: The CryptAuth v2 Enrollment specification states, "the
    // key actions ACTIVATE, DEACTIVATE and DELETE should take effect right
    // after the client receives SyncKeysResponse. These actions should not
    // wait for the end of the session, such as receiving a successful
    // EnrollKeysResponse."
    std::optional<std::string> handle_to_activate;
    std::vector<std::string> handles_to_delete;
    std::optional<CryptAuthEnrollmentResult::ResultCode> error_code_actions =
        ProcessKeyActions(single_response.key_actions(),
                          key_handle_orders_[bundle_name], &handle_to_activate,
                          &handles_to_delete);

    // Do not apply the key actions or process the key creation instructions
    // if the key actions are invalid. Proceed to the next key bundle.
    if (error_code_actions) {
      // Set final error code if it hasn't already been set.
      if (!error_code)
        error_code = error_code_actions;

      continue;
    }

    for (const std::string& handle : handles_to_delete)
      key_registry_->DeleteKey(bundle_name, handle);

    if (handle_to_activate)
      key_registry_->SetActiveKey(bundle_name, *handle_to_activate);

    // Process new-key data, if any.
    std::optional<CryptAuthKeyCreator::CreateKeyData> new_key_to_create;
    std::optional<cryptauthv2::KeyDirective> new_key_directive;
    std::optional<CryptAuthEnrollmentResult::ResultCode> error_code_creation =
        ProcessKeyCreationInstructions(bundle_name, single_response,
                                       sync_keys_response.server_ephemeral_dh(),
                                       &new_key_to_create, &new_key_directive);

    // If the key-creation instructions are invalid, do not add to the list of
    // keys to be created. Proceed to the next key bundle.
    if (error_code_creation) {
      // Set final error code if it hasn't already been set.
      if (!error_code)
        error_code = error_code_creation;

      continue;
    }

    if (new_key_to_create)
      new_keys_to_create->insert_or_assign(bundle_name, *new_key_to_create);

    if (new_key_directive)
      new_key_directives->insert_or_assign(bundle_name, *new_key_directive);
  }

  return error_code;
}

std::optional<CryptAuthEnrollmentResult::ResultCode>
CryptAuthV2EnrollerImpl::ProcessKeyCreationInstructions(
    const CryptAuthKeyBundle::Name& bundle_name,
    const SyncSingleKeyResponse& single_key_response,
    const std::string& server_ephemeral_dh,
    std::optional<CryptAuthKeyCreator::CreateKeyData>* new_key_to_create,
    std::optional<cryptauthv2::KeyDirective>* new_key_directive) {
  if (single_key_response.key_creation() == SyncSingleKeyResponse::NONE)
    return std::nullopt;

  CryptAuthKey::Status status =
      ConvertKeyCreationToKeyStatus(single_key_response.key_creation());
  cryptauthv2::KeyType type = single_key_response.key_type();

  if (!IsSupportedKeyType(type)) {
    PA_LOG(ERROR) << "KeyType " << type << " not supported.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorKeyCreationKeyTypeNotSupported;
  }

  // Symmetric keys cannot be created without the server's Diffie-Hellman key.
  if (server_ephemeral_dh.empty() && (type == cryptauthv2::KeyType::RAW128 ||
                                      type == cryptauthv2::KeyType::RAW256)) {
    PA_LOG(ERROR)
        << "Missing server's Diffie-Hellman key. Cannot create symmetric keys.";
    return CryptAuthEnrollmentResult::ResultCode::
        kErrorSymmetricKeyCreationMissingServerDiffieHellman;
  }

  if (single_key_response.has_key_directive())
    *new_key_directive = single_key_response.key_directive();

  // Handle the user key pair special case separately below.
  if (bundle_name != CryptAuthKeyBundle::Name::kUserKeyPair) {
    *new_key_to_create = CryptAuthKeyCreator::CreateKeyData(status, type);

    return std::nullopt;
  }

  DCHECK(bundle_name == CryptAuthKeyBundle::Name::kUserKeyPair);
  return ProcessNewUserKeyPairInstructions(
      status, type, key_registry_->GetActiveKey(bundle_name),
      new_key_to_create);
}

void CryptAuthV2EnrollerImpl::OnSyncKeysFailure(NetworkRequestError error) {
  RecordSyncKeysMetrics(base::TimeTicks::Now() - last_state_change_timestamp_,
                        CryptAuthApiCallResultFromNetworkRequestError(error));

  FinishAttempt(SyncKeysNetworkRequestErrorToResultCode(error));
}

void CryptAuthV2EnrollerImpl::OnKeysCreated(
    const std::string& session_id,
    const base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>&
        new_key_directives,
    const base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
        new_keys,
    const std::optional<CryptAuthKey>& client_ephemeral_dh) {
  DCHECK(state_ == State::kWaitingForKeyCreation);

  RecordKeyCreationMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthAsyncTaskResult::kSuccess);

  EnrollKeysRequest request;
  request.set_random_session_id(session_id);
  if (client_ephemeral_dh)
    request.set_client_ephemeral_dh(client_ephemeral_dh->public_key());

  std::unique_ptr<CryptAuthKeyProofComputer> key_proof_computer =
      CryptAuthKeyProofComputerImpl::Factory::Create();

  for (const std::pair<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
           name_key_pair : new_keys) {
    if (!name_key_pair.second) {
      CryptAuthEnrollmentResult::ResultCode result_code;
      switch (name_key_pair.first) {
        case CryptAuthKeyBundle::Name::kUserKeyPair:
          result_code = CryptAuthEnrollmentResult::ResultCode::
              kErrorUserKeyPairCreationFailed;
          break;
        case CryptAuthKeyBundle::Name::kLegacyAuthzenKey:
          result_code = CryptAuthEnrollmentResult::ResultCode::
              kErrorLegacyAuthzenKeyCreationFailed;
          break;
        case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether:
          result_code = CryptAuthEnrollmentResult::ResultCode::
              kErrorDeviceSyncBetterTogetherKeyCreationFailed;
          break;
        case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey:
          NOTREACHED_IN_MIGRATION();
          result_code = CryptAuthEnrollmentResult::ResultCode::
              kErrorUserKeyPairCreationFailed;
          break;
      }
      FinishAttempt(result_code);
      return;
    }

    const CryptAuthKeyBundle::Name& bundle_name = name_key_pair.first;
    const CryptAuthKey& new_key = *name_key_pair.second;

    std::string bundle_name_str =
        CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name);

    EnrollSingleKeyRequest* single_key_request =
        request.add_enroll_single_key_requests();
    single_key_request->set_key_name(bundle_name_str);
    single_key_request->set_new_key_handle(new_key.handle());
    if (new_key.IsAsymmetricKey())
      single_key_request->set_key_material(new_key.public_key());

    // Compute key proofs for the new keys using the random_session_id from the
    // SyncKeysResponse as the payload and the particular salt specified by the
    // v2 Enrollment protocol.
    std::optional<std::string> key_proof = key_proof_computer->ComputeKeyProof(
        new_key, session_id, kCryptAuthKeyProofSalt, bundle_name_str);
    if (!key_proof || key_proof->empty()) {
      FinishAttempt(CryptAuthEnrollmentResult::ResultCode::
                        kErrorKeyProofComputationFailed);
      return;
    }

    single_key_request->set_key_proof(*key_proof);
  }

  SetState(State::kWaitingForEnrollKeysResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->EnrollKeys(
      request,
      base::BindOnce(&CryptAuthV2EnrollerImpl::OnEnrollKeysSuccess,
                     base::Unretained(this), new_key_directives, new_keys),
      base::BindOnce(&CryptAuthV2EnrollerImpl::OnEnrollKeysFailure,
                     base::Unretained(this)));
}

void CryptAuthV2EnrollerImpl::OnEnrollKeysSuccess(
    const base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>&
        new_key_directives,
    const base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
        new_keys,
    const EnrollKeysResponse& response) {
  DCHECK(state_ == State::kWaitingForEnrollKeysResponse);

  RecordEnrollKeysMetrics(base::TimeTicks::Now() - last_state_change_timestamp_,
                          CryptAuthApiCallResult::kSuccess);

  for (const std::pair<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
           new_key : new_keys) {
    DCHECK(new_key.second);
    key_registry_->AddKey(new_key.first, *new_key.second);
  }

  for (const std::pair<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>&
           new_key_directive : new_key_directives) {
    key_registry_->SetKeyDirective(new_key_directive.first,
                                   new_key_directive.second);
  }

  FinishAttempt(CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled);
}

void CryptAuthV2EnrollerImpl::OnEnrollKeysFailure(NetworkRequestError error) {
  RecordEnrollKeysMetrics(base::TimeTicks::Now() - last_state_change_timestamp_,
                          CryptAuthApiCallResultFromNetworkRequestError(error));

  FinishAttempt(EnrollKeysNetworkRequestErrorToResultCode(error));
}

void CryptAuthV2EnrollerImpl::FinishAttempt(
    CryptAuthEnrollmentResult::ResultCode result_code) {
  SetState(State::kFinished);

  OnAttemptFinished(
      CryptAuthEnrollmentResult(result_code, new_client_directive_));
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthV2EnrollerImpl::State& state) {
  switch (state) {
    case CryptAuthV2EnrollerImpl::State::kNotStarted:
      stream << "[Enroller state: Not started]";
      break;
    case CryptAuthV2EnrollerImpl::State::kWaitingForSyncKeysResponse:
      stream << "[Enroller state: Waiting for SyncKeys response]";
      break;
    case CryptAuthV2EnrollerImpl::State::kWaitingForKeyCreation:
      stream << "[Enroller state: Waiting for key creation]";
      break;
    case CryptAuthV2EnrollerImpl::State::kWaitingForEnrollKeysResponse:
      stream << "[Enroller state: Waiting for EnrollKeys response]";
      break;
    case CryptAuthV2EnrollerImpl::State::kFinished:
      stream << "[Enroller state: Finished]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace ash
