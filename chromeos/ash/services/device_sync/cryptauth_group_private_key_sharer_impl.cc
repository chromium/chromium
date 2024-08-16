// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "crypto/sha2.h"

namespace ash {

namespace device_sync {

namespace {

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// these timeout values. For now, set these timeouts to the max execution time
// recorded by the metrics.
constexpr base::TimeDelta kWaitingForGroupPrivateKeyEncryptionTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForShareGroupPrivateKeyResponseTimeout =
    kMaxAsyncExecutionTime;

CryptAuthDeviceSyncResult::ResultCode
ShareGroupPrivateKeyNetworkRequestErrorToResultCode(NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallUnknownError;
  }
}

// The first 8 bytes of the SHA-256 hash of |str|, converted into a 64-bit
// signed integer in little-endian order. This format is chosen to be consistent
// with the CryptAuth backend implementation.
int64_t CalculateInt64Sha256Hash(const std::string& str) {
  uint8_t hash_bytes[sizeof(int64_t)];
  crypto::SHA256HashString(str, hash_bytes, sizeof(hash_bytes));

  int64_t hash_int64 = 0;
  for (size_t i = 0; i < 8u; ++i)
    hash_int64 |= static_cast<int64_t>(hash_bytes[i]) << (i * 8);

  return hash_int64;
}

void RecordGroupPrivateKeyEncryptionMetrics(
    const base::TimeDelta& execution_time,
    CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.ExecutionTime."
      "GroupPrivateKeyEncryption",
      execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.AsyncTaskResult."
      "GroupPrivateKeyEncryption",
      result);
}

void RecordShareGroupPrivateKeyMetrics(const base::TimeDelta& execution_time,
                                       CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.ExecutionTime."
      "ShareGroupPrivateKey",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.ApiCallResult."
      "ShareGroupPrivateKey",
      result);
}

}  // namespace

// static
CryptAuthGroupPrivateKeySharerImpl::Factory*
    CryptAuthGroupPrivateKeySharerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthGroupPrivateKeySharer>
CryptAuthGroupPrivateKeySharerImpl::Factory::Create(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_)
    return test_factory_->CreateInstance(client_factory, std::move(timer));

  return base::WrapUnique(
      new CryptAuthGroupPrivateKeySharerImpl(client_factory, std::move(timer)));
}

// static
void CryptAuthGroupPrivateKeySharerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthGroupPrivateKeySharerImpl::Factory::~Factory() = default;

CryptAuthGroupPrivateKeySharerImpl::CryptAuthGroupPrivateKeySharerImpl(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory), timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthGroupPrivateKeySharerImpl::~CryptAuthGroupPrivateKeySharerImpl() =
    default;

// static
std::optional<base::TimeDelta>
CryptAuthGroupPrivateKeySharerImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForGroupPrivateKeyEncryption:
      return kWaitingForGroupPrivateKeyEncryptionTimeout;
    case State::kWaitingForShareGroupPrivateKeyResponse:
      return kWaitingForShareGroupPrivateKeyResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return std::nullopt;
  }
}

// static
std::optional<CryptAuthDeviceSyncResult::ResultCode>
CryptAuthGroupPrivateKeySharerImpl::ResultCodeErrorFromTimeoutDuringState(
    State state) {
  switch (state) {
    case State::kWaitingForGroupPrivateKeyEncryption:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupPrivateKeyEncryption;
    case State::kWaitingForShareGroupPrivateKeyResponse:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForShareGroupPrivateKeyResponse;
    default:
      return std::nullopt;
  }
}

void CryptAuthGroupPrivateKeySharerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthGroupPrivateKeySharerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthGroupPrivateKeySharerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  std::optional<CryptAuthDeviceSyncResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForGroupPrivateKeyEncryption:
      RecordGroupPrivateKeyEncryptionMetrics(
          execution_time, CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForShareGroupPrivateKeyResponse:
      RecordShareGroupPrivateKeyMetrics(execution_time,
                                        CryptAuthApiCallResult::kTimeout);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  FinishAttempt(*error_code);
}

