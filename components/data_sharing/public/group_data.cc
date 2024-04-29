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

GroupData::GroupData() = default;

GroupData::GroupData(const GroupData&) = default;
GroupData& GroupData::operator=(const GroupData&) = default;

GroupData::GroupData(GroupData&&) = default;
GroupData& GroupData::operator=(GroupData&&) = default;

GroupData::~GroupData() = default;

bool operator<(const GroupData& lhs, const GroupData& rhs) {
  return lhs.group_id < rhs.group_id;
}

}  // namespace data_sharing
