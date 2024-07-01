// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"

#include <optional>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace tab_groups {

SavedTabGroup::SavedTabGroup(
    const std::u16string& title,
    const tab_groups::TabGroupColorId& color,
    const std::vector<SavedTabGroupTab>& urls,
    std::optional<size_t> position,
    std::optional<base::Uuid> saved_guid,
    std::optional<LocalTabGroupID> local_group_id,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    bool created_before_syncing_tab_groups,
    std::optional<base::Time> creation_time_windows_epoch_micros,
    std::optional<base::Time> update_time_windows_epoch_micros)
    : saved_guid_(
          std::move(saved_guid).value_or(base::Uuid::GenerateRandomV4())),
      local_group_id_(local_group_id),
      title_(title),
      color_(color),
      saved_tabs_(urls),
      position_(position),
      creator_cache_guid_(std::move(creator_cache_guid)),
      last_updater_cache_guid_(std::move(last_updater_cache_guid)),
      created_before_syncing_tab_groups_(created_before_syncing_tab_groups),
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.value_or(base::Time::Now())),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.value_or(base::Time::Now())) {}

SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;
SavedTabGroup& SavedTabGroup::operator=(const SavedTabGroup& other) = default;

SavedTabGroup::SavedTabGroup(SavedTabGroup&& other) = default;
SavedTabGroup& SavedTabGroup::operator=(SavedTabGroup&& other) = default;

SavedTabGroup::~SavedTabGroup() = default;

const SavedTabGroupTab* SavedTabGroup::GetTab(
    const base::Uuid& saved_tab_guid) const {
  std::optional<int> index = GetIndexOfTab(saved_tab_guid);
  if (!index.has_value())
    return nullptr;
  return &saved_tabs()[index.value()];
}

const SavedTabGroupTab* SavedTabGroup::GetTab(
    const LocalTabID& local_tab_id) const {
  std::optional<int> index = GetIndexOfTab(local_tab_id);
  if (!index.has_value())
    return nullptr;
  return &saved_tabs()[index.value()];
}

SavedTabGroupTab* SavedTabGroup::GetTab(const base::Uuid& saved_tab_guid) {
  std::optional<int> index = GetIndexOfTab(saved_tab_guid);
  if (!index.has_value()) {
    return nullptr;
  }
  return &saved_tabs()[index.value()];
}

SavedTabGroupTab* SavedTabGroup::GetTab(const LocalTabID& local_tab_id) {
  std::optional<int> index = GetIndexOfTab(local_tab_id);
  if (!index.has_value()) {
    return nullptr;
  }
  return &saved_tabs()[index.value()];
}

bool SavedTabGroup::ContainsTab(const base::Uuid& saved_tab_guid) const {
  std::optional<int> index = GetIndexOfTab(saved_tab_guid);
  return index.has_value();
}

bool SavedTabGroup::ContainsTab(const LocalTabID& local_tab_id) const {
  std::optional<int> index = GetIndexOfTab(local_tab_id);
  return index.has_value();
}

std::optional<int> SavedTabGroup::GetIndexOfTab(
    const base::Uuid& saved_tab_guid) const {
  auto it = base::ranges::find_if(
      saved_tabs(), [saved_tab_guid](const SavedTabGroupTab& tab) {
        return tab.saved_tab_guid() == saved_tab_guid;
      });
  if (it != saved_tabs().end())
    return it - saved_tabs().begin();
  return std::nullopt;
}

std::optional<int> SavedTabGroup::GetIndexOfTab(
    const LocalTabID& local_tab_id) const {
  auto it = base::ranges::find_if(saved_tabs(),
                                  [local_tab_id](const SavedTabGroupTab& tab) {
                                    return tab.local_tab_id() == local_tab_id;
                                  });
  if (it != saved_tabs().end())
    return it - saved_tabs().begin();
  return std::nullopt;
}

