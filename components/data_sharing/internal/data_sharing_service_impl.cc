// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"
#include "components/data_sharing/internal/group_data_proto_utils.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

namespace {

const char kGroupIdKey[] = "group_id";
const char kTokenBlobKey[] = "token_blob";

// Should not be called with kOk StatusCode, unless SDK delegate misbehaves by
// passing it as an error value.
DataSharingService::PeopleGroupActionFailure StatusToPeopleGroupActionFailure(
    const absl::Status& status) {
  switch (status.code()) {
    case absl::StatusCode::kOk:
      // Not expected here, treat as a persistent failure.
      return DataSharingService::PeopleGroupActionFailure::kPersistentFailure;
    case absl::StatusCode::kCancelled:
    case absl::StatusCode::kUnknown:
    case absl::StatusCode::kDeadlineExceeded:
    case absl::StatusCode::kResourceExhausted:
    case absl::StatusCode::kAborted:
    case absl::StatusCode::kInternal:
    case absl::StatusCode::kUnavailable:
      return DataSharingService::PeopleGroupActionFailure::kTransientFailure;
    case absl::StatusCode::kNotFound:
    case absl::StatusCode::kAlreadyExists:
    case absl::StatusCode::kPermissionDenied:
    case absl::StatusCode::kFailedPrecondition:
    case absl::StatusCode::kOutOfRange:
    case absl::StatusCode::kUnimplemented:
    case absl::StatusCode::kDataLoss:
    case absl::StatusCode::kUnauthenticated:
      return DataSharingService::PeopleGroupActionFailure::kPersistentFailure;
    default:
      // absl::StatusCode should always have "default:" in `switch()`.
      return DataSharingService::PeopleGroupActionFailure::kPersistentFailure;
  }
}

DataSharingService::PeopleGroupActionOutcome StatusToPeopleGroupActionOutcome(
    const absl::Status& status) {
  if (status.ok()) {
    return DataSharingService::PeopleGroupActionOutcome::kSuccess;
  }
  switch (StatusToPeopleGroupActionFailure(status)) {
    case DataSharingService::PeopleGroupActionFailure::kUnknown:
      return DataSharingService::PeopleGroupActionOutcome::kUnknown;
    case DataSharingService::PeopleGroupActionFailure::kPersistentFailure:
      return DataSharingService::PeopleGroupActionOutcome::kPersistentFailure;
    case DataSharingService::PeopleGroupActionFailure::kTransientFailure:
      return DataSharingService::PeopleGroupActionOutcome::kTransientFailure;
  }
}

}  // namespace

DataSharingServiceImpl::DataSharingServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    syncer::OnceDataTypeStoreFactory data_type_store_factory,
    version_info::Channel channel,
    std::unique_ptr<DataSharingSDKDelegate> sdk_delegate,
    std::unique_ptr<DataSharingUIDelegate> ui_delegate)
    : data_sharing_network_loader_(
          std::make_unique<DataSharingNetworkLoaderImpl>(url_loader_factory,
                                                         identity_manager)),
      sdk_delegate_(std::move(sdk_delegate)),
      ui_delegate_(std::move(ui_delegate)),
      preview_server_proxy_(
          std::make_unique<PreviewServerProxy>(identity_manager,
                                               url_loader_factory)) {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::COLLABORATION_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel));
  collaboration_group_sync_bridge_ =
      std::make_unique<CollaborationGroupSyncBridge>(
          std::move(change_processor), std::move(data_type_store_factory));
  collaboration_group_sync_bridge_->AddObserver(this);
  if (sdk_delegate_) {
    sdk_delegate_->Initialize(data_sharing_network_loader_.get());
  }

  // Initialize ServiceStatus.
  current_status_.collaboration_status = CollaborationStatus::kDisabled;
  if (base::FeatureList::IsEnabled(features::kDataSharingFeature)) {
    current_status_.collaboration_status =
        CollaborationStatus::kEnabledCreateAndJoin;
  }

  // TODO(b/360184707): Add identity manager and sync service to observe state
  // changes.
  current_status_.signin_status = SigninStatus::kNotSignedIn;
  current_status_.sync_status = SyncStatus::kNotSyncing;
}

DataSharingServiceImpl::~DataSharingServiceImpl() {
  collaboration_group_sync_bridge_->RemoveObserver(this);
}

bool DataSharingServiceImpl::IsEmptyService() {
  return false;
}

