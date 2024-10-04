// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_PROTO_CONVERSIONS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_PROTO_CONVERSIONS_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/proto/saved_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups {

tab_groups::TabGroupColorId SyncColorToTabGroupColor(
    const sync_pb::SavedTabGroup::SavedTabGroupColor color);

sync_pb::SavedTabGroup_SavedTabGroupColor TabGroupColorToSyncColor(
    const tab_groups::TabGroupColorId color);

SavedTabGroup DataToSavedTabGroup(const proto::SavedTabGroupData& data);

proto::SavedTabGroupData SavedTabGroupToData(const SavedTabGroup& group);

SavedTabGroupTab DataToSavedTabGroupTab(const proto::SavedTabGroupData& data);

proto::SavedTabGroupData SavedTabGroupTabToData(const SavedTabGroupTab& tab);

std::optional<size_t> GroupPositionFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specifics);

std::optional<std::string> GetCreatorCacheGuidFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific);
std::optional<std::string> GetLastUpdaterCacheGuidFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_PROTO_CONVERSIONS_H_
