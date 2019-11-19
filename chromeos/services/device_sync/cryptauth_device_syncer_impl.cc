// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_syncer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer_impl.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/value_string_encoding.h"

namespace chromeos {

namespace device_sync {

namespace {

const cryptauthv2::KeyType kGroupKeyType = cryptauthv2::KeyType::P256;

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// these timeout values. For now, set these timeouts to the max execution time
// recorded by the metrics.
constexpr base::TimeDelta kWaitingForEncryptedGroupPrivateKeyProcessingTimeout =
    kMaxAsyncExecutionTime;
constexpr base::TimeDelta kWaitingForEncryptedDeviceMetadataProcessingTimeout =
    kMaxAsyncExecutionTime;

void RecordGroupPrivateKeyDecryptionMetrics(base::TimeDelta execution_time) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.ExecutionTime."
      "GroupPrivateKeyDecryption",
      execution_time);
}

void RecordDeviceMetadataDecryptionMetrics(base::TimeDelta execution_time) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.ExecutionTime."
      "DeviceMetadataDecryption",
      execution_time);
}

}  // namespace

// static
CryptAuthDeviceSyncerImpl::Factory*
    CryptAuthDeviceSyncerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthDeviceSyncerImpl::Factory* CryptAuthDeviceSyncerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthDeviceSyncerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceSyncerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceSyncer>
CryptAuthDeviceSyncerImpl::Factory::BuildInstance(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new CryptAuthDeviceSyncerImpl(
      device_registry, key_registry, client_factory, std::move(timer)));
}

CryptAuthDeviceSyncerImpl::CryptAuthDeviceSyncerImpl(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : device_registry_(device_registry),
      key_registry_(key_registry),
      client_factory_(client_factory),
      timer_(std::move(timer)) {
  DCHECK(device_registry);
  DCHECK(key_registry);
  DCHECK(client_factory);
}

CryptAuthDeviceSyncerImpl::~CryptAuthDeviceSyncerImpl() = default;

// static
base::Optional<base::TimeDelta> CryptAuthDeviceSyncerImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      return kWaitingForEncryptedGroupPrivateKeyProcessingTimeout;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      return kWaitingForEncryptedDeviceMetadataProcessingTimeout;
    default:
      // Signifies that there should not be a timeout.
      // Note: CryptAuthMetadataSyncerImpl, CryptAuthFeatureStatusGetterImpl,
      // and CryptAuthGroupPrivateKeySharerImpl guarantee that the callbacks
      // passed to their public methods are always invoke; in other words, these
      // implementations handle their relevant timeouts internally.
      return base::nullopt;
  }
}

// static
base::Optional<CryptAuthDeviceSyncResult::ResultCode>
CryptAuthDeviceSyncerImpl::ResultCodeErrorFromTimeoutDuringState(State state) {
  switch (state) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupPrivateKeyDecryption;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForDeviceMetadataDecryption;
    default:
      return base::nullopt;
  }
}

void CryptAuthDeviceSyncerImpl::OnAttemptStarted(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata) {
  DCHECK_EQ(State::kNotStarted, state_);

  request_context_.set_group(CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request_context_.mutable_client_metadata()->CopyFrom(client_metadata);
  request_context_.set_device_id(client_app_metadata.instance_id());
  request_context_.set_device_id_token(client_app_metadata.instance_id_token());

  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  if (!user_key_pair) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorMissingUserKeyPair);
    return;
  }

  local_better_together_device_metadata_.set_public_key(
      user_key_pair->public_key());
  local_better_together_device_metadata_.set_no_pii_device_name(
      client_app_metadata.device_model());

  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due to
  // async timeouts.
  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceSyncerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  base::Optional<CryptAuthDeviceSyncResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      RecordGroupPrivateKeyDecryptionMetrics(execution_time);
      break;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      RecordDeviceMetadataDecryptionMetrics(execution_time);
      break;
    default:
      NOTREACHED();
  }

  FinishAttempt(*error_code);
}

