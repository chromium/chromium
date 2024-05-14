// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_syncer_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"

namespace ash {

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

void RecordGroupPrivateKeyDecryptionMetrics(base::TimeDelta execution_time,
                                            CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.ExecutionTime."
      "GroupPrivateKeyDecryption",
      execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.AsyncTaskResult."
      "GroupPrivateKeyDecryption",
      result);
}

void RecordDeviceMetadataDecryptionMetrics(base::TimeDelta execution_time,
                                           CryptAuthAsyncTaskResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.ExecutionTime."
      "DeviceMetadataDecryption",
      execution_time);
  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncV2.DeviceSyncer.AsyncTaskResult."
      "DeviceMetadataDecryption",
      result);
}

}  // namespace

// static
CryptAuthDeviceSyncerImpl::Factory*
    CryptAuthDeviceSyncerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthDeviceSyncer>
CryptAuthDeviceSyncerImpl::Factory::Create(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
    AttestationCertificatesSyncer* attestation_certificates_syncer,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        device_registry, key_registry, client_factory,
        synced_bluetooth_address_tracker, attestation_certificates_syncer,
        pref_service, std::move(timer));
  }

  return base::WrapUnique(new CryptAuthDeviceSyncerImpl(
      device_registry, key_registry, client_factory,
      synced_bluetooth_address_tracker, attestation_certificates_syncer,
      pref_service, std::move(timer)));
}

// static
void CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceSyncerImpl::Factory::~Factory() = default;

CryptAuthDeviceSyncerImpl::CryptAuthDeviceSyncerImpl(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
    AttestationCertificatesSyncer* attestation_certificates_syncer,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer)
    : device_registry_(device_registry),
      key_registry_(key_registry),
      client_factory_(client_factory),
      synced_bluetooth_address_tracker_(synced_bluetooth_address_tracker),
      attestation_certificates_syncer_(attestation_certificates_syncer),
      pref_service_(pref_service),
      timer_(std::move(timer)) {
  DCHECK(device_registry);
  DCHECK(key_registry);
  DCHECK(client_factory);
}

CryptAuthDeviceSyncerImpl::~CryptAuthDeviceSyncerImpl() = default;

// static
std::optional<base::TimeDelta> CryptAuthDeviceSyncerImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      return kWaitingForEncryptedGroupPrivateKeyProcessingTimeout;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      return kWaitingForEncryptedDeviceMetadataProcessingTimeout;
    default:
      // Signifies that there should not be a timeout.
      // Note: CryptAuthMetadataSyncerImpl, CryptAuthFeatureStatusGetterImpl,
      // CryptAuthGroupPrivateKeySharerImpl, and BluetoothAdapter guarantee that
      // the callbacks passed to their public methods are always invoke; in
      // other words, these implementations handle their relevant timeouts
      // internally.
      return std::nullopt;
  }
}

