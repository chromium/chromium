// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/sync_device_info/device_info_sync_bridge.h"

#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <unordered_set>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_proto_enum_util.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace syncer {

using base::Time;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::FeatureSpecificFields;
using sync_pb::SharingSpecificFields;

using Record = DataTypeStore::Record;
using RecordList = DataTypeStore::RecordList;
using WriteBatch = DataTypeStore::WriteBatch;

namespace {

constexpr base::TimeDelta kExpirationThreshold = base::Days(56);

// Find the timestamp for the last time this |device_info| was edited.
Time GetLastUpdateTime(const DeviceInfoSpecifics& specifics) {
  if (specifics.has_last_updated_timestamp()) {
    return ProtoTimeToTime(specifics.last_updated_timestamp());
  } else {
    return Time();
  }
}

base::TimeDelta GetPulseIntervalFromSpecifics(
    const DeviceInfoSpecifics& specifics) {
  if (specifics.has_pulse_interval_in_minutes()) {
    return base::Minutes(specifics.pulse_interval_in_minutes());
  }
  // If the interval is not set on the specifics it must be an old device, so we
  // fall back to the value used by old devices. We really do not want to use
  // the default int value of 0.
  return base::Days(1);
}

std::optional<DeviceInfo::SharingInfo> SpecificsToSharingInfo(
    const DeviceInfoSpecifics& specifics) {
  TRACE_EVENT0("sync", "syncer::SpecificsToSharingInfo");
  if (!specifics.has_sharing_fields()) {
    return std::nullopt;
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  for (int i = 0; i < specifics.sharing_fields().enabled_features_size(); ++i) {
    enabled_features.insert(specifics.sharing_fields().enabled_features(i));
  }
  return DeviceInfo::SharingInfo(
      {specifics.sharing_fields().vapid_fcm_token(),
       specifics.sharing_fields().vapid_p256dh(),
       specifics.sharing_fields().vapid_auth_secret()},
      {specifics.sharing_fields().sender_id_fcm_token_v2(),
       specifics.sharing_fields().sender_id_p256dh_v2(),
       specifics.sharing_fields().sender_id_auth_secret_v2()},
      specifics.sharing_fields().chime_representative_target_id(),
      std::move(enabled_features));
}

std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>
SpecificsToPhoneAsASecurityKeyInfo(const DeviceInfoSpecifics& specifics) {
  if (!specifics.has_paask_fields()) {
    return std::nullopt;
  }

  DeviceInfo::PhoneAsASecurityKeyInfo to;
  const auto& from = specifics.paask_fields();
  if (!from.has_tunnel_server_domain() || !from.has_id() ||
      !from.has_contact_id() || !from.has_secret() ||
      !from.has_peer_public_key_x962() ||
      from.tunnel_server_domain() >= 0x10000) {
    return std::nullopt;
  }
  to.tunnel_server_domain = from.tunnel_server_domain();
  to.id = from.id();
  to.contact_id = base::ToVector(base::as_byte_span(from.contact_id()));

  if (from.secret().size() != to.secret.size()) {
    return std::nullopt;
  }
  memcpy(to.secret.data(), from.secret().data(), to.secret.size());

  if (from.peer_public_key_x962().size() != to.peer_public_key_x962.size()) {
    return std::nullopt;
  }
  memcpy(to.peer_public_key_x962.data(), from.peer_public_key_x962().data(),
         to.peer_public_key_x962.size());

  return to;
}

std::optional<base::Time> SpecificsToFloatingWorkspaceLastSigninTime(
    const DeviceInfoSpecifics& specifics) {
  if (!specifics.feature_fields()
           .has_floating_workspace_last_signin_time_windows_epoch_micros()) {
    return std::nullopt;
  }
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
      specifics.feature_fields()
          .floating_workspace_last_signin_time_windows_epoch_micros()));
}

std::string GetVersionNumberFromSpecifics(
    const DeviceInfoSpecifics& specifics) {
  // The new field takes precedence, if populated.
  if (specifics.has_chrome_version_info()) {
    return specifics.chrome_version_info().version_number();
  }

  // Fall back to the legacy proto field.
  return specifics.chrome_version();
}

// Returns true if |speifics| represents a client that is
// chromium-based and hence exposed in DeviceInfoTracker.
bool IsChromeClient(const DeviceInfoSpecifics& specifics) {
  return specifics.has_chrome_version_info() || specifics.has_chrome_version();
}

