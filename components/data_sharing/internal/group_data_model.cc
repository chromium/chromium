// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_model.h"

#include <iterator>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/data_sharing/internal/group_data_proto_utils.h"
#include "components/data_sharing/internal/group_data_store.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace data_sharing {

namespace {

const size_t kMaxRecordedGroupEvents = 1000;
const size_t kReadGroupsBatchSize = 50;

VersionToken ComputeVersionToken(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return VersionToken(specifics.consistency_token());
}

bool IsGroupDataStale(const VersionToken& store_version_token,
                      const base::Time& store_last_updated_timestamp,
                      const VersionToken& collaboration_group_version_token) {
  return store_version_token != collaboration_group_version_token ||
         store_last_updated_timestamp.is_null() ||
         store_last_updated_timestamp <
             base::Time::Now() -
                 features::kDataSharingGroupDataPeriodicPollingInterval.Get();
}

}  // namespace

GroupDataModel::GroupDataModel(
    const base::FilePath& data_sharing_dir,
    CollaborationGroupSyncBridge* collaboration_group_sync_bridge,
    DataSharingSDKDelegate* sdk_delegate)
    : group_data_store_(data_sharing_dir,
                        base::BindOnce(&GroupDataModel::OnGroupDataStoreLoaded,
                                       base::Unretained(this))),
      collaboration_group_sync_bridge_(collaboration_group_sync_bridge),
      sdk_delegate_(sdk_delegate) {
  CHECK(collaboration_group_sync_bridge_);
  CHECK(sdk_delegate_);
  collaboration_group_sync_bridge_->AddObserver(this);

  if (collaboration_group_sync_bridge_->IsDataLoaded()) {
    // Bridge might be already loaded at startup, but store loading involves
    // an asynchronous task just started, so it can't be loaded yet.
    CHECK(!is_group_data_store_loaded_);
    is_collaboration_group_bridge_loaded_ = true;
  }
}

GroupDataModel::~GroupDataModel() {
  collaboration_group_sync_bridge_->RemoveObserver(this);
}

void GroupDataModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GroupDataModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<GroupData> GroupDataModel::GetGroup(
    const GroupId& group_id) const {
  if (!IsModelLoaded()) {
    return std::nullopt;
  }

  return group_data_store_.GetGroupData(group_id);
}

std::set<GroupData> GroupDataModel::GetAllGroups() const {
  if (!IsModelLoaded()) {
    return {};
  }

  std::set<GroupData> result;
  for (auto group_id : group_data_store_.GetAllGroupIds()) {
    auto group_data_opt = group_data_store_.GetGroupData(group_id);
    CHECK(group_data_opt.has_value());
    result.emplace(*group_data_opt);
  }
  return result;
}

std::optional<GroupMemberPartialData>
GroupDataModel::GetPossiblyRemovedGroupMember(
    const GroupId& group_id,
    const GaiaId& member_gaia_id) const {
  if (!IsModelLoaded()) {
    return std::nullopt;
  }

  const auto group_data_opt = group_data_store_.GetGroupData(group_id);
  if (!group_data_opt.has_value()) {
    return std::nullopt;
  }
  for (const auto& member : group_data_opt->members) {
    if (member.gaia_id == member_gaia_id) {
      return GroupMemberPartialData::FromGroupMember(member);
    }
  }

  for (const auto& member : group_data_opt->former_members) {
    if (member.gaia_id == member_gaia_id) {
      return GroupMemberPartialData::FromGroupMember(member);
    }
  }

  // TODO(crbug.com/373628741): attempt to read the data from the database with
  // removed members once it is implemented.
  return std::nullopt;
}

std::vector<GroupEvent> GroupDataModel::GetGroupEventsSinceStartup() const {
  return group_events_since_startup_;
}

bool GroupDataModel::IsModelLoaded() const {
  return is_group_data_store_loaded_ && is_collaboration_group_bridge_loaded_;
}

// TODO(crbug.com/301390275): looks like we don't need specific changes anymore
// (see ProcessGroupChanges()), so the parameters can be removed.
void GroupDataModel::OnGroupsUpdated(
    const std::vector<GroupId>& added_group_ids,
    const std::vector<GroupId>& updated_group_ids,
    const std::vector<GroupId>& deleted_group_ids) {
  if (!IsModelLoaded()) {
    return;
  }

  if (!has_ongoing_group_fetch_) {
    ProcessGroupChanges(/*is_initial_load=*/false);
  } else {
    has_pending_changes_ = true;
  }
}

