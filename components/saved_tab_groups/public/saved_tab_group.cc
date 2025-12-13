// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/saved_tab_group.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace tab_groups {

namespace {

// The maximum number of the last removed tabs to keep metadata. This is used to
// prevent keeping all the removed tabs when the group is huge.
constexpr size_t kMaxLastRemovedTabsMetadata = 100;

bool ShouldPlaceNewTabBeforeExistingTab(const SavedTabGroupTab& new_tab,
                                        const SavedTabGroupTab& existing_tab) {
  if (new_tab.position() < existing_tab.position()) {
    return true;
  }

  if (existing_tab.position() == new_tab.position() &&
      existing_tab.update_time() < new_tab.update_time()) {
    // Use the update time for a consistent ordering across devices.
    return true;
  }

  return false;
}

}  // namespace

SavedTabGroup::SavedTabGroup(const std::u16string& title,
                             const tab_groups::TabGroupColorId& color,
                             const std::vector<SavedTabGroupTab>& urls,
                             std::optional<size_t> position,
                             std::optional<base::Uuid> saved_guid,
                             std::optional<LocalTabGroupID> local_group_id,
                             std::optional<std::string> creator_cache_guid,
                             std::optional<std::string> last_updater_cache_guid,
                             bool created_before_syncing_tab_groups,
                             std::optional<base::Time> creation_time,
                             std::optional<base::Time> update_time)
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
      creation_time_(creation_time.value_or(base::Time::Now())),
      update_time_(update_time.value_or(base::Time::Now())) {}

SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;
SavedTabGroup& SavedTabGroup::operator=(const SavedTabGroup& other) = default;

SavedTabGroup::SavedTabGroup(SavedTabGroup&& other) = default;
SavedTabGroup& SavedTabGroup::operator=(SavedTabGroup&& other) = default;

SavedTabGroup::~SavedTabGroup() = default;

SavedTabGroup::RemovedTabMetadata::RemovedTabMetadata() = default;
SavedTabGroup::RemovedTabMetadata::~RemovedTabMetadata() = default;

std::optional<base::Uuid> SavedTabGroup::GetOriginatingTabGroupGuid(
    bool for_sync) const {
  if (use_originating_tab_group_guid_ || for_sync) {
    return originating_tab_group_guid_;
  }

  // The current user must always be an owner of saved tab groups.
  CHECK(is_shared_tab_group() || !originating_tab_group_guid_.has_value());
  return std::nullopt;
}

const SavedTabGroupTab* SavedTabGroup::GetTab(
    const base::Uuid& saved_tab_guid) const {
  std::optional<int> index = GetIndexOfTab(saved_tab_guid);
  if (!index.has_value()) {
    return nullptr;
  }
  return &saved_tabs()[index.value()];
}

const SavedTabGroupTab* SavedTabGroup::GetTab(
    const LocalTabID& local_tab_id) const {
  std::optional<int> index = GetIndexOfTab(local_tab_id);
  if (!index.has_value()) {
    return nullptr;
  }
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
  auto it = std::ranges::find_if(
      saved_tabs(), [saved_tab_guid](const SavedTabGroupTab& tab) {
        return tab.saved_tab_guid() == saved_tab_guid;
      });
  if (it != saved_tabs().end()) {
    return it - saved_tabs().begin();
  }
  return std::nullopt;
}

std::optional<int> SavedTabGroup::GetIndexOfTab(
    const LocalTabID& local_tab_id) const {
  auto it = std::ranges::find_if(saved_tabs(),
                                 [local_tab_id](const SavedTabGroupTab& tab) {
                                   return tab.local_tab_id() == local_tab_id;
                                 });
  if (it != saved_tabs().end()) {
    return it - saved_tabs().begin();
  }
  return std::nullopt;
}