void DataSharingServiceImpl::AddObserver(
    DataSharingService::Observer* observer) {
  observers_.AddObserver(observer);
}

void DataSharingServiceImpl::RemoveObserver(
    DataSharingService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

DataSharingNetworkLoader*
DataSharingServiceImpl::GetDataSharingNetworkLoader() {
  return data_sharing_network_loader_.get();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
DataSharingServiceImpl::GetCollaborationGroupControllerDelegate() {
  return collaboration_group_sync_bridge_->change_processor()
      ->GetControllerDelegate();
}

void DataSharingServiceImpl::ReadAllGroups(
    base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback) {
  // TODO(crbug.com/301390275): this method should read data from the cache
  // instead of SDK.
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(PeopleGroupActionFailure::kPersistentFailure)));
    return;
  }

  data_sharing_pb::ReadGroupsParams params;
  for (const GroupId& group_id :
       collaboration_group_sync_bridge_->GetCollaborationGroupIds()) {
    params.add_group_ids(group_id.value());
  }

  if (params.group_ids().empty()) {
    // No groups to read.
    std::move(callback).Run(std::set<GroupData>());
    return;
  }

  sdk_delegate_->ReadGroups(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnReadAllGroupsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::ReadGroup(
    const GroupId& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {
  // TODO(crbug.com/301390275): this method should read data from the cache
  // instead of SDK.
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(PeopleGroupActionFailure::kPersistentFailure)));
    return;
  }

  data_sharing_pb::ReadGroupsParams params;
  params.add_group_ids(group_id.value());
  sdk_delegate_->ReadGroups(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnReadSingleGroupCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::CreateGroup(
    const std::string& group_name,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(PeopleGroupActionFailure::kPersistentFailure)));
    return;
  }

  data_sharing_pb::CreateGroupParams params;
  params.set_display_name(group_name);
  sdk_delegate_->CreateGroup(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnCreateGroupCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::DeleteGroup(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       PeopleGroupActionOutcome::kPersistentFailure));
    return;
  }

  data_sharing_pb::DeleteGroupParams params;
  params.set_group_id(group_id.value());
  sdk_delegate_->DeleteGroup(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnSimpleGroupActionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::InviteMember(
    const GroupId& group_id,
    const std::string& invitee_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       PeopleGroupActionOutcome::kPersistentFailure));
    return;
  }

  data_sharing_pb::LookupGaiaIdByEmailParams lookup_params;
  lookup_params.set_email(invitee_email);
  sdk_delegate_->LookupGaiaIdByEmail(
      lookup_params,
      base::BindOnce(
          &DataSharingServiceImpl::OnGaiaIdLookupForAddMemberCompleted,
          weak_ptr_factory_.GetWeakPtr(), group_id, std::move(callback)));
}

void DataSharingServiceImpl::AddMember(
    const GroupId& group_id,
    const std::string& access_token,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       PeopleGroupActionOutcome::kPersistentFailure));
    return;
  }

  data_sharing_pb::AddMemberParams params;
  params.set_group_id(group_id.value());
  params.set_access_token(access_token);
  sdk_delegate_->AddMember(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnSimpleGroupActionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::RemoveMember(
    const GroupId& group_id,
    const std::string& member_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       PeopleGroupActionOutcome::kPersistentFailure));
    return;
  }

  data_sharing_pb::LookupGaiaIdByEmailParams lookup_params;
  lookup_params.set_email(member_email);
  sdk_delegate_->LookupGaiaIdByEmail(
      lookup_params,
      base::BindOnce(
          &DataSharingServiceImpl::OnGaiaIdLookupForRemoveMemberCompleted,
          weak_ptr_factory_.GetWeakPtr(), group_id, std::move(callback)));
}

void DataSharingServiceImpl::OnGroupsUpdated(
    const std::vector<GroupId>& added_group_ids,
    const std::vector<GroupId>& updated_group_ids,
    const std::vector<GroupId>& deleted_group_ids) {
  // TODO(crbug.com/301390275): get rid of this method and corresponding
  // asynchronous logic. Once caching is supported, observers should be
  // notified upon cache updates instead.
  if (!sdk_delegate_) {
    return;
  }

  // Deletions could be notified immediately.
  for (const GroupId& group_id : deleted_group_ids) {
    for (auto& observer : observers_) {
      observer.OnGroupRemoved(group_id);
    }
  }

  // Fetch added and updated groups.
  data_sharing_pb::ReadGroupsParams params;
  for (const GroupId& group_id : added_group_ids) {
    params.add_group_ids(group_id.value());
  }
  for (const GroupId& group_id : updated_group_ids) {
    params.add_group_ids(group_id.value());
  }
  if (params.group_ids().empty()) {
    // No groups to read.
    return;
  }

  sdk_delegate_->ReadGroups(
      params,
      base::BindOnce(
          &DataSharingServiceImpl::OnReadGroupsToNotifyObserversCompleted,
          weak_ptr_factory_.GetWeakPtr(),
          /*added_group_ids=*/
          std::set<GroupId>(added_group_ids.begin(), added_group_ids.end()),
          /*updated_group_ids=*/
          std::set<GroupId>(updated_group_ids.begin(),
                            updated_group_ids.end())));
}