void CryptAuthGroupPrivateKeySharerImpl::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const IdToEncryptingKeyMap& id_to_encrypting_key_map) {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK(!group_key.private_key().empty());

  CryptAuthEciesEncryptor::IdToInputMap group_private_keys_to_encrypt;
  for (const auto& id_encrypting_key_pair : id_to_encrypting_key_map) {
    const std::string& id = id_encrypting_key_pair.first;
    const std::string& encrypting_key = id_encrypting_key_pair.second;

    // If the encrypting key is empty, the group private key cannot be
    // encrypted. Skip this ID and attempt to encrypt the group private key for
    // as many IDs as possible.
    bool is_encrypting_key_empty = encrypting_key.empty();
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.IsEncryptingKeyEmpty",
        is_encrypting_key_empty);
    if (is_encrypting_key_empty) {
      PA_LOG(ERROR) << "Cannot encrypt group private key for device with ID "
                    << id << ". Encrypting key is empty.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    group_private_keys_to_encrypt[id] = CryptAuthEciesEncryptor::PayloadAndKey(
        group_key.private_key(), encrypting_key);
  }

  // All encrypting keys are empty; encryption not possible.
  if (group_private_keys_to_encrypt.empty()) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
    return;
  }

  SetState(State::kWaitingForGroupPrivateKeyEncryption);

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
  encryptor_->BatchEncrypt(
      group_private_keys_to_encrypt,
      base::BindOnce(
          &CryptAuthGroupPrivateKeySharerImpl::OnGroupPrivateKeysEncrypted,
          base::Unretained(this), request_context, group_key));
}

void CryptAuthGroupPrivateKeySharerImpl::OnGroupPrivateKeysEncrypted(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_encrypted_group_private_key_map) {
  DCHECK_EQ(State::kWaitingForGroupPrivateKeyEncryption, state_);

  // Record a success because the operation did not timeout. A separate metric
  // tracks individual encryption failures.
  RecordGroupPrivateKeyEncryptionMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthAsyncTaskResult::kSuccess);

  cryptauthv2::ShareGroupPrivateKeyRequest request;
  request.mutable_context()->CopyFrom(request_context);

  for (const auto& id_encrypted_key_pair :
       id_to_encrypted_group_private_key_map) {
    // If the group private key could not be encrypted for this ID--due to an
    // invalid encrypting key, for instance--skip it. Continue to share as many
    // encrypted group private keys as possible.
    bool was_encryption_successful = id_encrypted_key_pair.second.has_value();
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.GroupPrivateKeySharer.EncryptionSuccess",
        was_encryption_successful);
    if (!was_encryption_successful) {
      PA_LOG(ERROR) << "Group private key could not be encrypted for device "
                    << "with ID " << id_encrypted_key_pair.first;
      did_non_fatal_error_occur_ = true;
      continue;
    }

    cryptauthv2::EncryptedGroupPrivateKey* encrypted_key =
        request.add_encrypted_group_private_keys();
    encrypted_key->set_recipient_device_id(id_encrypted_key_pair.first);
    encrypted_key->set_sender_device_id(request_context.device_id());
    encrypted_key->set_encrypted_private_key(*id_encrypted_key_pair.second);

    // CryptAuth requires a SHA-256 hash of the group public key as an int64.
    encrypted_key->set_group_public_key_hash(
        CalculateInt64Sha256Hash(group_key.public_key()));
  }

  // All encryption attempts failed; nothing to share.
  if (request.encrypted_group_private_keys().empty()) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
    return;
  }

  SetState(State::kWaitingForShareGroupPrivateKeyResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->ShareGroupPrivateKey(
      request,
      base::BindOnce(
          &CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeySuccess,
          base::Unretained(this)),
      base::BindOnce(
          &CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeyFailure,
          base::Unretained(this)));
}

void CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeySuccess(
    const cryptauthv2::ShareGroupPrivateKeyResponse& response) {
  DCHECK_EQ(State::kWaitingForShareGroupPrivateKeyResponse, state_);

  RecordShareGroupPrivateKeyMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResult::kSuccess);

  CryptAuthDeviceSyncResult::ResultCode result_code =
      did_non_fatal_error_occur_
          ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
          : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
  FinishAttempt(result_code);
}

void CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeyFailure(
    NetworkRequestError error) {
  DCHECK_EQ(State::kWaitingForShareGroupPrivateKeyResponse, state_);

  RecordShareGroupPrivateKeyMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResultFromNetworkRequestError(error));

  FinishAttempt(ShareGroupPrivateKeyNetworkRequestErrorToResultCode(error));
}

void CryptAuthGroupPrivateKeySharerImpl::FinishAttempt(
    CryptAuthDeviceSyncResult::ResultCode result_code) {
  encryptor_.reset();
  cryptauth_client_.reset();

  SetState(State::kFinished);

  OnAttemptFinished(result_code);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthGroupPrivateKeySharerImpl::State& state) {
  switch (state) {
    case CryptAuthGroupPrivateKeySharerImpl::State::kNotStarted:
      stream << "[GroupPrivateKeySharer state: Not started]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::
        kWaitingForGroupPrivateKeyEncryption:
      stream << "[GroupPrivateKeySharer state: Waiting for group private key "
             << "encryption]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::
        kWaitingForShareGroupPrivateKeyResponse:
      stream << "[GroupPrivateKeySharer state: Waiting for "
             << "ShareGroupPrivateKey response]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::kFinished:
      stream << "[GroupPrivateKeySharer state: Finished]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace ash
