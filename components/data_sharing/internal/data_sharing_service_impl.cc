// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace data_sharing {

DataSharingServiceImpl::DataSharingServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    syncer::OnceModelTypeStoreFactory model_type_store_factory,
    version_info::Channel channel)
    : data_sharing_network_loader_(
          std::make_unique<DataSharingNetworkLoaderImpl>(url_loader_factory,
                                                         identity_manager)) {
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
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::CreateGroup(
    const std::string& group_name,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::DeleteGroup(
    const std::string& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::InviteMember(
    const std::string& group_id,
    const std::string& invitee_gaia_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingServiceImpl::RemoveMember(
    const std::string& group_id,
    const std::string& member_gaia_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {
  NOTIMPLEMENTED();
}

}  // namespace data_sharing