void DataSharingServiceImpl::OnDataLoaded() {
  // TODO(crbug.com/301390275): once caching is supported, this method should
  // be removed and ReadAllGroups() should read cached groups instead. Right
  // now it will read no groups before collaboration group data is loaded and
  // we need to issue another read afterwards and notify observers.
  if (!sdk_delegate_) {
    return;
  }

  data_sharing_pb::ReadGroupsParams params;
  std::vector<GroupId> group_ids =
      collaboration_group_sync_bridge_->GetCollaborationGroupIds();
  for (const GroupId& group_id : group_ids) {
    params.add_group_ids(group_id.value());
  }

  if (params.group_ids().empty()) {
    // No groups to read.
    return;
  }

  sdk_delegate_->ReadGroups(
      params,
      base::BindOnce(
          &DataSharingServiceImpl::OnReadGroupsToNotifyObserversCompleted,
          weak_ptr_factory_.GetWeakPtr(), /*added_group_ids=*/
          std::set<GroupId>(group_ids.begin(), group_ids.end()),
          /*updated_group_ids=*/std::set<GroupId>()));
}

void DataSharingServiceImpl::Shutdown() {
  if (sdk_delegate_) {
    sdk_delegate_->Shutdown();
  }
}

void DataSharingServiceImpl::OnReadSingleGroupCompleted(
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        result) {
  if (result.has_value()) {
    if (result.value().group_data_size() == 1) {
      std::move(callback).Run(GroupDataFromProto(result.value().group_data(0)));
      return;
    } else {
      // SDK indicated success, but didn't return exactly single group,
      // indicating serious bug in SDK.
      std::move(callback).Run(
          base::unexpected(PeopleGroupActionFailure::kPersistentFailure));
      return;
    }
  }

  std::move(callback).Run(
      base::unexpected(StatusToPeopleGroupActionFailure(result.error())));
}

void DataSharingServiceImpl::OnReadAllGroupsCompleted(
    base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        result) {
  if (result.has_value()) {
    std::set<GroupData> groups;
    for (const data_sharing_pb::GroupData& group_data :
         result.value().group_data()) {
      groups.insert(GroupDataFromProto(group_data));
    }
    std::move(callback).Run(groups);
    return;
  }

  std::move(callback).Run(
      base::unexpected(StatusToPeopleGroupActionFailure(result.error())));
}

void DataSharingServiceImpl::OnCreateGroupCompleted(
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
    const base::expected<data_sharing_pb::CreateGroupResult, absl::Status>&
        result) {
  if (result.has_value()) {
    std::move(callback).Run(GroupDataFromProto(result.value().group_data()));
    return;
  }

  std::move(callback).Run(
      base::unexpected(StatusToPeopleGroupActionFailure(result.error())));
}

