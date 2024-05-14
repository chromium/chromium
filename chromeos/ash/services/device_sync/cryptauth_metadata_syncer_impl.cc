// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace device_sync {

namespace {

const char kUnsetPrefValue[] = "[Unset pref value]";

const cryptauthv2::KeyType kGroupKeyType = cryptauthv2::KeyType::P256;

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// these timeout values. For now, set these timeouts to the max execution time
// recorded by the metrics.
constexpr base::TimeDelta kWaitingForGroupKeyCreationTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForLocalDeviceMetadataEncryptionTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForFirstSyncMetadataResponseTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForSecondSyncMetadataResponseTimeout =
    kMaxAsyncExecutionTime;

CryptAuthDeviceSyncResult::ResultCode
SyncMetadataNetworkRequestErrorToResultCode(NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallUnknownError;
  }
}

void RecordGroupKeyCreationMetrics(const base::TimeDelta& execution_time,
                                   CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ExecutionTime.GroupKeyCreation",
      execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.AsyncTaskResult.GroupKeyCreation",
      result);
}

void RecordLocalDeviceMetadataEncryptionMetrics(
    const base::TimeDelta& execution_time,
    CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ExecutionTime."
      "LocalDeviceMetadataEncryption",
      execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.AsyncTaskResult."
      "LocalDeviceMetadataEncryption",
      result);
}

void RecordFirstSyncMetadataMetrics(const base::TimeDelta& execution_time,
                                    CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ExecutionTime.FirstSyncMetadata",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ApiCallResult.FirstSyncMetadata",
      result);
}

void RecordSecondSyncMetadataMetrics(const base::TimeDelta& execution_time,
                                     CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ExecutionTime.SecondSyncMetadata",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.MetadataSyncer.ApiCallResult.SecondSyncMetadata",
      result);
}

}  // namespace

// static
CryptAuthMetadataSyncerImpl::Factory*
    CryptAuthMetadataSyncerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthMetadataSyncer>
CryptAuthMetadataSyncerImpl::Factory::Create(
    CryptAuthClientFactory* client_factory,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(client_factory, pref_service,
                                         std::move(timer));
  }

  return base::WrapUnique(new CryptAuthMetadataSyncerImpl(
      client_factory, pref_service, std::move(timer)));
}

// static
void CryptAuthMetadataSyncerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthMetadataSyncerImpl::Factory::~Factory() = default;

// static
void CryptAuthMetadataSyncerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata, kUnsetPrefValue);
  registry->RegisterStringPref(
      prefs::kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata,
      kUnsetPrefValue);
  registry->RegisterStringPref(prefs::kCryptAuthLastSyncedGroupPublicKey,
                               kUnsetPrefValue);
}

// static
std::optional<base::TimeDelta> CryptAuthMetadataSyncerImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForGroupKeyCreation:
      return kWaitingForGroupKeyCreationTimeout;
    case State::kWaitingForLocalDeviceMetadataEncryption:
      return kWaitingForLocalDeviceMetadataEncryptionTimeout;
    case State::kWaitingForFirstSyncMetadataResponse:
      return kWaitingForFirstSyncMetadataResponseTimeout;
    case State::kWaitingForSecondSyncMetadataResponse:
      return kWaitingForSecondSyncMetadataResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return std::nullopt;
  }
}

// static
std::optional<CryptAuthDeviceSyncResult::ResultCode>
CryptAuthMetadataSyncerImpl::ResultCodeErrorFromTimeoutDuringState(
    State state) {
  switch (state) {
    case State::kWaitingForGroupKeyCreation:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupKeyCreation;
    case State::kWaitingForLocalDeviceMetadataEncryption:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForLocalDeviceMetadataEncryption;
    case State::kWaitingForFirstSyncMetadataResponse:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForFirstSyncMetadataResponse;
    case State::kWaitingForSecondSyncMetadataResponse:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForSecondSyncMetadataResponse;
    default:
      return std::nullopt;
  }
}

CryptAuthMetadataSyncerImpl::CryptAuthMetadataSyncerImpl(
    CryptAuthClientFactory* client_factory,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory),
      pref_service_(pref_service),
      timer_(std::move(timer)) {
  DCHECK(client_factory);
  DCHECK(pref_service);
}

CryptAuthMetadataSyncerImpl::~CryptAuthMetadataSyncerImpl() = default;