SavedTabGroup& SavedTabGroup::SetTitle(std::u16string title) {
  title_ = title;
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetColor(tab_groups::TabGroupColorId color) {
  color_ = color;
  SetUpdateTime(base::Time::Now());
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

SavedTabGroup& SavedTabGroup::SetUpdateTime(base::Time update_time) {
  update_time_ = update_time;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetLastUserInteractionTime(
    base::Time last_user_interaction_time) {
  last_user_interaction_time_ = last_user_interaction_time;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetPosition(size_t position) {
  position_ = position;
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetPinned(bool pinned) {
  if (pinned && position_ != 0) {
    position_ = 0;
    SetUpdateTime(base::Time::Now());
  } else if (!pinned && position_ != std::nullopt) {
    position_ = std::nullopt;
    SetUpdateTime(base::Time::Now());
  }
  return *this;
}

SavedTabGroup& SavedTabGroup::SetBookmarkNodeId(
    std::optional<base::Uuid> bookmark_node_id) {
  bookmark_node_id_ = bookmark_node_id;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetCollaborationId(
    std::optional<syncer::CollaborationId> collaboration_id) {
  collaboration_id_ = std::move(collaboration_id);
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::SetSharedGroupStatus(
    SharedGroupStatus shared_group_status) {
  shared_group_status_ = shared_group_status;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetOriginatingTabGroupGuid(
    std::optional<base::Uuid> originating_tab_group_guid,
    bool use_originating_tab_group_guid) {
  originating_tab_group_guid_ = std::move(originating_tab_group_guid);
  use_originating_tab_group_guid_ = use_originating_tab_group_guid;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetIsTransitioningToSaved(
    bool is_transitioning_to_saved) {
  DCHECK(is_shared_tab_group() || !is_transitioning_to_saved);
  is_transitioning_to_saved_ = is_transitioning_to_saved;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetUpdatedByAttribution(GaiaId updated_by) {
  if (shared_attribution_.created_by.empty()) {
    shared_attribution_.created_by = updated_by;
  }
  shared_attribution_.updated_by = std::move(updated_by);
  return *this;
}

SavedTabGroup& SavedTabGroup::SetCreatedByAttribution(GaiaId created_by) {
  CHECK(shared_attribution_.created_by.empty());
  shared_attribution_.created_by = std::move(created_by);
  return *this;
}

SavedTabGroup& SavedTabGroup::SetIsHidden(bool is_hidden) {
  is_hidden_ = is_hidden;
  return *this;
}

SavedTabGroup& SavedTabGroup::SetArchivalTime(
    std::optional<base::Time> archival_time) {
  archival_time_ = archival_time;
  return *this;
}

SavedTabGroup& SavedTabGroup::AddTabLocally(SavedTabGroupTab tab) {
  InsertTabImpl(tab);
  UpdateTabPositionsImpl();
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::AddTabFromSync(SavedTabGroupTab tab) {
  InsertTabImpl(tab);
  if (is_shared_tab_group()) {
    // Shared tabs use unique positions for syncing, hence generate a local
    // numbered position on remote update.
    UpdateTabPositionsImpl();
  } else {
    // TODO(crbug.com/369768775): consider removing the following line for saved
    // tab groups because update time is used from sync.
    SetUpdateTime(base::Time::Now());
  }
  return *this;
}

SavedTabGroup& SavedTabGroup::RemoveTabLocally(
    const base::Uuid& saved_tab_guid,
    std::optional<GaiaId> local_gaia_id) {
  if (local_gaia_id.has_value()) {
    UpdateLastRemovedTabMetadata(saved_tab_guid, local_gaia_id.value());
  }
  RemoveTabImpl(saved_tab_guid);
  UpdateTabPositionsImpl();
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::RemoveTabFromSync(
    const base::Uuid& saved_tab_guid,
    GaiaId removed_by,
    bool ignore_empty_groups_for_testing) {
  CHECK(removed_by.empty() || is_shared_tab_group());
  UpdateLastRemovedTabMetadata(saved_tab_guid, removed_by);
  RemoveTabImpl(saved_tab_guid, /*allow_empty_groups=*/true);
  SetUpdateTime(base::Time::Now());
  return *this;
}

SavedTabGroup& SavedTabGroup::UpdateTab(SavedTabGroupTab tab) {
  std::optional<size_t> index = GetIndexOfTab(tab.saved_tab_guid());
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0u);
  CHECK_LT(index.value(), saved_tabs_.size());
  saved_tabs_.erase(saved_tabs_.begin() + index.value());
  saved_tabs_.insert(saved_tabs_.begin() + index.value(), std::move(tab));
  SetUpdateTime(base::Time::Now());
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
  SetUpdateTime(base::Time::Now());
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

  if (is_shared_tab_group()) {
    // Shared tab groups use unique positions for syncing tabs. The callers are
    // expected to provide the valid index as a position to insert.
    size_t position_to_insert =
        std::min(saved_tabs_.size(), tab.position().value());
    saved_tabs_.insert(saved_tabs_.begin() + position_to_insert,
                       std::move(tab));
    return;
  }

  for (size_t index = 0; index < saved_tabs_.size(); ++index) {
    if (ShouldPlaceNewTabBeforeExistingTab(tab, saved_tabs_[index])) {
      saved_tabs_.insert(saved_tabs_.begin() + index, std::move(tab));
      return;
    }
  }

  // This can happen when the last element of the vector has the same position
  // as `group` and was more recently updated, or `tab` is the last element.
  saved_tabs_.push_back(std::move(tab));
}

void SavedTabGroup::UpdateTabPositionsImpl() {
  for (size_t i = 0; i < saved_tabs_.size(); ++i) {
    saved_tabs_[i].SetPosition(i);
  }

  SetUpdateTime(base::Time::Now());
}

void SavedTabGroup::MergeRemoteGroupMetadata(
    const std::u16string& title,
    TabGroupColorId color,
    std::optional<size_t> position,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    base::Time update_time) {
  SetTitle(title);
  SetColor(color);

  // Do not merge position for shared tab group since the position is saved from
  // elsewhere.
  if (!is_shared_tab_group()) {
    if (position.has_value()) {
      SetPosition(position.value());
    } else {
      SetPinned(false);
    }
  }

  SetCreatorCacheGuid(creator_cache_guid);
  SetLastUpdaterCacheGuid(last_updater_cache_guid);

  SetUpdateTime(update_time);
}

bool SavedTabGroup::IsSyncEquivalent(const SavedTabGroup& other) const {
  return saved_guid() == other.saved_guid() && color() == other.color() &&
         title() == other.title() && position() == other.position();
}

SavedTabGroup SavedTabGroup::CloneAsSharedTabGroup(
    syncer::CollaborationId collaboration_id) const {
  SavedTabGroup shared_group = CopyBaseFieldsWithTabs();
  shared_group.is_transitioning_to_shared_ = true;
  shared_group.SetCollaborationId(std::move(collaboration_id));
  shared_group.SetOriginatingTabGroupGuid(
      saved_guid(), /*use_originating_tab_group_guid=*/true);
  return shared_group;
}

SavedTabGroup SavedTabGroup::CloneAsSavedTabGroup() const {
  DCHECK(is_shared_tab_group());
  SavedTabGroup saved_group = CopyBaseFieldsWithTabs();
  saved_group.SetOriginatingTabGroupGuid(
      saved_guid(), /*use_originating_tab_group_guid=*/true);
  return saved_group;
}

// static
size_t SavedTabGroup::GetMaxLastRemovedTabsMetadataForTesting() {
  return kMaxLastRemovedTabsMetadata;
}

void SavedTabGroup::MarkTransitionedToShared() {
  is_transitioning_to_shared_ = false;
}

void SavedTabGroup::MarkTransitioningToSharedForTesting() {
  is_transitioning_to_shared_ = true;
}

void SavedTabGroup::RemoveTabImpl(const base::Uuid& saved_tab_guid,
                                  bool allow_empty_groups) {
  std::optional<size_t> index = GetIndexOfTab(saved_tab_guid);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0u);
  CHECK_LT(index.value(), saved_tabs_.size());
  saved_tabs_.erase(saved_tabs_.begin() + index.value());

  base::UmaHistogramBoolean(
      "TabGroups.SavedTabGroups.TabRemovedFromGroupWasLastTab",
      saved_tabs_.empty());
  CHECK(allow_empty_groups || !saved_tabs_.empty());
}

SavedTabGroup SavedTabGroup::CopyBaseFieldsWithTabs() const {
  SavedTabGroup cloned_group(title(), color(), /*urls=*/{}, position_);

  for (size_t i = 0; i < saved_tabs().size(); ++i) {
    const SavedTabGroupTab& tab = saved_tabs()[i];

    // Use tab's index as position for the copied tab as tabs are
    // displayed in the same order.
    SavedTabGroupTab cloned_tab(tab.url(), tab.title(),
                                cloned_group.saved_guid(), /*position=*/i);
    cloned_tab.SetFavicon(tab.favicon());
    cloned_group.AddTabLocally(std::move(cloned_tab));
  }
  return cloned_group;
}

void SavedTabGroup::UpdateLastRemovedTabMetadata(
    const base::Uuid& saved_tab_guid,
    GaiaId removed_by) {
  if (removed_by.empty()) {
    return;
  }
  last_removed_tabs_metadata_[saved_tab_guid].removed_by =
      std::move(removed_by);
  last_removed_tabs_metadata_[saved_tab_guid].removal_time = base::Time::Now();

  // Clean up old removed tabs metadata.
  if (last_removed_tabs_metadata_.size() > kMaxLastRemovedTabsMetadata) {
    // Erase only one minimal element because it should be the case in
    // practice.
    last_removed_tabs_metadata_.erase(std::ranges::min_element(
        last_removed_tabs_metadata_, std::ranges::less(),
        [](const auto& guid_and_metadata) {
          return guid_and_metadata.second.removal_time;
        }));
  }
}

}  // namespace tab_groups
