// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_PERSONAL_COLLABORATION_DATA_CONVERSION_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_PERSONAL_COLLABORATION_DATA_CONVERSION_UTILS_H_

#include <optional>

#include "base/time/time.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace tab_groups {
class SavedTabGroup;
class SavedTabGroupTab;
}  // namespace tab_groups

namespace tab_groups {

// Convert proto int64 microseconds since Windows-epoch to base::Time.
base::Time DeserializeTime(int64_t proto_time);

// Conversion method to create a SharedTabGroupAccountDataSpecifics object of
// SpecificType::TAB for a given tab_groups::SavedTabGroupTab. Tab group must
// exist and be shared, and tab must have a "last seen" time set.
sync_pb::SharedTabGroupAccountDataSpecifics
CreatePersonalCollaborationSpecificsFromSavedTabGroupTab(
    const tab_groups::SavedTabGroup& group,
    const tab_groups::SavedTabGroupTab& tab,
    const std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>&
        old_specifics);

// Conversion method to create a SharedTabGroupAccountDataSpecifics object of
// SpecificType::TAB_GROUP for a given tab_groups::SavedTabGroup.
sync_pb::SharedTabGroupAccountDataSpecifics
CreatePersonalCollaborationSpecificsFromSharedTabGroup(
    const tab_groups::SavedTabGroup& tab_group,
    const std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>&
        old_specifics);

// Create client tag that consists of the tab guid concatenated with
// collaboration id.
std::string CreateClientTagForSharedTab(const SavedTabGroup& group,
                                        const SavedTabGroupTab& tab);

// Create client tag that consists of the tab group guid concatenated with
// collaboration id.
std::string CreateClientTagForSharedGroup(const SavedTabGroup& group);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_PERSONAL_COLLABORATION_DATA_CONVERSION_UTILS_H_