// Converts DeviceInfoSpecifics into DeviceInfo.
DeviceInfo SpecificsToModel(const DeviceInfoSpecifics& specifics) {
  DeviceInfo::FormFactor device_form_factor;
  if (specifics.has_device_form_factor()) {
    device_form_factor = ToDeviceInfoFormFactor(specifics.device_form_factor());
  } else {
    // Fallback to derive from old device type enum.
    device_form_factor =
        DeriveFormFactorFromDeviceType(specifics.device_type());
  }
  DeviceInfo::OsType os_type;
  if (specifics.has_os_type()) {
    os_type = ToDeviceInfoOsType(specifics.os_type());
  } else {
    // Fallback to derive from old device type enum.
    os_type = DeriveOsFromDeviceType(specifics.device_type(),
                                     specifics.manufacturer());
  }
  return DeviceInfo(
      specifics.cache_guid(), specifics.client_name(),
      GetVersionNumberFromSpecifics(specifics), specifics.sync_user_agent(),
      specifics.device_type(), os_type, device_form_factor,
      specifics.signin_scoped_device_id(), specifics.manufacturer(),
      specifics.model(), specifics.full_hardware_class(),
      ProtoTimeToTime(specifics.last_updated_timestamp()),
      GetPulseIntervalFromSpecifics(specifics),
      specifics.feature_fields().send_tab_to_self_receiving_enabled(),
      specifics.feature_fields().send_tab_to_self_receiving_type(),
      SpecificsToSharingInfo(specifics),
      SpecificsToPhoneAsASecurityKeyInfo(specifics),
      specifics.invalidation_fields().instance_id_token(),
      GetDataTypeSetFromSpecificsFieldNumberList(
          specifics.invalidation_fields().interested_data_type_ids()),
      SpecificsToFloatingWorkspaceLastSigninTime(specifics));
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<EntityData> CopyToEntityData(
    const DeviceInfoSpecifics& specifics) {
  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_device_info() = specifics;
  entity_data->name = specifics.client_name();
  return entity_data;
}

sync_pb::PhoneAsASecurityKeySpecificFields PhoneAsASecurityKeyInfoToProto(
    const DeviceInfo::PhoneAsASecurityKeyInfo& paask_info) {
  sync_pb::PhoneAsASecurityKeySpecificFields paask_fields;
  paask_fields.set_tunnel_server_domain(paask_info.tunnel_server_domain);
  paask_fields.set_contact_id(paask_info.contact_id.data(),
                              paask_info.contact_id.size());
  paask_fields.set_secret(paask_info.secret.data(), paask_info.secret.size());
  paask_fields.set_id(paask_info.id);
  paask_fields.set_peer_public_key_x962(paask_info.peer_public_key_x962.data(),
                                        paask_info.peer_public_key_x962.size());
  return paask_fields;
}

// Converts a local DeviceInfo into a freshly allocated DeviceInfoSpecifics.
std::unique_ptr<DeviceInfoSpecifics> MakeLocalDeviceSpecifics(
    const DeviceInfo& info) {
  auto specifics = std::make_unique<DeviceInfoSpecifics>();
  specifics->set_cache_guid(info.guid());
  specifics->set_client_name(info.client_name());
  specifics->set_chrome_version(info.chrome_version());
  specifics->mutable_chrome_version_info()->set_version_number(
      info.chrome_version());
  specifics->set_sync_user_agent(info.sync_user_agent());
  specifics->set_device_type(info.device_type());
  specifics->set_os_type(ToOsTypeProto(info.os_type()));
  specifics->set_device_form_factor(
      ToDeviceFormFactorProto(info.form_factor()));
  specifics->set_signin_scoped_device_id(info.signin_scoped_device_id());
  specifics->set_manufacturer(info.manufacturer_name());
  specifics->set_model(info.model_name());

  const std::string full_hardware_class = info.full_hardware_class();
  if (!full_hardware_class.empty()) {
    specifics->set_full_hardware_class(full_hardware_class);
  }

  // The local device should have not been updated yet. Set the last updated
  // timestamp to now.
  DCHECK(info.last_updated_timestamp() == base::Time());
  specifics->set_last_updated_timestamp(TimeToProtoTime(Time::Now()));
  specifics->set_pulse_interval_in_minutes(info.pulse_interval().InMinutes());

  FeatureSpecificFields* feature_fields = specifics->mutable_feature_fields();
  feature_fields->set_send_tab_to_self_receiving_enabled(
      info.send_tab_to_self_receiving_enabled());
  feature_fields->set_send_tab_to_self_receiving_type(
      info.send_tab_to_self_receiving_type());
  if (info.floating_workspace_last_signin_timestamp().has_value()) {
    feature_fields
        ->set_floating_workspace_last_signin_time_windows_epoch_micros(
            info.floating_workspace_last_signin_timestamp()
                .value()
                .ToDeltaSinceWindowsEpoch()
                .InMicroseconds());
  }
  const std::optional<DeviceInfo::SharingInfo>& sharing_info =
      info.sharing_info();
  if (sharing_info) {
    SharingSpecificFields* sharing_fields = specifics->mutable_sharing_fields();
    sharing_fields->set_vapid_fcm_token(
        sharing_info->vapid_target_info.fcm_token);
    sharing_fields->set_vapid_p256dh(sharing_info->vapid_target_info.p256dh);
    sharing_fields->set_vapid_auth_secret(
        sharing_info->vapid_target_info.auth_secret);
    sharing_fields->set_sender_id_fcm_token_v2(
        sharing_info->sender_id_target_info.fcm_token);
    sharing_fields->set_sender_id_p256dh_v2(
        sharing_info->sender_id_target_info.p256dh);
    sharing_fields->set_sender_id_auth_secret_v2(
        sharing_info->sender_id_target_info.auth_secret);
    sharing_fields->set_chime_representative_target_id(
        sharing_info->chime_representative_target_id);
    for (sync_pb::SharingSpecificFields::EnabledFeatures feature :
         sharing_info->enabled_features) {
      sharing_fields->add_enabled_features(feature);
    }
  }

  const std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>& paask_info =
      info.paask_info();
  if (paask_info) {
    *specifics->mutable_paask_fields() =
        PhoneAsASecurityKeyInfoToProto(*paask_info);
  }

  // Set sync invalidations FCM registration token and interested data types.
  if (!info.fcm_registration_token().empty()) {
    specifics->mutable_invalidation_fields()->set_instance_id_token(
        info.fcm_registration_token());
  }
  for (const DataType data_type : info.interested_data_types()) {
    specifics->mutable_invalidation_fields()->add_interested_data_type_ids(
        GetSpecificsFieldNumberFromDataType(data_type));
  }

  return specifics;
}

// Returns true if |stored| is similar enough to |current| that |current|
// needn't be uploaded.
bool StoredDeviceInfoStillAccurate(const DeviceInfo* stored,
                                   const DeviceInfo* current) {
  return current->guid() == stored->guid() &&
         current->client_name() == stored->client_name() &&
         current->chrome_version() == stored->chrome_version() &&
         current->sync_user_agent() == stored->sync_user_agent() &&
         current->device_type() == stored->device_type() &&
         current->os_type() == stored->os_type() &&
         current->form_factor() == stored->form_factor() &&
         current->signin_scoped_device_id() ==
             stored->signin_scoped_device_id() &&
         current->manufacturer_name() == stored->manufacturer_name() &&
         current->model_name() == stored->model_name() &&
         current->full_hardware_class() == stored->full_hardware_class() &&
         current->send_tab_to_self_receiving_enabled() ==
             stored->send_tab_to_self_receiving_enabled() &&
         current->send_tab_to_self_receiving_type() ==
             stored->send_tab_to_self_receiving_type() &&
         current->sharing_info() == stored->sharing_info() &&
         current->paask_info().has_value() ==
             stored->paask_info().has_value() &&
         (!current->paask_info().has_value() ||
          current->paask_info()->NonRotatingFieldsEqual(
              stored->paask_info().value())) &&
         current->fcm_registration_token() ==
             stored->fcm_registration_token() &&
         current->interested_data_types() == stored->interested_data_types() &&
         current->floating_workspace_last_signin_timestamp().has_value() ==
             stored->floating_workspace_last_signin_timestamp().has_value() &&
         current->floating_workspace_last_signin_timestamp() ==
             stored->floating_workspace_last_signin_timestamp();
}

// Record a histogram of the age of the PaaSK fields, in days. To confirm that
// crbug.com/1465558 is fixed.
// TODO(crbug.com/40276038): remove this function before Oct 2023.
void RecordPhoneAsASecurityKeyFieldsAge(const DeviceInfoSpecifics& specifics) {
  if (!specifics.has_paask_fields()) {
    return;
  }
  // This is just for the purposes of measurement so this code knows that the
  // ID field, in prelinked data, is actually a time_t divided by 86400, the
  // number of seconds in a typical day.
  const int age_days = static_cast<int>(base::Time::Now().ToTimeT() / 86400) -
                       static_cast<int>(specifics.paask_fields().id());
  int recorded_value = age_days;
  // The desktop will ignore records older than 31 days so it's not useful to
  // track if they're older than that.
  if (recorded_value > 31) {
    recorded_value = 31;
  } else if (recorded_value < 0) {
    // If the system clock has gone backwards then the age might be negative.
    // Record this with a special value so that we can confirm that it's very
    // rare.
    recorded_value = 32;
  }
  base::UmaHistogramExactLinear("WebAuthentication.CableV2.PrelinkDataAgeDays",
                                recorded_value, /*exclusive_max=*/33);
}

}  // namespace

