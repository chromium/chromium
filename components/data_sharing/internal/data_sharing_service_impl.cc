// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

namespace data_sharing {

DataSharingServiceImpl::DataSharingServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : data_sharing_network_loader_(
          std::make_unique<DataSharingNetworkLoaderImpl>(url_loader_factory,
                                                         identity_manager)) {}

DataSharingServiceImpl::~DataSharingServiceImpl() = default;

bool DataSharingServiceImpl::IsEmptyService() {
  return false;
}

DataSharingNetworkLoader*
DataSharingServiceImpl::GetDataSharingNetworkLoader() {
  return data_sharing_network_loader_.get();
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