void CryptAuthMetadataSyncerImpl::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
    const CryptAuthKey* initial_group_key) {
  DCHECK_EQ(State::kNotStarted, state_);

  request_context_ = request_context;
  local_device_metadata_ = local_device_metadata;
  initial_group_key_ = initial_group_key;

  AttemptNextStep();
}

void CryptAuthMetadataSyncerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthMetadataSyncerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthMetadataSyncerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  std::optional<CryptAuthDeviceSyncResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForGroupKeyCreation:
      RecordGroupKeyCreationMetrics(execution_time,
                                    CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForLocalDeviceMetadataEncryption:
      RecordLocalDeviceMetadataEncryptionMetrics(
          execution_time, CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForFirstSyncMetadataResponse:
      RecordFirstSyncMetadataMetrics(execution_time,
                                     CryptAuthApiCallResult::kTimeout);
      break;
    case State::kWaitingForSecondSyncMetadataResponse:
      RecordSecondSyncMetadataMetrics(execution_time,
                                      CryptAuthApiCallResult::kTimeout);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  FinishAttempt(*error_code);
}

const CryptAuthKey* CryptAuthMetadataSyncerImpl::GetGroupKey() {
  if (new_group_key_)
    return new_group_key_.get();

  return initial_group_key_;
}

CryptAuthMetadataSyncerImpl::GroupPublicKeyState
CryptAuthMetadataSyncerImpl::GetGroupPublicKeyState() {
  const CryptAuthKey* group_key = GetGroupKey();

  if (!group_key)
    return GroupPublicKeyState::kNewKeyNeedsToBeCreated;

  if (!sync_metadata_response_)
    return GroupPublicKeyState::kKeyExistsButNotConfirmedWithCryptAuth;

  if (sync_metadata_response_->group_public_key().empty())
    return GroupPublicKeyState::kNewKeyNeedsToBeCreated;

  if (group_key->public_key() != sync_metadata_response_->group_public_key())
    return GroupPublicKeyState::kNewKeyReceivedFromCryptAuth;

  return GroupPublicKeyState::kEstablished;
}

void CryptAuthMetadataSyncerImpl::AttemptNextStep() {
  switch (state_) {
    // Start the flow.
    case State::kNotStarted: {
      GroupPublicKeyState group_public_key_state = GetGroupPublicKeyState();
      PA_LOG(VERBOSE) << "Group public key state: " << group_public_key_state;
      switch (group_public_key_state) {
        case GroupPublicKeyState::kNewKeyNeedsToBeCreated:
          CreateGroupKey();
          return;
        case GroupPublicKeyState::kKeyExistsButNotConfirmedWithCryptAuth:
          EncryptLocalDeviceMetadata();
          return;
        default:
          NOTREACHED_IN_MIGRATION();
          return;
      }
    }

    // After group key creation, encrypt the local device metadata.
    case State::kWaitingForGroupKeyCreation:
      EncryptLocalDeviceMetadata();
      return;

    // After local device metadata is encrypted, start constructing the
    // SyncMetadata call.
    case State::kWaitingForLocalDeviceMetadataEncryption:
      MakeSyncMetadataCall();
      return;

    // After receiving the first SyncMetadata response, take further action
    // based on the state of the group public key.
    case State::kWaitingForFirstSyncMetadataResponse: {
      GroupPublicKeyState group_public_key_state = GetGroupPublicKeyState();
      PA_LOG(VERBOSE) << "Group public key state: " << group_public_key_state;
      switch (group_public_key_state) {
        case GroupPublicKeyState::kNewKeyNeedsToBeCreated:
          CreateGroupKey();
          return;
        case GroupPublicKeyState::kNewKeyReceivedFromCryptAuth:
          new_group_key_ = std::make_unique<CryptAuthKey>(
              sync_metadata_response_->group_public_key(),
              std::string() /* private_key */, CryptAuthKey::Status::kActive,
              kGroupKeyType);
          EncryptLocalDeviceMetadata();
          return;
        case GroupPublicKeyState::kEstablished:
          FilterMetadataAndFinishAttempt();
          return;
        default:
          NOTREACHED_IN_MIGRATION();
          return;
      }
    }

    // After receiving the second SyncMetadata response, process the metadata
    // and finish. Note: In the v2 DeviceSync protocol, no more than two
    // SyncMetadata requests should be necessary to establish the group public
    // key.
    case State::kWaitingForSecondSyncMetadataResponse: {
      GroupPublicKeyState group_public_key_state = GetGroupPublicKeyState();
      PA_LOG(VERBOSE) << "Group public key state: " << group_public_key_state;
      switch (group_public_key_state) {
        case GroupPublicKeyState::kEstablished:
          FilterMetadataAndFinishAttempt();
          return;
        default:
          FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                            kErrorEstablishingGroupPublicKey);
          return;
      }
    }

    // Each CryptAuthMetadataSyncer object can only be used once.
    case State::kFinished:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

bool CryptAuthMetadataSyncerImpl::
    ShouldUseCachedEncryptedLocalDeviceMetadata() {
  std::optional<std::string> last_synced_unencrypted_metadata =
      util::DecodeFromString(pref_service_->GetString(
          prefs::kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata));
  std::optional<std::string> last_synced_group_public_key =
      util::DecodeFromString(
          pref_service_->GetString(prefs::kCryptAuthLastSyncedGroupPublicKey));
  std::optional<std::string> last_synced_encrypted_metadata =
      util::DecodeFromString(pref_service_->GetString(
          prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata));

  // Persisted values are not encoded properly.
  if (!last_synced_unencrypted_metadata || !last_synced_group_public_key ||
      !last_synced_encrypted_metadata) {
    return false;
  }

  // Prefs should be all set or all unset.
  DCHECK_EQ(last_synced_unencrypted_metadata == kUnsetPrefValue,
            last_synced_group_public_key == kUnsetPrefValue);
  DCHECK_EQ(last_synced_unencrypted_metadata == kUnsetPrefValue,
            last_synced_encrypted_metadata == kUnsetPrefValue);

  if (last_synced_unencrypted_metadata == kUnsetPrefValue)
    return false;

  return last_synced_unencrypted_metadata ==
             local_device_metadata_.SerializeAsString() &&
         last_synced_group_public_key == GetGroupKey()->public_key();
}

void CryptAuthMetadataSyncerImpl::EncryptLocalDeviceMetadata() {
  SetState(State::kWaitingForLocalDeviceMetadataEncryption);

  if (ShouldUseCachedEncryptedLocalDeviceMetadata()) {
    OnLocalDeviceMetadataEncrypted(
        *util::DecodeFromString(pref_service_->GetString(
            prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata)));
    return;
  }

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
  encryptor_->Encrypt(
      local_device_metadata_.SerializeAsString(), GetGroupKey()->public_key(),
      base::BindOnce(
          &CryptAuthMetadataSyncerImpl::OnLocalDeviceMetadataEncrypted,
          base::Unretained(this)));
}

void CryptAuthMetadataSyncerImpl::OnLocalDeviceMetadataEncrypted(
    const std::optional<std::string>& encrypted_metadata) {
  DCHECK_EQ(State::kWaitingForLocalDeviceMetadataEncryption, state_);

  bool success = encrypted_metadata.has_value();
  RecordLocalDeviceMetadataEncryptionMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      success ? CryptAuthAsyncTaskResult::kSuccess
              : CryptAuthAsyncTaskResult::kError);

  if (!success) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingDeviceMetadata);
    return;
  }

  encrypted_local_device_metadata_ = encrypted_metadata;

  AttemptNextStep();
}