DeviceInfoSyncBridge::ImmutableDeviceInfoAndSpecifics::
    ImmutableDeviceInfoAndSpecifics(sync_pb::DeviceInfoSpecifics specifics)
    : specifics_(std::move(specifics)),
      device_info_(SpecificsToModel(specifics_)) {}

DeviceInfoSyncBridge::DeviceInfoSyncBridge(
    std::unique_ptr<MutableLocalDeviceInfoProvider> local_device_info_provider,
    OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
    std::unique_ptr<DeviceInfoPrefs> device_info_prefs)
    : DataTypeSyncBridge(std::move(change_processor)),
      local_device_info_provider_(std::move(local_device_info_provider)),
      device_info_prefs_(std::move(device_info_prefs)) {
  DCHECK(local_device_info_provider_);
  DCHECK(device_info_prefs_);

  // Provider must not be initialized, the bridge takes care.
  DCHECK(!local_device_info_provider_->GetLocalDeviceInfo());

  std::move(store_factory)
      .Run(DEVICE_INFO, base::BindOnce(&DeviceInfoSyncBridge::OnStoreCreated,
                                       weak_ptr_factory_.GetWeakPtr()));
}

DeviceInfoSyncBridge::~DeviceInfoSyncBridge() {
  for (auto& observer : observers_) {
    observer.OnDeviceInfoShutdown();
  }
}