void GroupDataModel::OnCollaborationGroupSyncDataLoaded() {
  is_collaboration_group_bridge_loaded_ = true;
  if (IsModelLoaded()) {
    // Don't notify observers about data being loaded yet - let's process
    // deletions first.
    CHECK(!has_ongoing_group_fetch_);
    ProcessGroupChanges(/*is_initial_load=*/true);
    ScheduleNextPeriodicPolling();
  }
}

void GroupDataModel::OnSyncBridgeUpdateTypeChanged(
    SyncBridgeUpdateType sync_bridge_update_type) {
  for (auto& observer : observers_) {
    observer.OnSyncBridgeUpdateTypeChanged(sync_bridge_update_type);
  }
}

void GroupDataModel::OnGroupDataStoreLoaded(
    GroupDataStore::DBInitStatus status) {
  base::UmaHistogramBoolean("DataSharing.GroupDBInitSuccess",
                            status == GroupDataStore::DBInitStatus::kSuccess);
  if (db_loaded_callback_) {
    std::move(db_loaded_callback_).Run();
  }
  if (status != GroupDataStore::DBInitStatus::kSuccess) {
    return;
  }

  is_group_data_store_loaded_ = true;
  if (IsModelLoaded()) {
    CHECK(!has_ongoing_group_fetch_);
    ProcessGroupChanges(/*is_initial_load=*/true);
    ScheduleNextPeriodicPolling();
  }
}

void GroupDataModel::ProcessGroupChanges(bool is_initial_load) {
  has_pending_changes_ = false;

  std::vector<GroupId> bridge_groups =
      collaboration_group_sync_bridge_->GetCollaborationGroupIds();
  std::vector<GroupId> store_groups = group_data_store_.GetAllGroupIds();

  std::sort(bridge_groups.begin(), bridge_groups.end());
  std::sort(store_groups.begin(), store_groups.end());

  // Handle deletions synchronously, since they don't need SDK call.
  std::vector<GroupId> deleted_group_ids;
  std::ranges::set_difference(store_groups.begin(), store_groups.end(),
                              bridge_groups.begin(), bridge_groups.end(),
                              std::back_inserter(deleted_group_ids));

  std::unordered_map<GroupId, std::optional<GroupData>> deleted_groups;
  for (const auto& group_id : deleted_group_ids) {
    deleted_groups.emplace(group_id, group_data_store_.GetGroupData(group_id));
  }

  group_data_store_.DeleteGroups(deleted_group_ids);
  if (is_initial_load) {
    // This is the first ProcessGroupChanges() call after startup, so notify
    // observers about data being loaded once deletions are processed.
    for (auto& observer : observers_) {
      observer.OnModelLoaded();
    }
  }

  for (auto& group_id : deleted_group_ids) {
    // TODO(crbug.com/377215683): pass the actual event time (at least derived
    // from CollaborationGroupSpecifics).
    base::Time event_time = base::Time::Now();
    for (auto& observer : observers_) {
      MaybeRecordGroupEvent(group_id, GroupEvent::EventType::kGroupRemoved,
                            event_time);
      observer.OnGroupDeleted(group_id, deleted_groups.at(group_id),
                              event_time);
    }
  }

  std::vector<GroupId> added_or_updated_group_ids;
  for (const auto& group_id : bridge_groups) {
    auto collaboration_group_specifics_opt =
        collaboration_group_sync_bridge_->GetSpecifics(group_id);
    CHECK(collaboration_group_specifics_opt.has_value());

    auto store_version_token_opt =
        group_data_store_.GetGroupVersionToken(group_id);
    if (!store_version_token_opt ||
        IsGroupDataStale(
            *store_version_token_opt,
            group_data_store_.GetGroupLastUpdatedTimestamp(group_id),
            ComputeVersionToken(*collaboration_group_specifics_opt))) {
      // Store either doesn't contain corresponding GroupData or contains stale
      // GroupData.
      added_or_updated_group_ids.push_back(group_id);
    }
  }

  if (!added_or_updated_group_ids.empty()) {
    FetchGroupsFromSDK(added_or_updated_group_ids);
  }
}

