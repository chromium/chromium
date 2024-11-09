// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/group_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"

namespace data_sharing {

GroupMember::GroupMember() = default;

GroupMember::GroupMember(const GroupMember&) = default;
GroupMember& GroupMember::operator=(const GroupMember&) = default;

GroupMember::GroupMember(GroupMember&&) = default;
GroupMember& GroupMember::operator=(GroupMember&&) = default;

GroupMember::~GroupMember() = default;

// static
GroupMemberPartialData GroupMemberPartialData::FromGroupMember(
    const GroupMember& member) {
  GroupMemberPartialData result;
  result.gaia_id = member.gaia_id;
  result.display_name = member.display_name;
  result.email = member.email;
  result.avatar_url = member.avatar_url;
  return result;
}

GroupMemberPartialData::GroupMemberPartialData() = default;

GroupMemberPartialData::GroupMemberPartialData(const GroupMemberPartialData&) =
    default;
GroupMemberPartialData& GroupMemberPartialData::operator=(
    const GroupMemberPartialData&) = default;

GroupMemberPartialData::GroupMemberPartialData(GroupMemberPartialData&&) =
    default;
GroupMemberPartialData& GroupMemberPartialData::operator=(
    GroupMemberPartialData&&) = default;

GroupMemberPartialData::~GroupMemberPartialData() = default;

GroupToken::GroupToken() = default;

GroupToken::GroupToken(GroupId group_id, const std::string& access_token)
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

TabPreview::TabPreview(const GURL& url) : url(url) {}
TabPreview::TabPreview(const TabPreview&) = default;
TabPreview& TabPreview::operator=(const TabPreview&) = default;

TabPreview::TabPreview(TabPreview&&) = default;
TabPreview& TabPreview::operator=(TabPreview&&) = default;
TabPreview::~TabPreview() = default;

std::string TabPreview::GetDisplayUrl() const {
  return base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url));
}

SharedDataPreview::SharedDataPreview() = default;
SharedDataPreview::SharedDataPreview(const SharedDataPreview&) = default;
SharedDataPreview& SharedDataPreview::operator=(const SharedDataPreview&) =
    default;

SharedDataPreview::SharedDataPreview(SharedDataPreview&&) = default;
SharedDataPreview& SharedDataPreview::operator=(SharedDataPreview&&) = default;
SharedDataPreview::~SharedDataPreview() = default;

SharedTabGroupPreview::SharedTabGroupPreview() = default;
SharedTabGroupPreview::SharedTabGroupPreview(const SharedTabGroupPreview&) =
    default;
SharedTabGroupPreview& SharedTabGroupPreview::operator=(
    const SharedTabGroupPreview&) = default;

SharedTabGroupPreview::SharedTabGroupPreview(SharedTabGroupPreview&&) = default;
SharedTabGroupPreview& SharedTabGroupPreview::operator=(
    SharedTabGroupPreview&&) = default;

SharedTabGroupPreview::~SharedTabGroupPreview() = default;

bool operator<(const GroupData& lhs, const GroupData& rhs) {
  return lhs.group_token.group_id < rhs.group_token.group_id;
}

bool operator==(const GroupToken& lhs, const GroupToken& rhs) {
  return lhs.group_id == rhs.group_id && lhs.access_token == rhs.access_token;
}

bool operator<(const GroupToken& lhs, const GroupToken& rhs) {
  return lhs.group_id < rhs.group_id;
}

}  // namespace data_sharing
