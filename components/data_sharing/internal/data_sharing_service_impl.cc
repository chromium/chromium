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
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

namespace {

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
  NOTREACHED_NORETURN();
}

DataSharingService::PeopleGroupActionOutcome StatusToPeopleGroupActionOutcome(
    const absl::Status& status) {
  if (status.ok()) {
    return DataSharingService::PeopleGroupActionOutcome::kSuccess;
  }
  switch (StatusToPeopleGroupActionFailure(status)) {
    case DataSharingService::PeopleGroupActionFailure::kPersistentFailure:
      return DataSharingService::PeopleGroupActionOutcome::kPersistentFailure;
    case DataSharingService::PeopleGroupActionFailure::kTransientFailure:
      return DataSharingService::PeopleGroupActionOutcome::kTransientFailure;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

DataSharingServiceImpl::DataSharingServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    syncer::OnceModelTypeStoreFactory model_type_store_factory,
    version_info::Channel channel,
    std::unique_ptr<DataSharingSDKDelegate> sdk_delegate)
    : data_sharing_network_loader_(
          std::make_unique<DataSharingNetworkLoaderImpl>(url_loader_factory,
                                                         identity_manager)),
      sdk_delegate_(std::move(sdk_delegate)) {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::COLLABORATION_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel));
  collaboration_group_sync_bridge_ =
      std::make_unique<CollaborationGroupSyncBridge>(
          std::move(change_processor), std::move(model_type_store_factory));
}

DataSharingServiceImpl::~DataSharingServiceImpl() = default;

bool DataSharingServiceImpl::IsEmptyService() {
  return false;
}

DataSharingNetworkLoader*
DataSharingServiceImpl::GetDataSharingNetworkLoader() {
  return data_sharing_network_loader_.get();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
DataSharingServiceImpl::GetCollaborationGroupControllerDelegate() {
  return collaboration_group_sync_bridge_->change_processor()
      ->GetControllerDelegate();
}

void DataSharingServiceImpl::ReadAllGroups(
    base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::ReadGroup(
    const std::string& group_id,
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
  params.add_group_ids(group_id);
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
    const std::string& group_id,
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
  params.set_group_id(group_id);
  sdk_delegate_->DeleteGroup(
      params,
      base::BindOnce(&DataSharingServiceImpl::OnDeleteGroupCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingServiceImpl::InviteMember(
    const std::string& group_id,
    const std::string& invitee_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::RemoveMember(
    const std::string& group_id,
    const std::string& member_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  NOTIMPLEMENTED();
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

void DataSharingServiceImpl::OnDeleteGroupCompleted(
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
    const absl::Status& result) {
  std::move(callback).Run(StatusToPeopleGroupActionOutcome(result));
}

}  // namespace data_sharing