SavedTabGroup& SavedTabGroup::SetTitle(std::u16string title) {
  title_ = title;
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetColor(tab_groups::TabGroupColorId color) {
  color_ = color;
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetLocalGroupId(
    std::optional<LocalTabGroupID> tab_group_id) {
  local_group_id_ = tab_group_id;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetCreatorCacheGuid(
    std::optional<std::string> new_cache_guid) {
  creator_cache_guid_ = new_cache_guid;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetLastUpdaterCacheGuid(
    std::optional<std::string> cache_guid) {
  last_updater_cache_guid_ = cache_guid;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetCreatedBeforeSyncingTabGroups(
    bool created_before_syncing_tab_groups) {
  created_before_syncing_tab_groups_ = created_before_syncing_tab_groups;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetUpdateTimeWindowsEpochMicros(
    base::Time update_time_windows_epoch_micros) {
  update_time_windows_epoch_micros_ = update_time_windows_epoch_micros;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetLastUserInteractionTime(
    base::Time last_user_interaction_time) {
  last_user_interaction_time_ = last_user_interaction_time;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetPosition(size_t position) {
  position_ = position;
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetPinned(bool pinned) {
  if (pinned && position_ != 0) {
    position_ = 0;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  } else if (!pinned && position_ != std::nullopt) {
    position_ = std::nullopt;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  }
  return *this;
}

SavedTabGroup& SavedTabGroup::AddTabLocally(SavedTabGroupTab tab) {
  InsertTabImpl(tab);
  UpdateTabPositionsImpl();
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::AddTabFromSync(SavedTabGroupTab tab) {
  InsertTabImpl(tab);
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::RemoveTabLocally(
    const base::Uuid& saved_tab_guid) {
  RemoveTabImpl(saved_tab_guid);
  UpdateTabPositionsImpl();
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::RemoveTabFromSync(
    const base::Uuid& saved_tab_guid) {
  RemoveTabImpl(saved_tab_guid);
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::UpdateTab(SavedTabGroupTab tab) {
  std::optional<size_t> index = GetIndexOfTab(tab.saved_tab_guid());
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0u);
  CHECK_LT(index.value(), saved_tabs_.size());
  saved_tabs_.erase(saved_tabs_.begin() + index.value());
  saved_tabs_.insert(saved_tabs_.begin() + index.value(), std::move(tab));
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::ReplaceTabAt(const base::Uuid& tab_id,
                                           SavedTabGroupTab tab) {
  std::optional<size_t> index = GetIndexOfTab(tab_id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0u);
  CHECK_LT(index.value(), saved_tabs_.size());
  saved_tabs_.erase(saved_tabs_.begin() + index.value());
  saved_tabs_.insert(saved_tabs_.begin() + index.value(), std::move(tab));
  UpdateTabPositionsImpl();
  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::MoveTabLocally(const base::Uuid& saved_tab_guid,
                                             size_t new_index) {
  MoveTabImpl(saved_tab_guid, new_index);
  UpdateTabPositionsImpl();
  return *this;
}

SavedTabGroup& SavedTabGroup::MoveTabFromSync(const base::Uuid& saved_tab_guid,
                                              size_t new_index) {
  MoveTabImpl(saved_tab_guid, new_index);
  return *this;
}

void SavedTabGroup::MoveTabImpl(const base::Uuid& saved_tab_guid,
                                size_t new_index) {
  std::optional<size_t> curr_index = GetIndexOfTab(saved_tab_guid);
  CHECK(curr_index.has_value());
  CHECK_GE(curr_index.value(), 0u);
  CHECK_LT(curr_index.value(), saved_tabs_.size());
  CHECK_GE(new_index, 0u);
  CHECK_LT(new_index, saved_tabs_.size());

  if (curr_index > new_index) {
    std::rotate(saved_tabs_.begin() + new_index,
                saved_tabs_.begin() + curr_index.value(),
                saved_tabs_.begin() + curr_index.value() + 1);
  } else if (curr_index < new_index) {
    std::rotate(
        saved_tabs_.rbegin() + ((saved_tabs_.size() - 1) - new_index),
        saved_tabs_.rbegin() + ((saved_tabs_.size() - 1) - curr_index.value()),
        saved_tabs_.rbegin() + ((saved_tabs_.size() - 1) - curr_index.value()) +
            1);
  }
}

void SavedTabGroup::InsertTabImpl(SavedTabGroupTab tab) {
  CHECK(!ContainsTab(tab.saved_tab_guid()));

  if (!tab.position().has_value()) {
    tab.SetPosition(saved_tabs_.size());
  }

  // We can always safely insert the first tab at the end. We can also safely
  // insert `tab` if its position is larger than the position at the end of
  // `saved_tabs_`.
  if (saved_tabs_.empty() ||
      saved_tabs_[saved_tabs_.size() - 1].position() < tab.position()) {
    saved_tabs_.emplace_back(std::move(tab));
    return;
  }

  // Insert `tab` in front of an element if one of these criteria
  // are met:
  // 1. The current index is larger than `tab`.
  // 2. The current index has the same position as `tab` and is not
  // the most recently updated position.
  for (size_t index = 0; index < saved_tabs_.size(); ++index) {
    const SavedTabGroupTab& curr_tab = saved_tabs_[index];
    bool curr_position_larger = curr_tab.position() > tab.position();
    bool curr_position_same = curr_tab.position() == tab.position();
    bool curr_position_least_recently_updated =
        curr_tab.update_time_windows_epoch_micros() <
        tab.update_time_windows_epoch_micros();

    if (curr_position_larger ||
        (curr_position_same && curr_position_least_recently_updated)) {
      saved_tabs_.insert(saved_tabs_.begin() + index, std::move(tab));
      return;
    }
  }

  // This can happen when the last element of the vector has the same position
  // as `group` and was more recently updated.
  saved_tabs_.push_back(std::move(tab));
}

void SavedTabGroup::UpdateTabPositionsImpl() {
  for (size_t i = 0; i < saved_tabs_.size(); ++i) {
    saved_tabs_[i].SetPosition(i);
  }

  SetUpdateTimeWindowsEpochMicros(base::Time::Now());
}

bool SavedTabGroup::RemoteGroupHasMoreRecentUpdates(
    base::Time remote_group_update_time) const {
  if (AlwaysAcceptServerDataInModel()) {
    return true;
  }

  // TODO(crbug.com/40870787): Investigate if we should consider the creation
  // time.
  return remote_group_update_time >= update_time_windows_epoch_micros();
}

void SavedTabGroup::MergeRemoteGroupMetadata(
    const std::u16string& title,
    TabGroupColorId color,
    std::optional<size_t> position,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    base::Time update_time) {
  if (!RemoteGroupHasMoreRecentUpdates(update_time)) {
    return;
  }

  SetTitle(title);
  SetColor(color);
  if (position.has_value()) {
    SetPosition(position.value());
  } else if (IsTabGroupsSaveUIUpdateEnabled()) {
    SetPinned(false);
  }

  SetCreatorCacheGuid(creator_cache_guid);
  SetLastUpdaterCacheGuid(last_updater_cache_guid);

  SetUpdateTimeWindowsEpochMicros(update_time);
}

bool SavedTabGroup::IsSyncEquivalent(const SavedTabGroup& other) const {
  return saved_guid() == other.saved_guid() && color() == other.color() &&
         title() == other.title() && position() == other.position();
}

void SavedTabGroup::RemoveTabImpl(const base::Uuid& saved_tab_guid) {
  std::optional<size_t> index = GetIndexOfTab(saved_tab_guid);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0u);
  CHECK_LT(index.value(), saved_tabs_.size());
  saved_tabs_.erase(saved_tabs_.begin() + index.value());
}

}  // namespace tab_groups
