// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/saved_tab_group_proto_conversions.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"

namespace tab_groups {
namespace {

// The current schema version of the SavedTabGroupData proto.
const int kCurrentSchemaVersion = 1;

base::Time TimeFromWindowsEpochMicros(int64_t time_windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(time_windows_epoch_micros));
}

std::optional<sync_pb::AttributionMetadata::Attribution>
GetAttributionFromSpecifics(const sync_pb::SavedTabGroupSpecifics& specific,
                            bool for_creation) {
  if (specific.has_attribution_metadata()) {
    const auto& attribution_metadata = specific.attribution_metadata();
    if (for_creation) {
      if (attribution_metadata.has_created()) {
        return attribution_metadata.created();
      }
    } else {
      if (attribution_metadata.has_updated()) {
        return attribution_metadata.updated();
      }
    }
  }

  return std::nullopt;
}

std::optional<std::string> GetCacheGuidFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific,
    bool is_created) {
  auto attribution = GetAttributionFromSpecifics(specific, is_created);
  if (attribution.has_value()) {
    if (attribution->has_device_info()) {
      const auto& device_info = attribution->device_info();
      if (device_info.has_cache_guid()) {
        return device_info.cache_guid();
      }
    }
  }

  return std::nullopt;
}

}  // namespace

std::optional<size_t> GroupPositionFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  // In v1 we always set tab group position even if the proto is not set, which
  // gives a default position of 0. In v2 we leave the position unset if the
  // proto is not set for unpinned tab groups.
  if (!IsTabGroupsSaveUIUpdateEnabled()) {
    return specifics.group().position();
  }
  if (specifics.group().has_pinned_position()) {
    return specifics.group().pinned_position();
  }
  return std::nullopt;
}

tab_groups::TabGroupColorId SyncColorToTabGroupColor(
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

sync_pb::SavedTabGroup_SavedTabGroupColor TabGroupColorToSyncColor(
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
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY;
  }

  NOTREACHED_IN_MIGRATION() << "No known conversion for the supplied color.";
}

SavedTabGroup DataToSavedTabGroup(const proto::SavedTabGroupData& data) {
  const auto& specific = data.specifics();
  CHECK(specific.has_group());

  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specific.group().color());
  const std::u16string& title = base::UTF8ToUTF16(specific.group().title());
  std::optional<size_t> position = GroupPositionFromSpecifics(specific);
  const base::Uuid guid = base::Uuid::ParseLowercase(specific.guid());
  const base::Time creation_time =
      TimeFromWindowsEpochMicros(specific.creation_time_windows_epoch_micros());
  const base::Time update_time =
      TimeFromWindowsEpochMicros(specific.update_time_windows_epoch_micros());

  std::optional<LocalTabGroupID> local_group_id;
  if (data.has_local_tab_group_data() &&
      data.local_tab_group_data().has_local_group_id()) {
    if (AreLocalIdsPersisted()) {
      local_group_id = LocalTabGroupIDFromString(
          data.local_tab_group_data().local_group_id());
    }
  }

  std::optional<std::string> creator_cache_guid =
      GetCreatorCacheGuidFromSpecifics(specific);
  std::optional<std::string> last_updater_cache_guid =
      GetLastUpdaterCacheGuidFromSpecifics(specific);

  bool created_before_syncing_tab_groups = false;
  base::Time last_user_interaction_time;
  if (data.has_local_tab_group_data()) {
    created_before_syncing_tab_groups =
        data.local_tab_group_data().created_before_syncing_tab_groups();
    last_user_interaction_time = TimeFromWindowsEpochMicros(
        data.local_tab_group_data()
            .last_user_interaction_time_windows_epoch_micros());
  }

  SavedTabGroup group = SavedTabGroup(
      title, color, {}, position, guid, local_group_id,
      std::move(creator_cache_guid), std::move(last_updater_cache_guid),
      created_before_syncing_tab_groups, creation_time);
  group.SetUpdateTimeWindowsEpochMicros(update_time);
  group.SetLastUserInteractionTime(last_user_interaction_time);

  return group;
}