void CryptAuthMetadataSyncerImpl::CreateGroupKey() {
  SetState(State::kWaitingForGroupKeyCreation);

  key_creator_ = CryptAuthKeyCreatorImpl::Factory::Create();
  key_creator_->CreateKeys(
      {{CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey,
        CryptAuthKeyCreator::CreateKeyData(CryptAuthKey::Status::kActive,
                                           kGroupKeyType)}},
      std::nullopt /* server_ephemeral_dh */,
      base::BindOnce(&CryptAuthMetadataSyncerImpl::OnGroupKeyCreated,
                     base::Unretained(this)));
}

void CryptAuthMetadataSyncerImpl::OnGroupKeyCreated(
    const base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
        new_keys,
    const std::optional<CryptAuthKey>& client_ephemeral_dh) {
  DCHECK_EQ(State::kWaitingForGroupKeyCreation, state_);

  const auto it = new_keys.find(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
  DCHECK(it != new_keys.end());

  bool success = it->second.has_value();
  RecordGroupKeyCreationMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      success ? CryptAuthAsyncTaskResult::kSuccess
              : CryptAuthAsyncTaskResult::kError);

  if (!success) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorCreatingGroupKey);
    return;
  }

  new_group_key_ = std::make_unique<CryptAuthKey>(*it->second);

  AttemptNextStep();
}

