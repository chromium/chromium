// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_page_handler_impl.h"

namespace {
data_sharing::mojom::MemberRole MemberRoleToRoleType(
    data_sharing::MemberRole member_role) {
  switch (member_role) {
    case data_sharing::MemberRole::kUnknown:
      return data_sharing::mojom::MemberRole::kUnspecified;
    case data_sharing::MemberRole::kOwner:
      return data_sharing::mojom::MemberRole::kOwner;
    case data_sharing::MemberRole::kMember:
      return data_sharing::mojom::MemberRole::kMember;
    case data_sharing::MemberRole::kInvitee:
      return data_sharing::mojom::MemberRole::kInvitee;
  }
}
}  // namespace

DataSharingInternalsPageHandlerImpl::DataSharingInternalsPageHandlerImpl(
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
    data_sharing::DataSharingService* data_sharing_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      data_sharing_service_(data_sharing_service) {
  // TODO(qinmin): adding this class as an observer to |data_sharing_service_|.
}

DataSharingInternalsPageHandlerImpl::~DataSharingInternalsPageHandlerImpl() =
    default;

void DataSharingInternalsPageHandlerImpl::IsEmptyService(
    IsEmptyServiceCallback callback) {
  std::move(callback).Run(data_sharing_service_->IsEmptyService());
}

void DataSharingInternalsPageHandlerImpl::GetAllGroups(
    GetAllGroupsCallback callback) {
  data_sharing_service_->ReadAllGroups(
      base::BindOnce(&DataSharingInternalsPageHandlerImpl::OnGetAllGroupsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataSharingInternalsPageHandlerImpl::OnGetAllGroupsDone(
    GetAllGroupsCallback callback,
    const data_sharing::DataSharingService::GroupsDataSetOrFailureOutcome&
        group_result) {
  if (group_result.has_value()) {
    std::vector<data_sharing::mojom::GroupDataPtr> group_data;
    for (const auto& data : group_result.value()) {
      auto group_entry = data_sharing::mojom::GroupData::New();
      group_entry->group_id = data.group_token.group_id.value();
      group_entry->display_name = data.display_name;
      for (const auto& member : data.members) {
        auto group_member = data_sharing::mojom::GroupMember::New();
        group_member->display_name = member.display_name;
        group_member->email = member.email;
        group_member->role = MemberRoleToRoleType(member.role);
        group_member->avatar_url = member.avatar_url;
        group_entry->members.emplace_back(std::move(group_member));
      }
      group_entry->access_token = data.group_token.access_token;
      group_data.emplace_back(std::move(group_entry));
    }
    std::move(callback).Run(true, std::move(group_data));
  } else {
    std::move(callback).Run(false,
                            std::vector<data_sharing::mojom::GroupDataPtr>());
  }
}