// static
std::optional<CryptAuthDeviceSyncResult::ResultCode>
CryptAuthDeviceSyncerImpl::ResultCodeErrorFromTimeoutDuringState(State state) {
  switch (state) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupPrivateKeyDecryption;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForDeviceMetadataDecryption;
    default:
      return std::nullopt;
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

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceSyncerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  std::optional<CryptAuthDeviceSyncResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForEncryptedGroupPrivateKeyProcessing:
      RecordGroupPrivateKeyDecryptionMetrics(
          execution_time, CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForEncryptedDeviceMetadataProcessing:
      RecordDeviceMetadataDecryptionMetrics(execution_time,
                                            CryptAuthAsyncTaskResult::kTimeout);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  FinishAttempt(*error_code);
}

void CryptAuthDeviceSyncerImpl::AttemptNextStep() {
  switch (state_) {
    case State::kNotStarted:
      GetBluetoothAddress();
      return;
    case State::kWaitingForBluetoothAddress:
      if (features::IsCryptauthAttestationSyncingEnabled()) {
        GetAttestationCertificates();
        return;
      }
      [[fallthrough]];
    case State::kWaitingForAttestationCertificates:
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
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void CryptAuthDeviceSyncerImpl::GetBluetoothAddress() {
  DCHECK_EQ(State::kNotStarted, state_);
  SetState(State::kWaitingForBluetoothAddress);
  synced_bluetooth_address_tracker_->GetBluetoothAddress(
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnBluetoothAddress,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthDeviceSyncerImpl::OnBluetoothAddress(
    const std::string& bluetooth_address) {
  DCHECK_EQ(State::kWaitingForBluetoothAddress, state_);

  if (!bluetooth_address.empty()) {
    local_better_together_device_metadata_.set_bluetooth_public_address(
        bluetooth_address);
  }

  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::GetAttestationCertificates() {
  SetState(State::kWaitingForAttestationCertificates);
  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  attestation_certificates_syncer_->UpdateCerts(
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnAttestationCertificates,
                     weak_ptr_factory_.GetWeakPtr()),
      user_key_pair->public_key());
}

void CryptAuthDeviceSyncerImpl::OnAttestationCertificates(
    const std::vector<std::string>& cert_chain,
    bool valid) {
  cryptauthv2::AttestationData* attestation_data =
      local_better_together_device_metadata_.mutable_attestation_data();
  attestation_data->set_type(
      cryptauthv2::AttestationData::CROS_SOFT_BIND_CERT_CHAIN);
  for (const std::string& cert : cert_chain) {
    attestation_data->add_certificates(cert);
  }
  are_attestation_certs_valid_ = valid;
  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::SyncMetadata() {
  SetState(State::kWaitingForMetadataSync);

  metadata_syncer_ = CryptAuthMetadataSyncerImpl::Factory::Create(
      client_factory_, pref_service_);
  metadata_syncer_->SyncMetadata(
      request_context_, local_better_together_device_metadata_,
      key_registry_->GetActiveKey(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey),
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnSyncMetadataFinished,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnSyncMetadataFinished(
    const CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap&
        id_to_device_metadata_packet_map,
    std::unique_ptr<CryptAuthKey> new_group_key,
    const std::optional<cryptauthv2::EncryptedGroupPrivateKey>&
        encrypted_group_private_key,
    const std::optional<cryptauthv2::ClientDirective>& new_client_directive,
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
      [[fallthrough]];
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
      CryptAuthFeatureStatusGetterImpl::Factory::Create(client_factory_);
  feature_status_getter_->GetFeatureStatuses(
      request_context_, device_ids,
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnGetFeatureStatusesFinished,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnGetFeatureStatusesFinished(
    const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
        id_to_device_software_feature_info_map,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK_EQ(State::kWaitingForFeatureStatuses, state_);

  switch (CryptAuthDeviceSyncResult::GetResultType(device_sync_result_code)) {
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      did_non_fatal_error_occur_ = true;
      [[fallthrough]];
    case CryptAuthDeviceSyncResult::ResultType::kSuccess:
      // We require that the local device feature statuses are returned; the
      // local device is needed in the registry.
      if (!base::Contains(id_to_device_software_feature_info_map,
                          request_context_.device_id())) {
        FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                          kErrorMissingLocalDeviceFeatureStatuses);
        return;
      }

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
  // Add all device information to the new registry except the new remote device
  // BetterTogether metadata that will be decrypted and added later if possible.
  // In the interim, use the existing BetterTogether metadata for the device
  // from the current registry, if available.
  new_device_registry_map_ = CryptAuthDeviceRegistry::InstanceIdToDeviceMap();
  for (const auto& id_device_software_feature_info_pair :
       id_to_device_software_feature_info_map) {
    const std::string& id = id_device_software_feature_info_pair.first;

    // The IDs in |id_to_device_software_feature_info_map| should be a subset of
    // those in |id_to_device_metadata_packet_map|.
    const auto packet_it = id_to_device_metadata_packet_map_.find(id);
    DCHECK(packet_it != id_to_device_metadata_packet_map_.end());
    const cryptauthv2::DeviceMetadataPacket& packet = packet_it->second;

    // Add BetterTogetherDeviceMetadata for the local device and all devices
    // with BetterTogetherDeviceMetadata in the existing device registry.
    std::optional<cryptauthv2::BetterTogetherDeviceMetadata> beto_metadata;
    if (id == request_context_.device_id()) {
      beto_metadata = local_better_together_device_metadata_;
    } else {
      const CryptAuthDevice* existing_device = device_registry_->GetDevice(id);
      if (existing_device)
        beto_metadata = existing_device->better_together_device_metadata;
    }

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
    group_private_key_status_ =
        GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived;
    AttemptNextStep();
    return;
  }

  if (encrypted_group_private_key_->encrypted_private_key().empty()) {
    // TODO(crbug.com/41443836): Log metrics for empty private key.
    PA_LOG(ERROR) << "Group private key from CryptAuth unexpectedly empty.";
    did_non_fatal_error_occur_ = true;
    group_private_key_status_ =
        GroupPrivateKeyStatus::kEncryptedGroupPrivateKeyEmpty;
    AttemptNextStep();
    return;
  }

  const CryptAuthKey* device_sync_better_together_key =
      key_registry_->GetActiveKey(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  if (!device_sync_better_together_key ||
      device_sync_better_together_key->private_key().empty()) {
    group_private_key_status_ =
        GroupPrivateKeyStatus::kLocalDeviceSyncBetterTogetherKeyMissing;
    FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                      kErrorMissingLocalDeviceSyncBetterTogetherKey);
    return;
  }

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
  encryptor_->Decrypt(
      encrypted_group_private_key_->encrypted_private_key(),
      device_sync_better_together_key->private_key(),
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnGroupPrivateKeyDecrypted,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnGroupPrivateKeyDecrypted(
    const std::optional<std::string>& group_private_key_from_cryptauth) {
  DCHECK_EQ(State::kWaitingForEncryptedGroupPrivateKeyProcessing, state_);

  bool success = group_private_key_from_cryptauth.has_value();
  RecordGroupPrivateKeyDecryptionMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      success ? CryptAuthAsyncTaskResult::kSuccess
              : CryptAuthAsyncTaskResult::kError);

  if (!success) {
    group_private_key_status_ =
        GroupPrivateKeyStatus::kGroupPrivateKeyDecryptionFailed;
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorDecryptingGroupPrivateKey);
    return;
  }
  group_private_key_status_ =
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted;

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
  } else {
    bool is_group_private_key_consistent =
        group_key->private_key() == group_private_key_from_cryptauth;
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.DeviceSyncer.IsGroupPrivateKeyConsistent",
        is_group_private_key_consistent);
    if (!is_group_private_key_consistent) {
      PA_LOG(ERROR) << "Group private key from CryptAuth unexpectedly "
                    << "disagrees with the one in local storage. Using "
                    << "group private key from local key registry.";
      did_non_fatal_error_occur_ = true;
    }
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
    PA_LOG(WARNING)
        << "CryptAuthDeviceSyncerImpl::" << __func__
        << ": Missing group private key needed to decrypt device metadata. "
        << "Finishing DeviceSync attempt and waiting for GCM message from "
        << "CryptAuth when the group private key becomes available.";
    better_together_metadata_status_ =
        BetterTogetherMetadataStatus::kGroupPrivateKeyMissing;
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

    // Do not try to decrypt metadata that is not sent. This can happen if a
    // device has not uploaded metadata encrypted with the correct group public
    // key.
    if (it->second.encrypted_metadata().empty())
      continue;

    id_to_encrypted_metadata_map[id_device_pair.first] =
        CryptAuthEciesEncryptor::PayloadAndKey(it->second.encrypted_metadata(),
                                               group_key->private_key());
  }

  if (id_to_encrypted_metadata_map.empty()) {
    PA_LOG(ERROR) << "No encrypted metadata sent by CryptAuth. We expect the "
                  << "local device's encrypted metadata, at a minimum.";
    better_together_metadata_status_ =
        BetterTogetherMetadataStatus::kEncryptedMetadataEmpty;
    did_non_fatal_error_occur_ = true;
    AttemptNextStep();
    return;
  }

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Create();
  encryptor_->BatchDecrypt(
      id_to_encrypted_metadata_map,
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnDeviceMetadataDecrypted,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnDeviceMetadataDecrypted(
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_decrypted_metadata_map) {
  DCHECK_EQ(State::kWaitingForEncryptedDeviceMetadataProcessing, state_);

  // Record a success because the operation did not timeout. A separate metric
  // tracks individual decryption failures.
  RecordDeviceMetadataDecryptionMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthAsyncTaskResult::kSuccess);
  better_together_metadata_status_ =
      BetterTogetherMetadataStatus::kMetadataDecrypted;

  AddDecryptedMetadataToNewDeviceRegistry(id_to_decrypted_metadata_map);

  AttemptNextStep();
}

void CryptAuthDeviceSyncerImpl::AddDecryptedMetadataToNewDeviceRegistry(
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_decrypted_metadata_map) {
  DCHECK(new_device_registry_map_);

  // Update the new device registry with BetterTogether device metadata.
  for (const auto& id_metadata_pair : id_to_decrypted_metadata_map) {
    bool was_metadata_decrypted = id_metadata_pair.second.has_value();
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.DeviceSyncer.MetadataDecryptionSuccess",
        was_metadata_decrypted);
    if (!was_metadata_decrypted) {
      PA_LOG(ERROR) << "Metadata for device with Instance ID "
                    << id_metadata_pair.first
                    << " was not able to be decrypted.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    cryptauthv2::BetterTogetherDeviceMetadata decrypted_metadata;
    bool was_metadata_parsed =
        decrypted_metadata.ParseFromString(*id_metadata_pair.second);
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.DeviceSyncer.MetadataParsingSuccess",
        was_metadata_parsed);
    if (!was_metadata_parsed) {
      PA_LOG(ERROR) << "Metadata for device with Instance ID "
                    << id_metadata_pair.first << " was not able to be parsed.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    auto it = new_device_registry_map_->find(id_metadata_pair.first);
    DCHECK(it != new_device_registry_map_->end());

    // The local device should already have its metadata set. Verify consistency
    // with data from CryptAuth.
    if (id_metadata_pair.first == request_context_.device_id()) {
      DCHECK(it->second.better_together_device_metadata);
      bool is_local_device_metadata_consistent =
          id_metadata_pair.second ==
          it->second.better_together_device_metadata->SerializeAsString();
      base::UmaHistogramBoolean(
          "CryptAuth.DeviceSyncV2.DeviceSyncer.IsLocalDeviceMetadataConsistent",
          is_local_device_metadata_consistent);
      if (!is_local_device_metadata_consistent) {
        PA_LOG(ERROR) << "Local device (Instance ID: "
                      << request_context_.device_id()
                      << ") metadata disagrees with that sent in SyncMetadata "
                      << "response.";
        did_non_fatal_error_occur_ = true;
      }
      continue;
    }

    it->second.better_together_device_metadata = decrypted_metadata;
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
      CryptAuthGroupPrivateKeySharerImpl::Factory::Create(client_factory_);
  group_private_key_sharer_->ShareGroupPrivateKey(
      request_context_, *group_key, id_to_encrypting_key_map,
      base::BindOnce(&CryptAuthDeviceSyncerImpl::OnShareGroupPrivateKeyFinished,
                     base::Unretained(this)));
}

void CryptAuthDeviceSyncerImpl::OnShareGroupPrivateKeyFinished(
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK_EQ(State::kWaitingForGroupPrivateKeySharing, state_);

  switch (CryptAuthDeviceSyncResult::GetResultType(device_sync_result_code)) {
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      did_non_fatal_error_occur_ = true;
      [[fallthrough]];
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

  CryptAuthDeviceSyncResult::ResultType result_type =
      CryptAuthDeviceSyncResult::GetResultType(result_code);
  if (result_type == CryptAuthDeviceSyncResult::ResultType::kSuccess) {
    synced_bluetooth_address_tracker_->SetLastSyncedBluetoothAddress(
        local_better_together_device_metadata_.bluetooth_public_address());
    if (features::IsCryptauthAttestationSyncingEnabled()) {
      if (are_attestation_certs_valid_) {
        attestation_certificates_syncer_->SetLastSyncTimestamp();
      }
      are_attestation_certs_valid_ = false;
    }
  }

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
    case CryptAuthDeviceSyncerImpl::State::kWaitingForBluetoothAddress:
      stream << "[DeviceSyncer state: Waiting for Bluetooth address]";
      break;
    case CryptAuthDeviceSyncerImpl::State::kWaitingForAttestationCertificates:
      stream << "[DeviceSyncer state: Waiting for attestation certs]";
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

}  // namespace ash