void GroupDataModel::DoPeriodicPollingAndScheduleNext() {
  if (!has_ongoing_group_fetch_) {
    ProcessGroupChanges(/*is_initial_load=*/false);
  } else {
    has_pending_changes_ = true;
  }

  ScheduleNextPeriodicPolling();
}

void GroupDataModel::ScheduleNextPeriodicPolling() {
  // DoPeriodicPollingAndScheduleNext() simply invokes ProcessGroupChanges()
  // that is no-op if there are no need to refresh any GroupData, thus it is
  // fine to simply call it once per hour for simplicity.
  next_periodic_polling_timer_.Start(
      FROM_HERE, base::Hours(1),
      base::BindOnce(&GroupDataModel::DoPeriodicPollingAndScheduleNext,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GroupDataModel::FetchGroupsFromSDK(
    const std::vector<GroupId>& added_or_updated_groups) {
  has_ongoing_group_fetch_ = true;
  outstanding_batches_ = static_cast<size_t>(
      std::ceil(static_cast<double>(added_or_updated_groups.size()) /
                kReadGroupsBatchSize));

  // The ReadGroups API has a restrictions groups that can be
  // fetched in one request. So, we break the request into batches and issue
  // them in parallel.
  for (size_t i = 0; i < added_or_updated_groups.size();
       i += kReadGroupsBatchSize) {
    std::vector<GroupId> batch(
        added_or_updated_groups.begin() + i,
        added_or_updated_groups.begin() +
            std::min(i + kReadGroupsBatchSize, added_or_updated_groups.size()));

    FetchBatchOfGroupsFromSDK(batch);
  }
}

void GroupDataModel::FetchBatchOfGroupsFromSDK(
    const std::vector<GroupId>& batch) {
  std::map<GroupId, VersionToken> group_versions;
  data_sharing_pb::ReadGroupsParams params;
  for (const GroupId& group_id : batch) {
    auto collaboration_group_specifics_opt =
        collaboration_group_sync_bridge_->GetSpecifics(group_id);
    CHECK(collaboration_group_specifics_opt.has_value());
    group_versions[group_id] =
        ComputeVersionToken(*collaboration_group_specifics_opt);

    data_sharing_pb::ReadGroupsParams::GroupParams* group_params =
        params.add_group_params();
    group_params->set_group_id(group_id.value());
    group_params->set_consistency_token(
        collaboration_group_specifics_opt->consistency_token());
  }

  sdk_delegate_->ReadGroups(
      params, base::BindOnce(&GroupDataModel::OnBatchOfGroupsFetchedFromSDK,
                             weak_ptr_factory_.GetWeakPtr(), group_versions,
                             base::Time::Now()));
}

void GroupDataModel::OnBatchOfGroupsFetchedFromSDK(
    const std::map<GroupId, VersionToken>& requested_groups_and_versions,
    const base::Time& requested_at_timestamp,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        read_groups_result) {
  if (!read_groups_result.has_value()) {
    HandleBatchCompletion();
    // TODO(crbug.com/301390275): handle entire request failure.
    return;
  }

  // TODO(crbug.com/301390275): handle partial failures (e.g. some group_ids
  // being absent from `read_groups_result`).
  for (auto group_data_proto : read_groups_result.value().group_data()) {
    GroupData group_data = GroupDataFromProto(group_data_proto);
    const GroupId group_id = group_data.group_token.group_id;
    if (!collaboration_group_sync_bridge_->GetSpecifics(group_id)) {
      // It is possible that the group has been deleted already.
      continue;
    }

    if (!requested_groups_and_versions.contains(group_id)) {
      // Guard against protocol violation (this group hasn't been requested).
      continue;
    }

    const auto old_group_data_opt = group_data_store_.GetGroupData(group_id);
    group_data_store_.StoreGroupData(requested_groups_and_versions.at(group_id),
                                     requested_at_timestamp, group_data_proto);
    for (auto& observer : observers_) {
      // TODO(crbug.com/377215683): pass the actual event time (at least derived
      // from CollaborationGroupSpecifics).
      if (old_group_data_opt.has_value()) {
        observer.OnGroupUpdated(group_id, base::Time::Now());
      } else {
        MaybeRecordGroupEvent(group_id, GroupEvent::EventType::kGroupAdded,
                              base::Time::Now());
        observer.OnGroupAdded(group_id, base::Time::Now());
      }
    }
    if (old_group_data_opt.has_value()) {
      NotifyObserversAboutChangedMembers(*old_group_data_opt, group_data);
    }
  }

  HandleBatchCompletion();
}

void GroupDataModel::HandleBatchCompletion() {
  if (--outstanding_batches_ == 0) {
    has_ongoing_group_fetch_ = false;
    if (has_pending_changes_) {
      // Some changes happened while the fetch was in flight, process them now.
      ProcessGroupChanges(/*is_initial_load=*/false);
    }
  }
}

void GroupDataModel::NotifyObserversAboutChangedMembers(
    const GroupData& old_group_data,
    const GroupData& new_group_data) {
  std::set<GaiaId> old_members_gaia_ids;
  for (const auto& member : old_group_data.members) {
    old_members_gaia_ids.insert(member.gaia_id);
  }
  std::map<GaiaId, base::Time> new_members;
  for (const auto& member : new_group_data.members) {
    new_members.emplace(member.gaia_id, member.last_updated_time);
  }
  std::map<GaiaId, base::Time> past_members;
  for (const auto& member : new_group_data.former_members) {
    past_members.emplace(member.gaia_id, member.last_updated_time);
  }

  std::vector<std::pair<GaiaId, base::Time>> added_members;
  for (const auto& new_pair : new_members) {
    if (!base::Contains(old_members_gaia_ids, new_pair.first)) {
      added_members.push_back(new_pair);
    }
  }

  std::vector<std::pair<GaiaId, base::Time>> removed_members;
  for (const auto& old_gaia_id : old_members_gaia_ids) {
    if (new_members.find(old_gaia_id) == new_members.end()) {
      auto iter = past_members.find(old_gaia_id);
      base::Time event_time =
          iter != past_members.end() ? iter->second : base::Time::Now();
      removed_members.push_back(std::make_pair(old_gaia_id, event_time));
    }
  }

  for (auto& observer : observers_) {
    for (const auto& added_pair : added_members) {
      base::Time event_time =
          added_pair.second.is_null() ? base::Time::Now() : added_pair.second;
      MaybeRecordGroupEvent(new_group_data.group_token.group_id,
                            GroupEvent::EventType::kMemberAdded, event_time,
                            added_pair.first);
      observer.OnMemberAdded(new_group_data.group_token.group_id,
                             added_pair.first, event_time);
    }
    for (const auto& removed_pair : removed_members) {
      base::Time event_time = removed_pair.second.is_null()
                                  ? base::Time::Now()
                                  : removed_pair.second;
      MaybeRecordGroupEvent(new_group_data.group_token.group_id,
                            GroupEvent::EventType::kMemberRemoved, event_time,
                            removed_pair.first);
      observer.OnMemberRemoved(new_group_data.group_token.group_id,
                               removed_pair.first, event_time);
    }
  }
}

void GroupDataModel::MaybeRecordGroupEvent(
    const GroupId& group_id,
    GroupEvent::EventType event_type,
    base::Time event_time,
    std::optional<GaiaId> affected_member_gaia_id) {
  if (group_events_since_startup_.size() >= kMaxRecordedGroupEvents) {
    // Prevent unbounded growth of the `group_events_since_startup_`. Normally,
    // this should never happen.
    return;
  }
  // All events except kGroupAdded and kGroupRemoved should have an affected
  // member.
  CHECK(event_type == GroupEvent::EventType::kGroupAdded ||
        event_type == GroupEvent::EventType::kGroupRemoved ||
        affected_member_gaia_id.has_value());

  GroupEvent group_event;
  group_event.event_type = event_type;
  group_event.group_id = group_id;
  group_event.affected_member_gaia_id = affected_member_gaia_id;
  group_event.event_time = event_time;
  group_events_since_startup_.push_back(std::move(group_event));
}

GroupDataStore& GroupDataModel::GetGroupDataStoreForTesting() {
  return group_data_store_;
}

void GroupDataModel::SetGroupDataStoreLoadedCallbackForTesting(
    base::OnceClosure db_loaded_callback) {
  db_loaded_callback_ = std::move(db_loaded_callback);
}

}  // namespace data_sharing