void CryptAuthMetadataSyncerImpl::MakeSyncMetadataCall() {
  DCHECK(encrypted_local_device_metadata_);

  const CryptAuthKey* group_key = GetGroupKey();
  DCHECK(group_key);

  cryptauthv2::SyncMetadataRequest request;
  request.mutable_context()->CopyFrom(request_context_);
  request.set_group_public_key(group_key->public_key());
  request.set_encrypted_metadata(*encrypted_local_device_metadata_);
  request.set_need_group_private_key(group_key->private_key().empty());

  ++num_sync_metadata_calls_;
  switch (num_sync_metadata_calls_) {
    case 1:
      SetState(State::kWaitingForFirstSyncMetadataResponse);
      break;
    case 2:
      SetState(State::kWaitingForSecondSyncMetadataResponse);
      break;
    default:
      // AttemptNextStep() ensures that no more than two calls are made.
      NOTREACHED_IN_MIGRATION();
      return;
  }

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->SyncMetadata(
      request,
      base::BindOnce(&CryptAuthMetadataSyncerImpl::OnSyncMetadataSuccess,
                     base::Unretained(this)),
      base::BindOnce(&CryptAuthMetadataSyncerImpl::OnSyncMetadataFailure,
                     base::Unretained(this)));
}

void CryptAuthMetadataSyncerImpl::OnSyncMetadataSuccess(
    const cryptauthv2::SyncMetadataResponse& response) {
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  if (state_ == State::kWaitingForFirstSyncMetadataResponse)
    RecordFirstSyncMetadataMetrics(execution_time,
                                   CryptAuthApiCallResult::kSuccess);
  else if (state_ == State::kWaitingForSecondSyncMetadataResponse)
    RecordSecondSyncMetadataMetrics(execution_time,
                                    CryptAuthApiCallResult::kSuccess);
  else
    NOTREACHED_IN_MIGRATION();

  PA_LOG(VERBOSE) << "SyncMetadata response:\n" << response;

  // Cache encrypted and unencrypted local device metadata, along with the group
  // public key used to encrypt the data, that was successfully sent in the
  // SyncMetadata request. Note: the cached group public key might not match
  // the key returned in the respone.
  pref_service_->SetString(
      prefs::kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata,
      util::EncodeAsString(local_device_metadata_.SerializeAsString()));
  pref_service_->SetString(prefs::kCryptAuthLastSyncedGroupPublicKey,
                           util::EncodeAsString(GetGroupKey()->public_key()));
  pref_service_->SetString(
      prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata,
      util::EncodeAsString(*encrypted_local_device_metadata_));

  sync_metadata_response_ = response;

  AttemptNextStep();
}

void CryptAuthMetadataSyncerImpl::OnSyncMetadataFailure(
    NetworkRequestError error) {
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  if (state_ == State::kWaitingForFirstSyncMetadataResponse)
    RecordFirstSyncMetadataMetrics(
        execution_time, CryptAuthApiCallResultFromNetworkRequestError(error));
  else if (state_ == State::kWaitingForSecondSyncMetadataResponse)
    RecordSecondSyncMetadataMetrics(
        execution_time, CryptAuthApiCallResultFromNetworkRequestError(error));
  else
    NOTREACHED_IN_MIGRATION();

  FinishAttempt(SyncMetadataNetworkRequestErrorToResultCode(error));
}