proto::SavedTabGroupData SavedTabGroupToData(const SavedTabGroup& group) {
  proto::SavedTabGroupData pb_data;
  auto* pb_specific = pb_data.mutable_specifics();
  pb_specific->set_guid(group.saved_guid().AsLowercaseString());
  pb_specific->set_creation_time_windows_epoch_micros(
      group.creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific->set_update_time_windows_epoch_micros(
      group.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SavedTabGroup* pb_group = pb_specific->mutable_group();
  pb_group->set_color(TabGroupColorToSyncColor(group.color()));
  pb_group->set_title(base::UTF16ToUTF8(group.title()));
  if (group.creator_cache_guid().has_value()) {
    pb_specific->mutable_attribution_metadata()
        ->mutable_created()
        ->mutable_device_info()
        ->set_cache_guid(group.creator_cache_guid().value());
  }

  if (group.last_updater_cache_guid().has_value()) {
    pb_specific->mutable_attribution_metadata()
        ->mutable_updated()
        ->mutable_device_info()
        ->set_cache_guid(group.last_updater_cache_guid().value());
  }

  if (group.position().has_value()) {
    if (IsTabGroupsSaveUIUpdateEnabled()) {
      pb_group->set_pinned_position(group.position().value());
    } else {
      pb_group->set_position(group.position().value());
    }
  }

  if (AreLocalIdsPersisted()) {
    const auto& local_group_id = group.local_group_id();
    if (local_group_id.has_value()) {
      pb_data.mutable_local_tab_group_data()->set_local_group_id(
          LocalTabGroupIDToString(local_group_id.value()));
    }
  }

  pb_data.mutable_local_tab_group_data()->set_created_before_syncing_tab_groups(
      group.created_before_syncing_tab_groups());
  pb_data.mutable_local_tab_group_data()
      ->set_last_user_interaction_time_windows_epoch_micros(
          group.last_user_interaction_time()
              .ToDeltaSinceWindowsEpoch()
              .InMicroseconds());

  pb_data.set_version(kCurrentSchemaVersion);

  // Note: When adding a new syncable field, also update IsSyncEquivalent().

  return pb_data;
}

SavedTabGroupTab DataToSavedTabGroupTab(const proto::SavedTabGroupData& data) {
  const auto& specific = data.specifics();
  CHECK(specific.has_tab());

  const base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.creation_time_windows_epoch_micros()));
  const base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.update_time_windows_epoch_micros()));

  std::optional<std::string> creator_cache_guid =
      GetCreatorCacheGuidFromSpecifics(specific);
  std::optional<std::string> last_updater_cache_guid =
      GetLastUpdaterCacheGuidFromSpecifics(specific);

  SavedTabGroupTab tab(
      GURL(specific.tab().url()), base::UTF8ToUTF16(specific.tab().title()),
      base::Uuid::ParseLowercase(specific.tab().group_guid()),
      specific.tab().position(), base::Uuid::ParseLowercase(specific.guid()),
      std::nullopt, std::move(creator_cache_guid),
      std::move(last_updater_cache_guid), creation_time, update_time);
  return tab;
}

proto::SavedTabGroupData SavedTabGroupTabToData(const SavedTabGroupTab& tab) {
  proto::SavedTabGroupData pb_data;
  auto* pb_specific = pb_data.mutable_specifics();

  pb_specific->set_guid(tab.saved_tab_guid().AsLowercaseString());
  pb_specific->set_creation_time_windows_epoch_micros(
      tab.creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific->set_update_time_windows_epoch_micros(
      tab.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  if (tab.creator_cache_guid().has_value()) {
    pb_specific->mutable_attribution_metadata()
        ->mutable_created()
        ->mutable_device_info()
        ->set_cache_guid(tab.creator_cache_guid().value());
  }

  if (tab.last_updater_cache_guid().has_value()) {
    pb_specific->mutable_attribution_metadata()
        ->mutable_updated()
        ->mutable_device_info()
        ->set_cache_guid(tab.last_updater_cache_guid().value());
  }

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific->mutable_tab();
  pb_tab->set_url(tab.url().spec());
  pb_tab->set_group_guid(tab.saved_group_guid().AsLowercaseString());
  pb_tab->set_title(base::UTF16ToUTF8(tab.title()));
  pb_tab->set_position(tab.position().value());
  // Note: When adding a new syncable field, also update IsSyncEquivalent().

  pb_data.set_version(kCurrentSchemaVersion);
  return pb_data;
}

std::optional<std::string> GetCreatorCacheGuidFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  return GetCacheGuidFromSpecifics(specific, /*is_created=*/true);
}

std::optional<std::string> GetLastUpdaterCacheGuidFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  return GetCacheGuidFromSpecifics(specific, /*is_created=*/false);
}

}  // namespace tab_groups
