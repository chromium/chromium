// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/personal_collaboration_data_conversion_utils.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace {

// Convert base::Time to proto int64 microseconds since Windows-epoch.
int64_t SerializeTime(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

}  // namespace

namespace tab_groups {

base::Time DeserializeTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

sync_pb::SharedTabGroupAccountDataSpecifics
CreatePersonalCollaborationSpecificsFromSavedTabGroupTab(
    const tab_groups::SavedTabGroup& group,
    const tab_groups::SavedTabGroupTab& tab,
    const std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>&
        old_specifics) {
  CHECK(group.is_shared_tab_group());

  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  if (old_specifics.has_value()) {
    specifics = old_specifics.value();
  }

  specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  specifics.set_collaboration_id(group.collaboration_id()->value());
  specifics.set_version(
      tab_groups::kCurrentSharedTabGroupAccountDataSpecificsProtoVersion);

  sync_pb::SharedTabDetails* tab_details =
      specifics.mutable_shared_tab_details();
  tab_details->set_shared_tab_group_guid(
      group.saved_guid().AsLowercaseString());
  tab_details->set_last_seen_timestamp_windows_epoch(
      SerializeTime(tab.last_seen_time().value()));

  return specifics;
}

sync_pb::SharedTabGroupAccountDataSpecifics
CreatePersonalCollaborationSpecificsFromSharedTabGroup(
    const tab_groups::SavedTabGroup& tab_group,
    const std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>&
        old_specifics) {
  CHECK(tab_group.is_shared_tab_group());

  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  if (old_specifics.has_value()) {
    specifics = old_specifics.value();
  }

  specifics.set_guid(tab_group.saved_guid().AsLowercaseString());
  specifics.set_collaboration_id(tab_group.collaboration_id()->value());
  specifics.set_version(
      tab_groups::kCurrentSharedTabGroupAccountDataSpecificsProtoVersion);

  sync_pb::SharedTabGroupDetails* tab_group_details =
      specifics.mutable_shared_tab_group_details();
  if (tab_group.position().has_value()) {
    tab_group_details->set_pinned_position(tab_group.position().value());
  }

  return specifics;
}

std::string CreateClientTagForSharedTab(const SavedTabGroup& group,
                                        const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString() + "|" +
         group.collaboration_id().value().value();
}

std::string CreateClientTagForSharedGroup(const SavedTabGroup& group) {
  return group.saved_guid().AsLowercaseString() + "|" +
         group.collaboration_id().value().value();
}

}  // namespace tab_groups
