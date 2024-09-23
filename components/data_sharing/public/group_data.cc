// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/group_data.h"

namespace data_sharing {

GroupMember::GroupMember() = default;

GroupMember::GroupMember(const GroupMember&) = default;
GroupMember& GroupMember::operator=(const GroupMember&) = default;

GroupMember::GroupMember(GroupMember&&) = default;
GroupMember& GroupMember::operator=(GroupMember&&) = default;

GroupMember::~GroupMember() = default;

GroupToken::GroupToken() = default;

GroupToken::GroupToken(GroupId group_id, std::string access_token)
    : group_id(std::move(group_id)), access_token(std::move(access_token)) {}

GroupToken::GroupToken(const GroupToken&) = default;
GroupToken& GroupToken::operator=(const GroupToken&) = default;

GroupToken::GroupToken(GroupToken&&) = default;
GroupToken& GroupToken::operator=(GroupToken&&) = default;

GroupToken::~GroupToken() = default;

bool GroupToken::IsValid() const {
  return !(group_id.value().empty() || access_token.empty());
}

GroupData::GroupData() = default;

GroupData::GroupData(GroupId group_id,
                     std::string display_name,
                     std::vector<GroupMember> members,
                     std::string access_token)
    : group_token(GroupToken(group_id, access_token)),
      display_name(std::move(display_name)),
      members(std::move(members)) {}

GroupData::GroupData(const GroupData&) = default;
GroupData& GroupData::operator=(const GroupData&) = default;

GroupData::GroupData(GroupData&&) = default;
GroupData& GroupData::operator=(GroupData&&) = default;

GroupData::~GroupData() = default;

SharedEntity::SharedEntity() = default;

SharedEntity::SharedEntity(const SharedEntity&) = default;
SharedEntity& SharedEntity::operator=(const SharedEntity&) = default;

SharedEntity::SharedEntity(SharedEntity&&) = default;
SharedEntity& SharedEntity::operator=(SharedEntity&&) = default;

SharedEntity::~SharedEntity() = default;

SharedDataPreview::SharedDataPreview() = default;

SharedDataPreview::SharedDataPreview(const SharedDataPreview&) = default;
SharedDataPreview& SharedDataPreview::operator=(const SharedDataPreview&) =
    default;

SharedDataPreview::SharedDataPreview(SharedDataPreview&&) = default;
SharedDataPreview& SharedDataPreview::operator=(SharedDataPreview&&) = default;

SharedDataPreview::~SharedDataPreview() = default;

bool operator<(const GroupData& lhs, const GroupData& rhs) {
  return lhs.group_token.group_id < rhs.group_token.group_id;
}

}  // namespace data_sharing
