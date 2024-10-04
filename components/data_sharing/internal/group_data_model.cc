// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_model.h"

#include <iterator>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "components/data_sharing/internal/group_data_proto_utils.h"
#include "components/data_sharing/internal/group_data_store.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"

namespace data_sharing {

namespace {

base::FilePath GetGroupDataStoreDBPath(const base::FilePath& data_sharing_dir) {
  return data_sharing_dir.Append(FILE_PATH_LITERAL("DataSharingDB"));
}

VersionToken ComputeVersionToken(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return VersionToken(base::NumberToString(
      specifics.changed_at_timestamp_millis_since_unix_epoch()));
}

}  // namespace

GroupDataModel::GroupDataModel(
    const base::FilePath& data_sharing_dir,
    CollaborationGroupSyncBridge* collaboration_group_sync_bridge,
    DataSharingSDKDelegate* sdk_delegate)
    : group_data_store_(GetGroupDataStoreDBPath(data_sharing_dir),
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

bool GroupDataModel::IsModelLoaded() const {
  return is_group_data_store_loaded_ && is_collaboration_group_bridge_loaded_;
}

// TODO(crbug.com/301390275): looks like we don't need to distinguish added and
// updated groups here, merge them into single parameter (they could be
// distinguished by their presence in `group_data_store_`).
void GroupDataModel::OnGroupsUpdated(
    const std::vector<GroupId>& added_group_ids,
    const std::vector<GroupId>& updated_group_ids,
    const std::vector<GroupId>& deleted_group_ids) {
  if (!IsModelLoaded()) {
    return;
  }
  group_data_store_.DeleteGroups(deleted_group_ids);
  for (auto& observer : observers_) {
    for (auto& group_id : deleted_group_ids) {
      observer.OnGroupDeleted(group_id);
    }
  }

  std::vector<GroupId> added_or_updated_groups = added_group_ids;
  std::copy(updated_group_ids.begin(), updated_group_ids.end(),
            std::back_inserter(added_or_updated_groups));
  // Observers will be notified once groups are actually fetched from the SDK.
  FetchGroupsFromSDK(added_or_updated_groups);
}

void GroupDataModel::OnDataLoaded() {
  is_collaboration_group_bridge_loaded_ = true;
  if (IsModelLoaded()) {
    // Don't notify observers about data being loaded yet - let's process
    // deletions first.
    ProcessInitialData();
  }
}

void GroupDataModel::OnGroupDataStoreLoaded(
    GroupDataStore::DBInitStatus status) {
  if (status != GroupDataStore::DBInitStatus::kSuccess) {
    // TODO(crbug.com/301390275): perhaps some error handling is needed in this
    // case (at least metrics).
    return;
  }

  is_group_data_store_loaded_ = true;
  if (IsModelLoaded()) {
    ProcessInitialData();
  }
}

void GroupDataModel::ProcessInitialData() {
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
  for (auto& observer : observers_) {
    observer.OnModelLoaded();
  }
  for (auto& group_id : deleted_group_ids) {
    for (auto& observer : observers_) {
      observer.OnGroupDeleted(group_id);
    }
  }
  // TODO(crbug.com/301390275): notify observers about deletions and the fact
  // that data is loaded.

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

  FetchGroupsFromSDK(added_or_updated_group_ids);
}

void GroupDataModel::FetchGroupsFromSDK(
    const std::vector<GroupId>& added_or_updated_groups) {
  if (!sdk_delegate_) {
    return;
  }

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

    const bool is_existing_group =
        group_data_store_.GetGroupData(group_id).has_value();
    group_data_store_.StoreGroupData(requested_groups_and_versions.at(group_id),
                                     group_data);
    // TODO(crbug.com/301390275): compute deltas for observers.
    for (auto& observer : observers_) {
      if (is_existing_group) {
        observer.OnGroupUpdated(group_id);
      } else {
        observer.OnGroupAdded(group_id);
      }
    }
  }
}

GroupDataStore& GroupDataModel::GetGroupDataStoreForTesting() {
  return group_data_store_;
}

}  // namespace data_sharing