void DataSharingServiceImpl::OnGaiaIdLookupForAddMemberCompleted(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
    const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                         absl::Status>& result) {
  if (!result.has_value()) {
    std::move(callback).Run(StatusToPeopleGroupActionOutcome(result.error()));
    return;
  }

  data_sharing_pb::AddMemberParams params;
  params.set_group_id(group_id.value());
  params.set_member_gaia_id(result.value().gaia_id());
  sdk_delegate_->AddMember(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnSimpleGroupActionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::OnGaiaIdLookupForRemoveMemberCompleted(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
    const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                         absl::Status>& result) {
  if (!result.has_value()) {
    std::move(callback).Run(StatusToPeopleGroupActionOutcome(result.error()));
    return;
  }

  data_sharing_pb::RemoveMemberParams params;
  params.set_group_id(group_id.value());
  params.set_member_gaia_id(result.value().gaia_id());
  sdk_delegate_->RemoveMember(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnSimpleGroupActionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::OnReadGroupsToNotifyObserversCompleted(
    const std::set<GroupId>& added_group_ids,
    const std::set<GroupId>& updated_group_ids,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        read_groups_result) {
  if (!read_groups_result.has_value()) {
    // TODO(crbug.com/301390275): remove this method or add error handling
    // (retries).
    return;
  }

  for (const data_sharing_pb::GroupData& group_data_proto :
       read_groups_result.value().group_data()) {
    GroupData group_data = GroupDataFromProto(group_data_proto);

    if (added_group_ids.count(group_data.group_token.group_id) > 0) {
      for (auto& observer : observers_) {
        observer.OnGroupAdded(group_data);
      }
    }

    if (updated_group_ids.count(group_data.group_token.group_id) > 0) {
      for (auto& observer : observers_) {
        observer.OnGroupChanged(group_data);
      }
    }
  }
}

void DataSharingServiceImpl::OnSimpleGroupActionCompleted(
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
    const absl::Status& status) {
  std::move(callback).Run(StatusToPeopleGroupActionOutcome(status));
}

CollaborationGroupSyncBridge*
DataSharingServiceImpl::GetCollaborationGroupSyncBridgeForTesting() {
  return collaboration_group_sync_bridge_.get();
}

bool DataSharingServiceImpl::ShouldInterceptNavigationForShareURL(
    const GURL& url) {
  return ParseDataSharingURL(url).has_value();
}

void DataSharingServiceImpl::HandleShareURLNavigationIntercepted(
    const GURL& url) {
  if (!ui_delegate_) {
    return;
  }
  ui_delegate_->HandleShareURLIntercepted(url);
}

std::unique_ptr<GURL> DataSharingServiceImpl::GetDataSharingURL(
    const GroupData& group_data) {
  if (!group_data.group_token.IsValid()) {
    return nullptr;
  }

  GURL url = GURL(data_sharing::features::kDataSharingURL.Get());

  url = net::AppendQueryParameter(url, kGroupIdKey,
                                  group_data.group_token.group_id.value());
  url = net::AppendQueryParameter(url, kTokenBlobKey,
                                  group_data.group_token.access_token);
  return std::make_unique<GURL>(url);
}

DataSharingService::ParseURLResult DataSharingServiceImpl::ParseDataSharingURL(
    const GURL& url) {
  GURL data_sharing_url = GURL(data_sharing::features::kDataSharingURL.Get());
  if (url.host() != data_sharing_url.host() ||
      url.path() != data_sharing_url.path()) {
    return base::unexpected(ParseURLStatus::kHostOrPathMismatchFailure);
  }

  std::string group_id;
  std::string access_token;
  net::GetValueForKeyInQuery(url, kGroupIdKey, &group_id);
  net::GetValueForKeyInQuery(url, kTokenBlobKey, &access_token);

  if (group_id.empty() || access_token.empty()) {
    return base::unexpected(ParseURLStatus::kQueryMissingFailure);
  }

  return GroupToken(GroupId(group_id), access_token);
}

void DataSharingServiceImpl::EnsureGroupVisibility(
    const GroupId& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {
  if (!sdk_delegate_) {
    // Reply in a posted task to avoid reentrance on the calling side.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(PeopleGroupActionFailure::kPersistentFailure)));
    return;
  }

  // TODO(ritikagup@): If a token was added recently then skip adding and return
  // read group.
  data_sharing_pb::AddAccessTokenParams params;
  params.set_group_id(group_id.value());
  sdk_delegate_->AddAccessToken(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnAccessTokenAdded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::GetSharedEntitiesPreview(
    const GroupToken& group_token,
    base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
        callback) {
  preview_server_proxy_->GetSharedDataPreview(group_token, std::move(callback));
}

DataSharingUIDelegate* DataSharingServiceImpl::GetUIDelegate() {
  return ui_delegate_.get();
}

ServiceStatus DataSharingServiceImpl::GetServiceStatus() {
  return current_status_;
}

void DataSharingServiceImpl::OnAccessTokenAdded(
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
    const base::expected<data_sharing_pb::AddAccessTokenResult, absl::Status>&
        result) {
  if (result.has_value()) {
    std::move(callback).Run(GroupDataFromProto(result.value().group_data()));
    return;
  }

  std::move(callback).Run(
      base::unexpected(StatusToPeopleGroupActionFailure(result.error())));
}

}  // namespace data_sharing