LocalDeviceInfoProvider* DeviceInfoSyncBridge::GetLocalDeviceInfoProvider() {
  return local_device_info_provider_.get();
}

void DeviceInfoSyncBridge::RefreshLocalDeviceInfoIfNeeded() {
  // Device info cannot be synced if the provider is not initialized. When it
  // gets initialized, local device info will be sent.
  if (!local_device_info_provider_->GetLocalDeviceInfo()) {
    return;
  }

  ReconcileLocalAndStored();
}

void DeviceInfoSyncBridge::SetCommittedAdditionalInterestedDataTypesCallback(
    base::RepeatingCallback<void(const DataTypeSet&)> callback) {
  new_interested_data_types_callback_ = std::move(callback);
}

void DeviceInfoSyncBridge::OnSyncStarting(
    const DataTypeActivationRequest& request) {
  // Store the cache GUID, mainly in case MergeFullSyncData() is executed later.
  local_cache_guid_ = request.cache_guid;
  // Garbage-collect old local cache GUIDs, for privacy reasons.
  device_info_prefs_->GarbageCollectExpiredCacheGuids();
  // Add the cache guid to the local prefs.
  device_info_prefs_->AddLocalCacheGuid(local_cache_guid_);
  // SyncMode determines the client name in GetLocalClientName().
  sync_mode_ = request.sync_mode;
  // Reset reupload state after each sync starting.
  reuploaded_on_tombstone_ = false;

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  // Local device's client name needs to updated if OnSyncStarting is called
  // after local device has already been initialized since the client name
  // depends on |sync_mode_|.
  local_device_info_provider_->UpdateClientName(GetLocalClientName());
  ReconcileLocalAndStored();
}

std::unique_ptr<MetadataChangeList>
DeviceInfoSyncBridge::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

std::optional<ModelError> DeviceInfoSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(all_data_.empty());
  DCHECK(!local_cache_guid_.empty());

  local_device_info_provider_->Initialize(
      local_cache_guid_, GetLocalClientName(),
      local_device_name_info_.manufacturer_name,
      local_device_name_info_.model_name,
      local_device_name_info_.full_hardware_class,
      /*device_info_restored_from_store=*/nullptr);

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const auto& change : entity_data) {
    const DeviceInfoSpecifics& specifics =
        change->data().specifics.device_info();
    DCHECK_EQ(change->storage_key(), specifics.cache_guid());

    // Each device is the authoritative source for itself, ignore any remote
    // changes that have a cache guid that is or was this local device.
    if (device_info_prefs_->IsRecentLocalCacheGuid(change->storage_key())) {
      continue;
    }

    StoreSpecifics(specifics, batch.get());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  // Complete batch with local data and commit.
  SendLocalDataWithBatch(std::move(batch));
  return std::nullopt;
}

std::optional<ModelError> DeviceInfoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK(!local_cache_guid_.empty());
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  bool has_changes = false;
  bool has_tombstone_for_local_device = false;
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    const std::string guid = change->storage_key();

    // Reupload local device if it was deleted from the server.
    if (local_cache_guid_ == guid &&
        change->type() == EntityChange::ACTION_DELETE) {
      has_tombstone_for_local_device = true;
      continue;
    }

    // Ignore any remote changes that have a cache guid that is or was this
    // local device.
    if (device_info_prefs_->IsRecentLocalCacheGuid(guid)) {
      continue;
    }

    if (change->type() == EntityChange::ACTION_DELETE) {
      has_changes |= DeleteSpecifics(guid, batch.get());
    } else {
      const DeviceInfoSpecifics& specifics =
          change->data().specifics.device_info();
      DCHECK(guid == specifics.cache_guid());
      StoreSpecifics(specifics, batch.get());
      has_changes = true;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitAndNotify(std::move(batch), has_changes);

  if (!change_processor()->IsEntityUnsynced(local_cache_guid_)) {
    for (base::OnceClosure& callback : device_info_synced_callback_list_) {
      std::move(callback).Run();
    }
    device_info_synced_callback_list_.clear();
  }

  if (has_tombstone_for_local_device && !reuploaded_on_tombstone_) {
    SendLocalData();
    reuploaded_on_tombstone_ = true;
  }

  return std::nullopt;
}

