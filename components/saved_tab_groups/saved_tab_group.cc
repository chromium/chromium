// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

SavedTabGroup::SavedTabGroup(
    const std::u16string& title,
    const tab_groups::TabGroupColorId& color,
    const std::vector<SavedTabGroupTab>& urls,
    absl::optional<base::GUID> saved_guid,
    absl::optional<tab_groups::TabGroupId> tab_group_id,
    absl::optional<base::Time> creation_time_windows_epoch_micros,
    absl::optional<base::Time> update_time_windows_epoch_micros)
    : saved_guid_(saved_guid.has_value() ? saved_guid.value()
                                         : base::GUID::GenerateRandomV4()),
      tab_group_id_(tab_group_id),
      title_(title),
      color_(color),
      saved_tabs_(urls),
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.has_value()
              ? creation_time_windows_epoch_micros.value()
              : base::Time::Now()),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.has_value()
              ? update_time_windows_epoch_micros.value()
              : base::Time::Now()) {}

SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;

SavedTabGroup::~SavedTabGroup() = default;

// static
SavedTabGroup SavedTabGroup::FromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specific.group().color());
  const std::u16string& title = base::UTF8ToUTF16(specific.group().title());

  base::GUID guid = base::GUID::ParseLowercase(specific.guid());
  base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.creation_time_windows_epoch_micros()));
  base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.update_time_windows_epoch_micros()));

  return SavedTabGroup(title, color, {}, guid, absl::nullopt, creation_time,
                       update_time);
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroup::ToSpecifics() {
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific =
      std::make_unique<sync_pb::SavedTabGroupSpecifics>();
  pb_specific->set_guid(saved_guid().AsLowercaseString());
  pb_specific->set_creation_time_windows_epoch_micros(
      creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific->set_update_time_windows_epoch_micros(
      update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SavedTabGroup* pb_group = pb_specific->mutable_group();
  pb_group->set_color(TabGroupColorToSyncColor(color()));
  pb_group->set_title(base::UTF16ToUTF8(title()));

  return pb_specific;
}

// static
tab_groups::TabGroupColorId SavedTabGroup::SyncColorToTabGroupColor(
    const sync_pb::SavedTabGroup::SavedTabGroupColor color) {
  switch (color) {
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY:
      return tab_groups::TabGroupColorId::kGrey;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE:
      return tab_groups::TabGroupColorId::kBlue;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_RED:
      return tab_groups::TabGroupColorId::kRed;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_YELLOW:
      return tab_groups::TabGroupColorId::kYellow;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREEN:
      return tab_groups::TabGroupColorId::kGreen;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PINK:
      return tab_groups::TabGroupColorId::kPink;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PURPLE:
      return tab_groups::TabGroupColorId::kPurple;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_CYAN:
      return tab_groups::TabGroupColorId::kCyan;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_ORANGE:
      return tab_groups::TabGroupColorId::kOrange;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_UNSPECIFIED:
      return tab_groups::TabGroupColorId::kGrey;
  }
}

// static
sync_pb::SavedTabGroup_SavedTabGroupColor
SavedTabGroup::TabGroupColorToSyncColor(
    const tab_groups::TabGroupColorId color) {
  switch (color) {
    case tab_groups::TabGroupColorId::kGrey:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY;
    case tab_groups::TabGroupColorId::kBlue:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE;
    case tab_groups::TabGroupColorId::kRed:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_RED;
    case tab_groups::TabGroupColorId::kYellow:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_YELLOW;
    case tab_groups::TabGroupColorId::kGreen:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREEN;
    case tab_groups::TabGroupColorId::kPink:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PINK;
    case tab_groups::TabGroupColorId::kPurple:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PURPLE;
    case tab_groups::TabGroupColorId::kCyan:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_CYAN;
    case tab_groups::TabGroupColorId::kOrange:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_ORANGE;
  }

  NOTREACHED() << "No known conversion for the supplied color.";
}
