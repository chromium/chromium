// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/empty_data_sharing_service.h"

#include "base/functional/callback.h"

namespace data_sharing {

EmptyDataSharingService::EmptyDataSharingService() = default;

EmptyDataSharingService::~EmptyDataSharingService() = default;

bool EmptyDataSharingService::IsEmptyService() {
  return true;
}

DataSharingNetworkLoader*
EmptyDataSharingService::GetDataSharingNetworkLoader() {
  return nullptr;
}

void EmptyDataSharingService::ReadAllGroups(
    base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::ReadGroup(
    const std::string& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::CreateGroup(
    const std::string& group_name,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::DeleteGroup(
    const std::string& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::InviteMember(
    const std::string& group_id,
    const std::string& invitee_gaia_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::RemoveMember(
    const std::string& group_id,
    const std::string& member_gaia_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

}  // namespace data_sharing
