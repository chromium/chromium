// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/group_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"

namespace data_sharing {

GroupMember::GroupMember() = default;

GroupMember::GroupMember(GaiaId gaia_id,
                         std::string display_name,
                         std::string email,
                         MemberRole role,
                         GURL avatar_url,
                         std::string given_name)
    : gaia_id(gaia_id),
      display_name(display_name),
      email(email),
      role(role),
      avatar_url(avatar_url),
      given_name(given_name) {}

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
  result.given_name = member.given_name;
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

GroupMember GroupMemberPartialData::ToGroupMember() {
  GroupMember member;
  member.gaia_id = gaia_id;
  member.display_name = display_name;
  member.email = email;
  member.avatar_url = avatar_url;
  member.given_name = given_name;
  return member;
}

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
                     std::string display_name_param,
                     std::vector<GroupMember> members_param,
                     std::vector<GroupMember> former_members_param,
                     std::string access_token,
                    GroupEnabledStatus enabled_status)
    : group_token(GroupToken(group_id, access_token)),
      display_name(std::move(display_name_param)),
      members(std::move(members_param)),
      former_members(std::move(former_members_param)),
      enabled_status(enabled_status) {}

GroupData::GroupData(const GroupData&) = default;
GroupData& GroupData::operator=(const GroupData&) = default;

GroupData::GroupData(GroupData&&) = default;
GroupData& GroupData::operator=(GroupData&&) = default;

GroupData::~GroupData() = default;

GroupEvent::GroupEvent() = default;

GroupEvent::GroupEvent(const GroupEvent&) = default;
GroupEvent& GroupEvent::operator=(const GroupEvent&) = default;

GroupEvent::GroupEvent(GroupEvent&&) = default;
GroupEvent& GroupEvent::operator=(GroupEvent&&) = default;

GroupEvent::GroupEvent(EventType event_type,
                       const GroupId& group_id,
                       const std::optional<GaiaId>& affected_member_gaia_id,
                       const base::Time& event_time)
    : event_type(event_type),
      group_id(group_id),
      affected_member_gaia_id(affected_member_gaia_id),
      event_time(event_time) {}

GroupEvent::~GroupEvent() = default;

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
  return std::tie(lhs.group_id, lhs.access_token) <
         std::tie(rhs.group_id, rhs.access_token);
}

}  // namespace data_sharing