std::unique_ptr<DataBatch> DeviceInfoSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& key : storage_keys) {
    const auto& iter = all_data_.find(key);
    if (iter != all_data_.end()) {
      DCHECK_EQ(key, iter->second.specifics().cache_guid());
      batch->Put(key, CopyToEntityData(iter->second.specifics()));
    }
  }
  return batch;
}

std::unique_ptr<DataBatch> DeviceInfoSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [cache_guid, device_info] : all_data_) {
    batch->Put(cache_guid, CopyToEntityData(device_info.specifics()));
  }
  return batch;
}

std::string DeviceInfoSyncBridge::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return DeviceInfoUtil::SpecificsToTag(entity_data.specifics.device_info());
}

std::string DeviceInfoSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return entity_data.specifics.device_info().cache_guid();
}

void DeviceInfoSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Sync is being disabled, so the local DeviceInfo is no longer valid and
  // should be cleared.
  local_device_info_provider_->Clear();
  local_cache_guid_.clear();
  pulse_timer_.Stop();

  // Remove all local data, if sync is being disabled, the user has expressed
  // their desire to not have knowledge about other devices.
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  if (!all_data_.empty()) {
    all_data_.clear();
    NotifyObservers();
  }
}

DataTypeSyncBridge::CommitAttemptFailedBehavior
DeviceInfoSyncBridge::OnCommitAttemptFailed(
    syncer::SyncCommitError commit_error) {
  // DeviceInfo is normally committed once a day and hence it's important to
  // retry on the next sync cycle in case of auth or network errors. For other
  // errors, do not retry to prevent blocking sync for other data types if
  // DeviceInfo entity causes the error. OnCommitAttemptErrors would show that
  // something is wrong with the DeviceInfo entity from the last commit request
  // but those errors are not retried at the moment since it's a very tiny
  // fraction.
  switch (commit_error) {
    case syncer::SyncCommitError::kAuthError:
    case syncer::SyncCommitError::kNetworkError:
      return CommitAttemptFailedBehavior::kShouldRetryOnNextCycle;
    case syncer::SyncCommitError::kBadServerResponse:
    case syncer::SyncCommitError::kServerError:
      return CommitAttemptFailedBehavior::kDontRetryOnNextCycle;
  }
}

bool DeviceInfoSyncBridge::IsSyncing() const {
  // Both conditions are neecessary due to the following possible cases:
  // 1. This method is called from MergeFullSyncData() when IsTrackingMetadata()
  // returns true but |all_data_| is not initialized.
  // 2. |all_data_| is initialized during loading data from the persistent
  // storage on startup but |change_processor| is not initialized yet. It
  // happens when OnReadAllData() is called but OnReadAllMetadata() is not
  // called.
  return change_processor()->IsTrackingMetadata() && !all_data_.empty();
}

const DeviceInfo* DeviceInfoSyncBridge::GetDeviceInfo(
    const std::string& client_id) const {
  const ClientIdToDeviceInfo::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return nullptr;
  }
  if (!IsChromeClient(iter->second.specifics())) {
    return nullptr;
  }
  return &iter->second.device_info();
}

std::vector<const DeviceInfo*> DeviceInfoSyncBridge::GetAllDeviceInfo() const {
  TRACE_EVENT1("sync", "DeviceInfoSyncBridge::GetAllDeviceInfo", "size",
               all_data_.size());
  std::vector<const DeviceInfo*> list;
  for (const auto& [cache_guid, device_info_and_specifics] : all_data_) {
    list.push_back(&device_info_and_specifics.device_info());
  }
  return list;
}

std::vector<const DeviceInfo*> DeviceInfoSyncBridge::GetAllChromeDeviceInfo()
    const {
  TRACE_EVENT1("sync", "DeviceInfoSyncBridge::GetAllChromeDeviceInfo", "size",
               all_data_.size());
  std::vector<const DeviceInfo*> list;
  for (const auto& [cache_guid, device_info_and_specifics] : all_data_) {
    if (IsChromeClient(device_info_and_specifics.specifics())) {
      list.push_back(&device_info_and_specifics.device_info());
    }
  }
  return list;
}

void DeviceInfoSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DeviceInfoSyncBridge::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  return device_info_prefs_->IsRecentLocalCacheGuid(cache_guid);
}

bool DeviceInfoSyncBridge::IsPulseTimerRunningForTest() const {
  return pulse_timer_.IsRunning();
}

void DeviceInfoSyncBridge::ForcePulseForTest() {
  if (pulse_timer_.IsRunning()) {
    pulse_timer_.FireNow();
    return;
  }

  // If |pulse_timer_| is not running, it means that the bridge is not
  // initialized. Set the flag to indicate that the local device info should be
  // reuploaded after initialization has finished.
  force_reupload_for_test_ = true;
}

void DeviceInfoSyncBridge::NotifyObservers() {
  TRACE_EVENT0("sync", "DeviceInfoSyncBridge::NotifyObservers");
  for (auto& observer : observers_) {
    observer.OnDeviceInfoChange();
  }
}

// static
std::optional<ModelError> DeviceInfoSyncBridge::ParseSpecificsOnBackendSequence(
    ClientIdToDeviceInfo* all_data,
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  DCHECK(all_data);
  DCHECK(all_data->empty());
  DCHECK(record_list);

  for (const Record& r : *record_list) {
    DeviceInfoSpecifics specifics;
    if (!specifics.ParseFromString(r.value)) {
      return ModelError(FROM_HERE, "Failed to deserialize specifics.");
    }

    std::string cache_guid = specifics.cache_guid();
    all_data->try_emplace(std::move(cache_guid), std::move(specifics));
  }

  return std::nullopt;
}

void DeviceInfoSyncBridge::StoreSpecifics(DeviceInfoSpecifics specifics,
                                          WriteBatch* batch) {
  const std::string guid = specifics.cache_guid();
  batch->WriteData(guid, specifics.SerializeAsString());
  all_data_.erase(guid);
  all_data_.emplace(guid, std::move(specifics));
}

bool DeviceInfoSyncBridge::DeleteSpecifics(const std::string& guid,
                                           WriteBatch* batch) {
  ClientIdToDeviceInfo::const_iterator iter = all_data_.find(guid);
  if (iter != all_data_.end()) {
    batch->DeleteData(guid);
    all_data_.erase(iter);
    return true;
  } else {
    return false;
  }
}

std::string DeviceInfoSyncBridge::GetLocalClientName() const {
  // |sync_mode_| may not be ready when this function is called.
  if (!sync_mode_) {
    auto device_it = all_data_.find(local_cache_guid_);
    if (device_it != all_data_.end()) {
      return device_it->second.specifics().client_name();
    }
  }

  return sync_mode_ == SyncMode::kFull
             ? local_device_name_info_.personalizable_name
             : local_device_name_info_.model_name;
}

void DeviceInfoSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  CHECK(store_);

  GetLocalDeviceNameInfo(
      base::BindOnce(&DeviceInfoSyncBridge::OnLocalDeviceNameInfoRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoSyncBridge::OnLocalDeviceNameInfoRetrieved(
    LocalDeviceNameInfo local_device_name_info) {
  local_device_name_info_ = std::move(local_device_name_info);

  auto all_data = std::make_unique<ClientIdToDeviceInfo>();
  ClientIdToDeviceInfo* all_data_copy = all_data.get();

  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseSpecificsOnBackendSequence,
                     base::Unretained(all_data_copy)),
      base::BindOnce(&DeviceInfoSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(all_data)));
}

