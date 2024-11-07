// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_model.h"

#include <iterator>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "components/data_sharing/internal/group_data_proto_utils.h"
#include "components/data_sharing/internal/group_data_store.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"

namespace data_sharing {

namespace {

VersionToken ComputeVersionToken(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return VersionToken(specifics.consistency_token());
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
    const std::string& member_gaia_id) const {
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

  // TODO(crbug.com/373628741): attempt to read the data from the database with
  // removed members once it is implemented.
  return std::nullopt;
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
  base::ranges::set_difference(store_groups.begin(), store_groups.end(),
                               bridge_groups.begin(), bridge_groups.end(),
                               std::back_inserter(deleted_group_ids));

  group_data_store_.DeleteGroups(deleted_group_ids);
  if (is_initial_load) {
    // This is the first ProcessGroupChanges() call after startup, so notify
    // observers about data being loaded once deletions are processed.
    for (auto& observer : observers_) {
      observer.OnModelLoaded();
    }
  }

  for (auto& group_id : deleted_group_ids) {
    for (auto& observer : observers_) {
      // TODO(crbug.com/377215683): pass the actual event time (at least derived
      // from CollaborationGroupSpecifics).
      observer.OnGroupDeleted(group_id, base::Time::Now());
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
        *store_version_token_opt !=
            ComputeVersionToken(*collaboration_group_specifics_opt)) {
      // Store either doesn't contain corresponding GroupData or contains stale
      // GroupData.
      added_or_updated_group_ids.push_back(group_id);
    }
  }

  if (!added_or_updated_group_ids.empty()) {
    FetchGroupsFromSDK(added_or_updated_group_ids);
  }
}

void GroupDataModel::FetchGroupsFromSDK(
    const std::vector<GroupId>& added_or_updated_groups) {
  if (!sdk_delegate_) {
    return;
  }

  has_ongoing_group_fetch_ = true;

  std::map<GroupId, VersionToken> group_versions;
  data_sharing_pb::ReadGroupsParams params;
  for (const GroupId& group_id : added_or_updated_groups) {
    // TODO(crbug.com/301390275): pass `consistency_token`.
    params.add_group_ids(group_id.value());

    auto collaboration_group_specifics_opt =
        collaboration_group_sync_bridge_->GetSpecifics(group_id);
    CHECK(collaboration_group_specifics_opt.has_value());
    group_versions[group_id] =
        ComputeVersionToken(*collaboration_group_specifics_opt);
  }

  sdk_delegate_->ReadGroups(
      params, base::BindOnce(&GroupDataModel::OnGroupsFetchedFromSDK,
                             weak_ptr_factory_.GetWeakPtr(), group_versions));
}

void GroupDataModel::OnGroupsFetchedFromSDK(
    const std::map<GroupId, VersionToken>& requested_groups_and_versions,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        read_groups_result) {
  if (!read_groups_result.has_value()) {
    has_ongoing_group_fetch_ = false;
    if (has_pending_changes_) {
      // Some changes happened while the fetch was in flight, process them now.
      ProcessGroupChanges(/*is_initial_load=*/false);
    }
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
                                     group_data);
    for (auto& observer : observers_) {
      // TODO(crbug.com/377215683): pass the actual event time (at least derived
      // from CollaborationGroupSpecifics).
      if (old_group_data_opt.has_value()) {
        observer.OnGroupUpdated(group_id, base::Time::Now());
      } else {
        observer.OnGroupAdded(group_id, base::Time::Now());
      }
    }
    if (old_group_data_opt.has_value()) {
      NotifyObserversAboutChangedMembers(*old_group_data_opt, group_data);
    }
  }

  has_ongoing_group_fetch_ = false;
  if (has_pending_changes_) {
    // Some changes happened while the fetch was in flight, process them now.
    ProcessGroupChanges(/*is_initial_load=*/false);
  }
}

void GroupDataModel::NotifyObserversAboutChangedMembers(
    const GroupData& old_group_data,
    const GroupData& new_group_data) {
  std::vector<std::string> old_members_gaia_ids;
  for (const auto& member : old_group_data.members) {
    old_members_gaia_ids.push_back(member.gaia_id);
  }
  std::vector<std::string> new_members_gaia_ids;
  for (const auto& member : new_group_data.members) {
    new_members_gaia_ids.push_back(member.gaia_id);
  }

  std::vector<std::string> added_members_gaia_ids;
  base::ranges::set_difference(
      new_members_gaia_ids.begin(), new_members_gaia_ids.end(),
      old_members_gaia_ids.begin(), old_members_gaia_ids.end(),
      std::back_inserter(added_members_gaia_ids));

  std::vector<std::string> removed_members_gaia_ids;
  base::ranges::set_difference(
      old_members_gaia_ids.begin(), old_members_gaia_ids.end(),
      new_members_gaia_ids.begin(), new_members_gaia_ids.end(),
      std::back_inserter(removed_members_gaia_ids));

  for (auto& observer : observers_) {
    // TODO(crbug.com/377215683): pass the actual event time (at least derived
    // from CollaborationGroupSpecifics).
    for (auto& member_gaia_id : added_members_gaia_ids) {
      observer.OnMemberAdded(new_group_data.group_token.group_id,
                             member_gaia_id, base::Time::Now());
    }
    for (auto& member_gaia_id : removed_members_gaia_ids) {
      observer.OnMemberRemoved(new_group_data.group_token.group_id,
                               member_gaia_id, base::Time::Now());
    }
  }
}

GroupDataStore& GroupDataModel::GetGroupDataStoreForTesting() {
  return group_data_store_;
}

void GroupDataModel::SetGroupDataStoreLoadedCallbackForTesting(
    base::OnceClosure db_loaded_callback) {
  db_loaded_callback_ = std::move(db_loaded_callback);
}

}  // namespace data_sharing