void CryptAuthDeviceSyncerImpl::AttemptNextStep() {
  switch (state_) {
    case State::kNotStarted:
      SyncMetadata();
      return;
    case State::kWaitingForMetadataSync:
      GetFeatureStatuses();
      return;
    case State::kWaitingForFeatureStatuses:
      ProcessEncryptedGroupPrivateKey();
      return;
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      ProcessEncryptedDeviceMetadata();
      return;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      ShareGroupPrivateKey();
      return;
    case State::kWaitingForGroupPrivateKeySharing: {
      CryptAuthDeviceSyncResult::ResultCode result_code =
          did_non_fatal_error_occur_
              ? CryptAuthDeviceSyncResult::ResultCode::
                    kFinishedWithNonFatalErrors
              : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
      FinishAttempt(result_code);
      return;
    }
    case State::kFinished:
      NOTREACHED();
      return;
  }
}

void CryptAuthDeviceSyncerImpl::SyncMetadata() {
  SetState(State::kWaitingForMetadataSync);

  metadata_syncer_ = CryptAuthMetadataSyncerImpl::Factory::Get()->BuildInstance(
      client_factory_);
  metadata_syncer_->SyncMetadata(
      request_context_, local_better_together_device_metadata_,
      key_registry_->GetActiveKey(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey),
      base::Bind(&CryptAuthDeviceSyncerImpl::OnSyncMetadataFinished,
                 base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnSyncMetadataFinished(
    const CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap&
        id_to_device_metadata_packet_map,
    std::unique_ptr<CryptAuthKey> new_group_key,
    const base::Optional<cryptauthv2::EncryptedGroupPrivateKey>&
        encrypted_group_private_key,
    const base::Optional<cryptauthv2::ClientDirective>& new_client_directive,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK_EQ(State::kWaitingForMetadataSync, state_);

  id_to_device_metadata_packet_map_ = id_to_device_metadata_packet_map;
  encrypted_group_private_key_ = encrypted_group_private_key;
  new_client_directive_ = new_client_directive;

  // If a new group key pair was created or if CryptAuth returned a new group
  // public key during the metadata sync, add the new group key to the key
  // registry.
  if (new_group_key)
    SetGroupKey(*new_group_key);

  switch (CryptAuthDeviceSyncResult::GetResultType(device_sync_result_code)) {
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      did_non_fatal_error_occur_ = true;
      FALLTHROUGH;
    case CryptAuthDeviceSyncResult::ResultType::kSuccess:
      // At a minimum, the local device metadata should be returned if no fatal
      // error occurred.
      DCHECK(base::Contains(id_to_device_metadata_packet_map_,
                            request_context_.device_id()));

      // A group key should be established by now if no fatal error occurred.
      DCHECK(key_registry_->GetActiveKey(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey));

      AttemptNextStep();
      return;
    case CryptAuthDeviceSyncResult::ResultType::kFatalError:
      FinishAttempt(device_sync_result_code);
      return;
  }
}

void CryptAuthDeviceSyncerImpl::SetGroupKey(const CryptAuthKey& new_group_key) {
  DCHECK_EQ(kGroupKeyType, new_group_key.type());

  const CryptAuthKey* current_group_key = key_registry_->GetActiveKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
  if (current_group_key) {
    if (*current_group_key == new_group_key)
      return;

    PA_LOG(VERBOSE) << "Deleting old DeviceSync BetterTogether group key:\n"
                    << "public = "
                    << util::EncodeAsString(current_group_key->public_key())
                    << ",\nprivate = "
                    << (current_group_key->private_key().empty()
                            ? "[empty]"
                            : "[not empty]");
    key_registry_->DeleteKey(
        CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey,
        current_group_key->handle());
  }

  PA_LOG(VERBOSE) << "New DeviceSync BetterTogether group key:\n"
                  << "public = "
                  << util::EncodeAsString(new_group_key.public_key())
                  << ",\nprivate = "
                  << (new_group_key.private_key().empty() ? "[empty]"
                                                          : "[not empty]");

  key_registry_->AddKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey,
      new_group_key);
}

void CryptAuthDeviceSyncerImpl::GetFeatureStatuses() {
  SetState(State::kWaitingForFeatureStatuses);

  base::flat_set<std::string> device_ids;
  for (const auto& id_packet_pair : id_to_device_metadata_packet_map_)
    device_ids.insert(id_packet_pair.first);

  feature_status_getter_ =
      CryptAuthFeatureStatusGetterImpl::Factory::Get()->BuildInstance(
          client_factory_);
  feature_status_getter_->GetFeatureStatuses(
      request_context_, device_ids,
      base::Bind(&CryptAuthDeviceSyncerImpl::OnGetFeatureStatusesFinished,
                 base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnGetFeatureStatusesFinished(
    const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
        id_to_device_software_feature_info_map,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK_EQ(State::kWaitingForFeatureStatuses, state_);

  // We require that the local device feature statuses are returned; the local
  // device is needed in the registry.
  if (!base::Contains(id_to_device_software_feature_info_map,
                      request_context_.device_id())) {
    FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                      kErrorMissingLocalDeviceFeatureStatuses);
    return;
  }

  switch (CryptAuthDeviceSyncResult::GetResultType(device_sync_result_code)) {
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      did_non_fatal_error_occur_ = true;
      FALLTHROUGH;
    case CryptAuthDeviceSyncResult::ResultType::kSuccess:
      BuildNewDeviceRegistry(id_to_device_software_feature_info_map);
      AttemptNextStep();
      return;
    case CryptAuthDeviceSyncResult::ResultType::kFatalError:
      FinishAttempt(device_sync_result_code);
      return;
  }
}

void CryptAuthDeviceSyncerImpl::BuildNewDeviceRegistry(
    const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
        id_to_device_software_feature_info_map) {
  // Add all device information to the new registry except the remote device
  // BetterTogether metadata that will be decrypted and added later if possible.
  new_device_registry_map_ = CryptAuthDeviceRegistry::InstanceIdToDeviceMap();
  for (const auto& id_device_software_feature_info_pair :
       id_to_device_software_feature_info_map) {
    const std::string& id = id_device_software_feature_info_pair.first;

    // The IDs in |id_to_device_software_feature_info_map| should be a subset of
    // those in |id_to_device_metadata_packet_map|.
    const auto packet_it = id_to_device_metadata_packet_map_.find(id);
    DCHECK(packet_it != id_to_device_metadata_packet_map_.end());
    const cryptauthv2::DeviceMetadataPacket& packet = packet_it->second;

    // Add BetterTogetherDeviceMetadata only for the local device.
    base::Optional<cryptauthv2::BetterTogetherDeviceMetadata> beto_metadata;
    if (id == request_context_.device_id())
      beto_metadata = local_better_together_device_metadata_;

    new_device_registry_map_->try_emplace(
        id, id, packet.device_name(), packet.device_public_key(),
        id_device_software_feature_info_pair.second.last_modified_time,
        beto_metadata,
        id_device_software_feature_info_pair.second.feature_state_map);
  }
}

void CryptAuthDeviceSyncerImpl::ProcessEncryptedGroupPrivateKey() {
  SetState(State::kWaitingForEncryptedGroupPrivateKeyProcessing);

  // CryptAuth will not return the group private key in the SyncMetadata
  // response if the key has not been uploaded by another user device or
  // possibly if we already own the group private key.
  if (!encrypted_group_private_key_) {
    AttemptNextStep();
    return;
  }

  const CryptAuthKey* device_sync_better_together_key =
      key_registry_->GetActiveKey(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  if (!device_sync_better_together_key ||
      device_sync_better_together_key->private_key().empty()) {
    FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                      kErrorMissingLocalDeviceSyncBetterTogetherKey);
    return;
  }

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Get()->BuildInstance();
  encryptor_->Decrypt(
      encrypted_group_private_key_->encrypted_private_key(),
      device_sync_better_together_key->private_key(),
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnGroupPrivateKeyDecrypted,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnGroupPrivateKeyDecrypted(
    const base::Optional<std::string>& group_private_key_from_cryptauth) {
  DCHECK_EQ(State::kWaitingForEncryptedGroupPrivateKeyProcessing, state_);

  RecordGroupPrivateKeyDecryptionMetrics(base::TimeTicks::Now() -
                                         last_state_change_timestamp_);

  if (!group_private_key_from_cryptauth) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorDecryptingGroupPrivateKey);
    return;
  }

  const CryptAuthKey* group_key = key_registry_->GetActiveKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
  DCHECK(group_key);

  // If there is no group private key in the key registry, add the newly
  // decrypted group private key. If a group private key already exists in the
  // key registry, verify it against the newly decrypted group private key.
  if (group_key->private_key().empty()) {
    SetGroupKey(CryptAuthKey(group_key->public_key(),
                             *group_private_key_from_cryptauth,
                             CryptAuthKey::Status::kActive, kGroupKeyType));
  } else if (group_key->private_key() != group_private_key_from_cryptauth) {
    // TODO(https://crbug.com/936273): Log metrics for inconsistent group
    // private keys.
    PA_LOG(ERROR) << "Group private key from CryptAuth unexpectedly "
                  << "disagrees with the one in local storage. Using "
                  << "group private key from local key registry.";
    did_non_fatal_error_occur_ = true;
  }

  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::ProcessEncryptedDeviceMetadata() {
  SetState(State::kWaitingForEncryptedDeviceMetadataProcessing);

  // If we still do not have a group private key, we cannot decrypt device
  // metadata nor share the group private key. Finish the DeviceSync attempt and
  // wait for a GCM notification alerting us that the group private key is
  // available.
  const CryptAuthKey* group_key = key_registry_->GetActiveKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
  DCHECK(group_key);
  if (group_key->private_key().empty()) {
    CryptAuthDeviceSyncResult::ResultCode result_code =
        did_non_fatal_error_occur_
            ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
            : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
    FinishAttempt(result_code);
    return;
  }

  DCHECK(new_device_registry_map_);
  CryptAuthEciesEncryptor::IdToInputMap id_to_encrypted_metadata_map;
  for (const auto& id_device_pair : *new_device_registry_map_) {
    const auto it =
        id_to_device_metadata_packet_map_.find(id_device_pair.first);
    DCHECK(it != id_to_device_metadata_packet_map_.end());
    id_to_encrypted_metadata_map[id_device_pair.first] =
        CryptAuthEciesEncryptor::PayloadAndKey(it->second.encrypted_metadata(),
                                               group_key->private_key());
  }

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Get()->BuildInstance();
  encryptor_->BatchDecrypt(
      id_to_encrypted_metadata_map,
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnDeviceMetadataDecrypted,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnDeviceMetadataDecrypted(
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_decrypted_metadata_map) {
  DCHECK_EQ(State::kWaitingForEncryptedDeviceMetadataProcessing, state_);

  RecordDeviceMetadataDecryptionMetrics(base::TimeTicks::Now() -
                                        last_state_change_timestamp_);

  AddDecryptedMetadataToNewDeviceRegistry(id_to_decrypted_metadata_map);

  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::AddDecryptedMetadataToNewDeviceRegistry(
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_decrypted_metadata_map) {
  DCHECK(new_device_registry_map_);

  // Update the new device registry with BetterTogether device metadata.
  for (auto& id_device_pair : *new_device_registry_map_) {
    cryptauthv2::BetterTogetherDeviceMetadata decrypted_metadata;

    const auto it = id_to_decrypted_metadata_map.find(id_device_pair.first);
    DCHECK(it != id_to_decrypted_metadata_map.end());

    // TODO(https://crbug.com/936273): Log metrics for metadata decryption
    // failure.
    bool was_metadata_decrypted = it->second.has_value();
    if (!was_metadata_decrypted) {
      PA_LOG(ERROR) << "Metadata for device with Instance ID " << it->first
                    << " was not able to be decrypted.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    // TODO(https://crbug.com/936273): Log metrics for metadata parsing failure.
    bool was_metadata_parsed = decrypted_metadata.ParseFromString(*it->second);
    if (!was_metadata_parsed) {
      PA_LOG(ERROR) << "Metadata for device with Instance ID " << it->first
                    << " was not able to be parsed.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    // The local device should already have its metadata set. Verify consistency
    // with data from CryptAuth.
    // TODO(https://crbug.com/936273): Log metrics for inconsistent local device
    // metadata.
    if (id_device_pair.first == request_context_.device_id()) {
      DCHECK(id_device_pair.second.better_together_device_metadata);
      bool is_local_device_metadata_consistent =
          *it->second == id_device_pair.second.better_together_device_metadata
                             ->SerializeAsString();
      if (!is_local_device_metadata_consistent) {
        PA_LOG(ERROR) << "Local device (Instance ID: "
                      << request_context_.device_id()
                      << ") metadata disagrees with that sent in SyncMetadata "
                      << "response.";
        did_non_fatal_error_occur_ = true;
      }

      continue;
    }

    id_device_pair.second.better_together_device_metadata = decrypted_metadata;
  }
}

void CryptAuthDeviceSyncerImpl::ShareGroupPrivateKey() {
  SetState(State::kWaitingForGroupPrivateKeySharing);

  CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap id_to_encrypting_key_map;
  for (const auto& id_packet_pair : id_to_device_metadata_packet_map_) {
    if (!id_packet_pair.second.need_group_private_key())
      continue;

    id_to_encrypting_key_map.insert_or_assign(
        id_packet_pair.first, id_packet_pair.second.device_public_key());
  }

  // No device needs the group private key.
  if (id_to_encrypting_key_map.empty()) {
    OnShareGroupPrivateKeyFinished(
        CryptAuthDeviceSyncResult::ResultCode::kSuccess);
    return;
  }

  const CryptAuthKey* group_key = key_registry_->GetActiveKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
  DCHECK(group_key);

  group_private_key_sharer_ =
      CryptAuthGroupPrivateKeySharerImpl::Factory::Get()->BuildInstance(
          client_factory_);
  group_private_key_sharer_->ShareGroupPrivateKey(
      request_context_, *group_key, id_to_encrypting_key_map,
      base::Bind(&CryptAuthDeviceSyncerImpl::OnShareGroupPrivateKeyFinished,
                 base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnShareGroupPrivateKeyFinished(
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK_EQ(State::kWaitingForGroupPrivateKeySharing, state_);

  switch (CryptAuthDeviceSyncResult::GetResultType(device_sync_result_code)) {
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      did_non_fatal_error_occur_ = true;
      FALLTHROUGH;
    case CryptAuthDeviceSyncResult::ResultType::kSuccess:
      AttemptNextStep();
      return;
    case CryptAuthDeviceSyncResult::ResultType::kFatalError:
      FinishAttempt(device_sync_result_code);
      return;
  }
}

void CryptAuthDeviceSyncerImpl::FinishAttempt(
    CryptAuthDeviceSyncResult::ResultCode result_code) {
  SetState(State::kFinished);

  metadata_syncer_.reset();
  feature_status_getter_.reset();
  encryptor_.reset();
  group_private_key_sharer_.reset();

  bool did_device_registry_change =
      new_device_registry_map_ &&
      device_registry_->SetRegistry(*new_device_registry_map_);

  OnAttemptFinished(CryptAuthDeviceSyncResult(
      result_code, did_device_registry_change, new_client_directive_));
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthDeviceSyncerImpl::State& state) {
  switch (state) {
    case CryptAuthDeviceSyncerImpl::State::kNotStarted:
      stream << "[DeviceSyncer state: Not started]";
      break;
    case CryptAuthDeviceSyncerImpl::State::kWaitingForMetadataSync:
      stream << "[DeviceSyncer state: Waiting for metadata sync]";
      break;
    case CryptAuthDeviceSyncerImpl::State::kWaitingForFeatureStatuses:
      stream << "[DeviceSyncer state: Waiting for feature statuses]";
      break;
    case CryptAuthDeviceSyncerImpl::State::
        kWaitingForEncryptedGroupPrivateKeyProcessing:
      stream << "[DeviceSyncer state: Waiting for encrypted group private key "
             << "processing]";
      break;
    case CryptAuthDeviceSyncerImpl::State::
        kWaitingForEncryptedDeviceMetadataProcessing:
      stream << "[DeviceSyncer state: Waiting for encrypted device metadata "
                "processing]";
      break;
    case CryptAuthDeviceSyncerImpl::State::kWaitingForGroupPrivateKeySharing:
      stream << "[DeviceSyncer state: Waiting for group private key "
             << "to be shared]";
      break;
    case CryptAuthDeviceSyncerImpl::State::kFinished:
      stream << "[DeviceSyncer state: Finished]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