void DeviceInfoSyncBridge::OnReadAllData(
    std::unique_ptr<ClientIdToDeviceInfo> all_data,
    const std::optional<syncer::ModelError>& error) {
  DCHECK(all_data);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  all_data_ = std::move(*all_data);

  store_->ReadAllMetadata(
      base::BindOnce(&DeviceInfoSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoSyncBridge::OnReadAllMetadata(
    const std::optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "DeviceInfoSyncBridge::OnReadAllMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // In the regular case for sync being disabled, wait for MergeFullSyncData()
  // before initializing the LocalDeviceInfoProvider.
  if (!syncer::IsInitialSyncDone(
          metadata_batch->GetDataTypeState().initial_sync_state()) &&
      metadata_batch->GetAllMetadata().empty() && all_data_.empty()) {
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
    return;
  }

  const std::string local_cache_guid_in_metadata =
      metadata_batch->GetDataTypeState().cache_guid();

  // Protect against corrupt local data.
  if (!syncer::IsInitialSyncDone(
          metadata_batch->GetDataTypeState().initial_sync_state()) ||
      local_cache_guid_in_metadata.empty() ||
      all_data_.count(local_cache_guid_in_metadata) == 0) {
    // Data or metadata is off. Just throw everything away and start clean.
    all_data_.clear();
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    change_processor()->ModelReadyToSync(std::make_unique<MetadataBatch>());
    return;
  }

  bool was_local_cache_guid_empty = local_cache_guid_.empty();
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // In rare cases a mismatch between cache GUIDs should cause all sync metadata
  // dropped. In that case, MergeFullSyncData() will eventually follow.
  if (!change_processor()->IsTrackingMetadata()) {
    // In this scenario, ApplyDisableSyncChanges() should have been exercised.
    // If OnSyncStarting() had already been called before, then it must have
    // been called again during ModelReadyToSync().
    DCHECK(was_local_cache_guid_empty == local_cache_guid_.empty());
    DCHECK(all_data_.empty());
    return;
  }

  // If OnSyncStarting() was already called then cache GUID must be the same.
  // Otherwise IsTrackingMetadata would return false due to cache GUID mismatch.
  DCHECK(local_cache_guid_.empty() ||
         local_cache_guid_ == local_cache_guid_in_metadata);
  // If sync already enabled (usual case without data corruption), we can
  // initialize the provider immediately.
  local_cache_guid_ = local_cache_guid_in_metadata;

  // Get stored sync invalidation fields to initialize local device info. This
  // is needed to prevent an unnecessary DeviceInfo commit on browser startup
  // when the SyncInvalidationsService is not initialized.
  auto iter = all_data_.find(local_cache_guid_);
  CHECK(iter != all_data_.end());

  local_device_info_provider_->Initialize(
      local_cache_guid_, GetLocalClientName(),
      local_device_name_info_.manufacturer_name,
      local_device_name_info_.model_name,
      local_device_name_info_.full_hardware_class, &iter->second.device_info());

  // This probably isn't strictly needed, but in case the cache_guid has changed
  // we save the new one to prefs.
  device_info_prefs_->AddLocalCacheGuid(local_cache_guid_);
  ExpireOldEntries();
  if (!ReconcileLocalAndStored()) {
    // If the device info list has not been changed, notify observers explicitly
    // that the list of devices has been successfully loaded from the storage.
    // Otherwise, all observers should already have been notified during
    // ReconcileLocalAndStored().
    NotifyObservers();
  }
}

void DeviceInfoSyncBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

bool DeviceInfoSyncBridge::ReconcileLocalAndStored() {
  TRACE_EVENT0("sync", "DeviceInfoSyncBridge::ReconcileLocalAndStored");
  CHECK(store_);

  const DeviceInfo* current_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(current_info);

  auto iter = all_data_.find(current_info->guid());
  CHECK(iter != all_data_.end());

  // Convert |iter->second| to a DeviceInfo for comparison.
  const DeviceInfo& previous_device_info = iter->second.device_info();
  if (StoredDeviceInfoStillAccurate(&previous_device_info, current_info) &&
      !force_reupload_for_test_) {
    if (pulse_timer_.IsRunning()) {
      // No need to update the |pulse_timer| since nothing has changed.
      return false;
    }

    const base::TimeDelta pulse_delay(DeviceInfoUtil::CalculatePulseDelay(
        GetLastUpdateTime(iter->second.specifics()), Time::Now()));
    if (!pulse_delay.is_zero()) {
      pulse_timer_.Start(FROM_HERE, pulse_delay,
                         base::BindOnce(&DeviceInfoSyncBridge::SendLocalData,
                                        base::Unretained(this)));
      return false;
    }
  }

  // Initiate an additional GetUpdates request if there are new data types
  // enabled (on successful commit).
  const DataTypeSet new_data_types =
      Difference(current_info->interested_data_types(),
                 previous_device_info.interested_data_types());
  if (new_interested_data_types_callback_ && !new_data_types.empty()) {
    device_info_synced_callback_list_.push_back(
        base::BindOnce(new_interested_data_types_callback_, new_data_types));
  }

  // If there was a force-upload request, it has been satisfied now.
  force_reupload_for_test_ = false;

  // Either the local data was updated, or it's time for a pulse update.
  SendLocalData();
  return true;
}

void DeviceInfoSyncBridge::SendLocalData() {
  CHECK(store_);
  CHECK(IsSyncing());
  SendLocalDataWithBatch(store_->CreateWriteBatch());
}

void DeviceInfoSyncBridge::SendLocalDataWithBatch(
    std::unique_ptr<DataTypeStore::WriteBatch> batch) {
  CHECK(store_);
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
  DCHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<DeviceInfoSpecifics> specifics = MakeLocalDeviceSpecifics(
      *local_device_info_provider_->GetLocalDeviceInfo());
  RecordPhoneAsASecurityKeyFieldsAge(*specifics);
  change_processor()->Put(specifics->cache_guid(), CopyToEntityData(*specifics),
                          batch->GetMetadataChangeList());
  StoreSpecifics(std::move(*specifics), batch.get());
  CommitAndNotify(std::move(batch), /*should_notify=*/true);

  pulse_timer_.Start(FROM_HERE, DeviceInfoUtil::GetPulseInterval(),
                     base::BindOnce(&DeviceInfoSyncBridge::SendLocalData,
                                    base::Unretained(this)));
}

void DeviceInfoSyncBridge::CommitAndNotify(std::unique_ptr<WriteBatch> batch,
                                           bool should_notify) {
  CHECK(store_);
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeviceInfoSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  if (should_notify) {
    NotifyObservers();
  }
}

std::map<DeviceInfo::FormFactor, int>
DeviceInfoSyncBridge::CountActiveDevicesByType() const {
  // The algorithm below leverages sync timestamps to give a tight lower bound
  // (modulo clock skew) on how many distinct devices are currently active
  // (where active means being used recently enough as specified by
  // DeviceInfoUtil::kActiveThreshold).
  //
  // Devices of the same OsType and FormFactor that have no overlap
  // between their time-of-use are likely to be the same device (just with a
  // different cache GUID). Thus, the algorithm counts, for each device type
  // separately, the maximum number of devices observed concurrently active.
  // Then returns the maximum. Then aggregates by form factor. Note ASH and
  // LACROS are both counted in desktop, as they have different OsType entries,
  // yet it's probably the same device.

  // The series of relevant events over time, the value being +1 when a device
  // was seen for the first time, and -1 when a device was seen last.
  const base::Time now = base::Time::Now();
  std::map<std::pair<DeviceInfo::FormFactor, DeviceInfo::OsType>,
           std::multimap<base::Time, int>>
      relevant_events;

  for (const auto& [cache_guid, device_info_and_specifics] : all_data_) {
    if (!IsChromeClient(device_info_and_specifics.specifics())) {
      continue;
    }

    if (DeviceInfoUtil::IsActive(
            GetLastUpdateTime(device_info_and_specifics.specifics()), now)) {
      base::Time begin = change_processor()->GetEntityCreationTime(cache_guid);
      base::Time end =
          change_processor()->GetEntityModificationTime(cache_guid);
      // Begin/end timestamps are received from other devices without local
      // sanitizing, so potentially the timestamps could be malformed, and the
      // modification time may predate the creation time.
      if (begin > end) {
        continue;
      }

      DeviceInfo::OsType os_type =
          device_info_and_specifics.device_info().os_type();
      DeviceInfo::FormFactor form_factor =
          device_info_and_specifics.device_info().form_factor();
      relevant_events[{form_factor, os_type}].emplace(begin, 1);
      relevant_events[{form_factor, os_type}].emplace(end, -1);
    }
  }

  std::map<std::pair<DeviceInfo::FormFactor, DeviceInfo::OsType>, int>
      device_count_by_type;
  for (const auto& [type, events] : relevant_events) {
    int max_overlapping = 0;
    int overlapping = 0;
    for (const auto& [time, value] : events) {
      overlapping += value;
      DCHECK_LE(0, overlapping);
      max_overlapping = std::max(max_overlapping, overlapping);
    }
    device_count_by_type[type] = max_overlapping;
    DCHECK_EQ(overlapping, 0);
  }

  std::map<DeviceInfo::FormFactor, int> device_count_by_form_factor;
  for (const auto& [type, counts] : device_count_by_type) {
    device_count_by_form_factor[type.first] += counts;
  }

  return device_count_by_form_factor;
}

void DeviceInfoSyncBridge::ExpireOldEntries() {
  CHECK(store_);
  TRACE_EVENT0("sync", "DeviceInfoSyncBridge::ExpireOldEntries");
  const base::Time expiration_threshold =
      base::Time::Now() - kExpirationThreshold;
  std::unordered_set<std::string> cache_guids_to_expire;
  // Just collecting cache guids to expire to avoid modifying |all_data_| via
  // DeleteSpecifics() while iterating over it.
  for (const auto& [cache_guid, device_info_and_specifics] : all_data_) {
    if (cache_guid != local_cache_guid_ &&
        GetLastUpdateTime(device_info_and_specifics.specifics()) <
            expiration_threshold) {
      cache_guids_to_expire.insert(cache_guid);
    }
  }

  if (cache_guids_to_expire.empty()) {
    return;
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const std::string& cache_guid : cache_guids_to_expire) {
    DeleteSpecifics(cache_guid, batch.get());
    batch->GetMetadataChangeList()->ClearMetadata(cache_guid);
    change_processor()->UntrackEntityForStorageKey(cache_guid);
  }
  CommitAndNotify(std::move(batch), /*should_notify=*/true);
}

}  // namespace syncer
