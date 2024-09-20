// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

FakeDataSharingSDKDelegate::FakeDataSharingSDKDelegate() = default;
FakeDataSharingSDKDelegate::~FakeDataSharingSDKDelegate() = default;

std::optional<data_sharing_pb::GroupData> FakeDataSharingSDKDelegate::GetGroup(
    const GroupId& group_id) {
  auto it = groups_.find(group_id);
  if (it != groups_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void FakeDataSharingSDKDelegate::RemoveGroup(const GroupId& group_id) {
  groups_.erase(group_id);
}

void FakeDataSharingSDKDelegate::UpdateGroup(
    const GroupId& group_id,
    const std::string& new_display_name) {
  auto it = groups_.find(group_id);
  ASSERT_TRUE(it != groups_.end());

  it->second.set_display_name(new_display_name);
}

GroupId FakeDataSharingSDKDelegate::AddGroupAndReturnId(
    const std::string& display_name) {
  const GroupId group_id = GroupId(base::NumberToString(next_group_id_++));

  data_sharing_pb::GroupData group_data;
  group_data.set_group_id(group_id.value());
  group_data.set_display_name(display_name);
  groups_[group_id] = group_data;

  return group_id;
}

void FakeDataSharingSDKDelegate::AddMember(const GroupId& group_id,
                                           const std::string& member_gaia_id) {
  auto group_it = groups_.find(group_id);
  ASSERT_TRUE(group_it != groups_.end());

  data_sharing_pb::GroupMember member;
  member.set_gaia_id(member_gaia_id);
  *group_it->second.add_members() = member;
}

void FakeDataSharingSDKDelegate::AddAccount(const std::string& email,
                                            const std::string& gaia_id) {
  email_to_gaia_id_[email] = gaia_id;
}

void FakeDataSharingSDKDelegate::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {}

void FakeDataSharingSDKDelegate::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::CreateGroupResult,
                                  absl::Status>&)> callback) {
  const GroupId group_id = AddGroupAndReturnId(params.display_name());

  data_sharing_pb::CreateGroupResult result;
  *result.mutable_group_data() = groups_[group_id];

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeDataSharingSDKDelegate::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  data_sharing_pb::ReadGroupsResult result;
  for (const auto& raw_group_id : params.group_ids()) {
    const GroupId group_id(raw_group_id);
    if (groups_.find(group_id) != groups_.end()) {
      *result.add_group_data() = groups_[group_id];
    }
  }

  if (result.group_data().empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(absl::Status(
                           absl::StatusCode::kNotFound, "Groups not found"))));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeDataSharingSDKDelegate::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  auto group_it = groups_.find(GroupId(params.group_id()));
  absl::Status status = absl::OkStatus();
  if (group_it != groups_.end()) {
    data_sharing_pb::GroupMember member;
    member.set_gaia_id(params.member_gaia_id());
    *group_it->second.add_members() = member;
  } else {
    status = absl::Status(absl::StatusCode::kNotFound, "Group not found");
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

void FakeDataSharingSDKDelegate::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  auto group_it = groups_.find(GroupId(params.group_id()));
  if (group_it == groups_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  absl::Status(absl::StatusCode::kNotFound,
                                               "Group not found")));
    return;
  }

  absl::Status status = absl::OkStatus();
  auto* group_members = group_it->second.mutable_members();
  auto member_it =
      std::find_if(group_members->begin(), group_members->end(),
                   [&params](const data_sharing_pb::GroupMember& member) {
                     return member.gaia_id() == params.member_gaia_id();
                   });
  if (member_it != group_members->end()) {
    group_members->erase(member_it);
  } else {
    status = absl::Status(absl::StatusCode::kNotFound, "Member not found");
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

void FakeDataSharingSDKDelegate::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  absl::Status status = absl::OkStatus();
  const GroupId group_id(params.group_id());
  if (groups_.find(group_id) != groups_.end()) {
    groups_.erase(group_id);
  } else {
    status = absl::Status(absl::StatusCode::kNotFound, "Group not found");
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

void FakeDataSharingSDKDelegate::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                  absl::Status>&)> callback) {
  auto it = email_to_gaia_id_.find(params.email());
  if (it != email_to_gaia_id_.end()) {
    data_sharing_pb::LookupGaiaIdByEmailResult result;
    result.set_gaia_id(it->second);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     base::unexpected(absl::Status(absl::StatusCode::kNotFound,
                                                   "Account not found"))));
}

void FakeDataSharingSDKDelegate::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                  absl::Status>&)> callback) {
  const GroupId group_id = AddGroupAndReturnId("Test_Group");

  data_sharing_pb::AddAccessTokenResult result;
  *result.mutable_group_data() = groups_[group_id];

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace data_sharing