void CryptAuthMetadataSyncerImpl::FilterMetadataAndFinishAttempt() {
  DCHECK_EQ(GroupPublicKeyState::kEstablished, GetGroupPublicKeyState());
  DCHECK(sync_metadata_response_);

  // At minimum, the local device's metadata should be present in the
  // SyncMetadataResponse.
  if (sync_metadata_response_->encrypted_metadata().empty()) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorNoMetadataInResponse);
    return;
  }

  bool did_non_fatal_error_occur = false;
  for (const cryptauthv2::DeviceMetadataPacket& metadata :
       sync_metadata_response_->encrypted_metadata()) {
    bool is_device_metadata_packet_valid =
        !metadata.device_id().empty() && !metadata.device_name().empty() &&
        !metadata.device_public_key().empty();
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.MetadataSyncer.IsDeviceMetadataPacketValid",
        is_device_metadata_packet_valid);
    if (!is_device_metadata_packet_valid) {
      PA_LOG(ERROR) << "Invalid DeviceMetadataPacket: device_id = "
                    << metadata.device_id() << ", device_public_key = "
                    << util::EncodeAsString(metadata.device_public_key())
                    << ", device_name empty? "
                    << (metadata.device_name().empty() ? "yes" : "no") << ".";
      did_non_fatal_error_occur = true;
      continue;
    }

    bool is_duplicate_id =
        base::Contains(id_to_device_metadata_packet_map_, metadata.device_id());
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.MetadataSyncer.IsDuplicateDeviceId",
        is_duplicate_id);
    if (is_duplicate_id) {
      PA_LOG(ERROR) << "Duplicate device IDs (" << metadata.device_id()
                    << ") in SyncMetadata response.";
      did_non_fatal_error_occur = true;
      continue;
    }

    id_to_device_metadata_packet_map_[metadata.device_id()] = metadata;
  }

  // Finish attempt if DeviceMetadataPackets were sent but none were valid.
  if (id_to_device_metadata_packet_map_.empty()) {
    FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                      kErrorAllResponseMetadataInvalid);
    return;
  }

  // We require that the local device's metadata is returned in the response.
  if (!base::Contains(id_to_device_metadata_packet_map_,
                      request_context_.device_id())) {
    PA_LOG(ERROR) << "Metadata for local device (Instance ID: "
                  << request_context_.device_id()
                  << ") not in SyncMetadata response.";
    FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                      kErrorNoLocalDeviceMetadataInResponse);
    return;
  }

  CryptAuthDeviceSyncResult::ResultCode result_code =
      did_non_fatal_error_occur
          ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
          : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
  FinishAttempt(result_code);
}

void CryptAuthMetadataSyncerImpl::FinishAttempt(
    CryptAuthDeviceSyncResult::ResultCode result_code) {
  cryptauth_client_.reset();
  key_creator_.reset();
  encryptor_.reset();

  std::optional<cryptauthv2::ClientDirective> new_client_directive;
  std::optional<cryptauthv2::EncryptedGroupPrivateKey>
      encrypted_group_private_key;
  if (sync_metadata_response_) {
    if (sync_metadata_response_->has_client_directive())
      new_client_directive = sync_metadata_response_->client_directive();

    if (sync_metadata_response_->has_encrypted_group_private_key()) {
      encrypted_group_private_key =
          sync_metadata_response_->encrypted_group_private_key();
    }
  }

  SetState(State::kFinished);

  OnAttemptFinished(id_to_device_metadata_packet_map_,
                    std::move(new_group_key_), encrypted_group_private_key,
                    new_client_directive, result_code);
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthMetadataSyncerImpl::State& state) {
  switch (state) {
    case CryptAuthMetadataSyncerImpl::State::kNotStarted:
      stream << "[MetadataSyncer state: Not started]";
      break;
    case CryptAuthMetadataSyncerImpl::State::kWaitingForGroupKeyCreation:
      stream << "[MetadataSyncer state: Waiting for group key pair creation]";
      break;
    case CryptAuthMetadataSyncerImpl::State::
        kWaitingForLocalDeviceMetadataEncryption:
      stream << "[MetadataSyncer state: Waiting for local device metadata "
             << "encryption]";
      break;
    case CryptAuthMetadataSyncerImpl::State::
        kWaitingForFirstSyncMetadataResponse:
      stream << "[MetadataSyncer state: Waiting for first SyncMetadata "
             << "response]";
      break;
    case CryptAuthMetadataSyncerImpl::State::
        kWaitingForSecondSyncMetadataResponse:
      stream << "[MetadataSyncer state: Waiting for second SyncMetadata "
             << "response]";
      break;
    case CryptAuthMetadataSyncerImpl::State::kFinished:
      stream << "[MetadataSyncer state: Finished]";
      break;
  }

  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthMetadataSyncerImpl::GroupPublicKeyState& key_state) {
  switch (key_state) {
    case CryptAuthMetadataSyncerImpl::GroupPublicKeyState::kUndetermined:
      stream << "[Undetermined]";
      break;
    case CryptAuthMetadataSyncerImpl::GroupPublicKeyState::
        kKeyExistsButNotConfirmedWithCryptAuth:
      stream << "[Key exists but not confirmed with CryptAuth]";
      break;
    case CryptAuthMetadataSyncerImpl::GroupPublicKeyState::
        kNewKeyNeedsToBeCreated:
      stream << "[New key needs to be created]";
      break;
    case CryptAuthMetadataSyncerImpl::GroupPublicKeyState::
        kNewKeyReceivedFromCryptAuth:
      stream << "[New key received from CryptAuth]";
      break;
    case CryptAuthMetadataSyncerImpl::GroupPublicKeyState::kEstablished:
      stream << "[Established]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace ash
