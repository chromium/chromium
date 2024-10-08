// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace tab_groups {

// Preserves the state of a Tab group that was saved from the
// tab_group_editor_bubble_view's save toggle button. Additionally, these values
// may change if the tab groups name, color, or urls are changed from the
// tab_group_editor_bubble_view.
class SavedTabGroup {
 public:
  SavedTabGroup(
      const std::u16string& title,
      const tab_groups::TabGroupColorId& color,
      const std::vector<SavedTabGroupTab>& urls,
      std::optional<size_t> position = std::nullopt,
      std::optional<base::Uuid> saved_guid = std::nullopt,
      std::optional<LocalTabGroupID> local_group_id = std::nullopt,
      std::optional<std::string> creator_cache_guid = std::nullopt,
      std::optional<std::string> last_updater_cache_guid = std::nullopt,
      bool created_before_syncing_tab_groups = false,
      std::optional<base::Time> creation_time_windows_epoch_micros =
          std::nullopt,
      std::optional<base::Time> update_time_windows_epoch_micros =
          std::nullopt);
  SavedTabGroup(const SavedTabGroup& other);
  SavedTabGroup& operator=(const SavedTabGroup& other);
  SavedTabGroup(SavedTabGroup&& other);
  SavedTabGroup& operator=(SavedTabGroup&& other);
  ~SavedTabGroup();

  // Metadata accessors.
  const base::Uuid& saved_guid() const { return saved_guid_; }
  const std::optional<LocalTabGroupID>& local_group_id() const {
    return local_group_id_;
  }
  const base::Time& creation_time_windows_epoch_micros() const {
    return creation_time_windows_epoch_micros_;
  }
  const base::Time& update_time_windows_epoch_micros() const {
    return update_time_windows_epoch_micros_;
  }
  const base::Time& last_user_interaction_time() const {
    return last_user_interaction_time_;
  }
  const std::optional<std::string>& creator_cache_guid() const {
    return creator_cache_guid_;
  }
  const std::optional<std::string>& last_updater_cache_guid() const {
    return last_updater_cache_guid_;
  }
  bool created_before_syncing_tab_groups() const {
    return created_before_syncing_tab_groups_;
  }

  const std::u16string& title() const { return title_; }
  const tab_groups::TabGroupColorId& color() const { return color_; }
  const std::vector<SavedTabGroupTab>& saved_tabs() const {
    return saved_tabs_;
  }
  std::optional<size_t> position() const { return position_; }
  const std::optional<std::string>& collaboration_id() const {
    return collaboration_id_;
  }
  std::optional<base::Uuid> originating_saved_tab_group_guid() const {
    return originating_saved_tab_group_guid_;
  }

  bool is_pinned() const { return position_.has_value(); }
  bool is_shared_tab_group() const { return collaboration_id_.has_value(); }

  std::vector<SavedTabGroupTab>& saved_tabs() { return saved_tabs_; }

  // Accessors for Tabs based on id.
  const SavedTabGroupTab* GetTab(const base::Uuid& saved_tab_guid) const;
  const SavedTabGroupTab* GetTab(const LocalTabID& local_tab_id) const;

  // Non const accessors for tabs based on id. this should only be used inside
  // of the model methods.
  SavedTabGroupTab* GetTab(const base::Uuid& saved_tab_guid);
  SavedTabGroupTab* GetTab(const LocalTabID& local_tab_id);

  // Returns the index for `tab_id` in `saved_tabs_` if it exists. Otherwise,
  // returns std::nullopt.
  std::optional<int> GetIndexOfTab(const base::Uuid& saved_tab_guid) const;
  std::optional<int> GetIndexOfTab(const LocalTabID& local_tab_id) const;

  // Returns true if the `tab_id` was found in `saved_tabs_`.
  bool ContainsTab(const base::Uuid& saved_tab_guid) const;
  bool ContainsTab(const LocalTabID& tab_id) const;

  // Metadata mutators.
  SavedTabGroup& SetTitle(std::u16string title);
  SavedTabGroup& SetColor(tab_groups::TabGroupColorId color);
  SavedTabGroup& SetLocalGroupId(std::optional<LocalTabGroupID> tab_group_id);
  SavedTabGroup& SetCreatorCacheGuid(std::optional<std::string> new_cache_guid);
  SavedTabGroup& SetLastUpdaterCacheGuid(std::optional<std::string> cache_guid);
  SavedTabGroup& SetCreatedBeforeSyncingTabGroups(
      bool created_before_syncing_tab_groups);
  SavedTabGroup& SetUpdateTimeWindowsEpochMicros(
      base::Time update_time_windows_epoch_micros);
  SavedTabGroup& SetLastUserInteractionTime(
      base::Time last_user_interaction_time);
  SavedTabGroup& SetPosition(size_t position);
  SavedTabGroup& SetPinned(bool pinned);
  SavedTabGroup& SetCollaborationId(
      std::optional<std::string> collaboration_id);
  SavedTabGroup& SetOriginatingSavedTabGroupGuid(
      std::optional<base::Uuid> originating_saved_tab_group_guid);

  // Tab mutators.
  // Add `tab` into its position in `saved_tabs_` if it is set. Otherwise add it
  // to the end. If the tab already exists, CHECK. If the tab was added locally
  // update the positions of all the tabs in the group. Otherwise, leave the
  // order of the group as is.
  SavedTabGroup& AddTabLocally(SavedTabGroupTab tab);
  SavedTabGroup& AddTabFromSync(SavedTabGroupTab tab);

  // Updates the tab with with `tab_id` tab.guid() with a value of `tab`. If
  // there is no tab, this function will CHECK.
  SavedTabGroup& UpdateTab(SavedTabGroupTab tab);

  // Removes a tab from `saved_tabs_` denoted by `saved_tab_guid` even if that
  // was the last tab in the group: crbug/1371959. If the tab was removed
  // locally update the positions of all tabs in the group. Otherwise, leave the
  // order of the group as is.
  SavedTabGroup& RemoveTabLocally(const base::Uuid& saved_tab_guid);
  SavedTabGroup& RemoveTabFromSync(const base::Uuid& saved_tab_guid);

  // Replaces that tab denoted by `tab_id` with value of `tab` unless the
  // replacement tab already exists. In this case we CHECK.
  SavedTabGroup& ReplaceTabAt(const base::Uuid& saved_tab_guid,
                              SavedTabGroupTab tab);

  // Moves the tab denoted by `tab_id` from its current index to the
  // `new_index`. If the tab was moved locally update the positions of all tabs
  // in the group. Otherwise, leave the order of the tabs as is. We do this
  // because sync does not guarantee all the data will be sent, in the order the
  // changes were made.
  SavedTabGroup& MoveTabLocally(const base::Uuid& saved_tab_guid,
                                size_t new_index);
  SavedTabGroup& MoveTabFromSync(const base::Uuid& saved_tab_guid,
                                 size_t new_index);

  // Merges this groups data with the `remote_group` params. Side effect:
  // updates the values of this group.
  void MergeRemoteGroupMetadata(
      const std::u16string& title,
      TabGroupColorId color,
      std::optional<size_t> position,
      std::optional<std::string> creator_cache_guid,
      std::optional<std::string> last_updater_cache_guid,
      base::Time update_time);

  // Returns whether the remote group has more recent updates.
  bool RemoteGroupHasMoreRecentUpdates(base::Time remote_update_time) const;

  // Returns true iff syncable data fields in `this` and `other` are equivalent.
  bool IsSyncEquivalent(const SavedTabGroup& other) const;

  // Creates a copy of this group and converts it to a shared tab group.
  SavedTabGroup CloneAsSharedTabGroup(std::string collaboration_id) const;

 private:
  // Moves the tab denoted by `saved_tab_guid` to the position `new_index`.
  void MoveTabImpl(const base::Uuid& saved_tab_guid, size_t new_index);

  // Insert `tab` into sorted order based on its position compared to already
  // stored tabs in its group. It should be noted that the list of tabs in each
  // group must already be in sorted order for this function to work as
  // intended. To do this, UpdateTabPositionsImpl() can be called before calling
  // this method.
  void InsertTabImpl(SavedTabGroupTab tab);

  // Updates all tab positions to match the index they are currently stored at
  // in the group at `group_index`. Does not call observers.
  void UpdateTabPositionsImpl();

  // Removes `saved_tab_guid` from this group.
  void RemoveTabImpl(const base::Uuid& saved_tab_guid);

  // The ID used to represent the group in sync.
  base::Uuid saved_guid_;

  // The ID of the tab group in the tab strip which is associated with the saved
  // tab group object. This can be null if the saved tab group is not in any tab
  // strip.
  std::optional<LocalTabGroupID> local_group_id_;

  // The title of the saved tab group.
  std::u16string title_;

  // The color of the saved tab group.
  tab_groups::TabGroupColorId color_;

  // The URLS and later webcontents (such as favicons) of the saved tab group.
  std::vector<SavedTabGroupTab> saved_tabs_;

  // The current position of the group in relation to all other saved groups.
  // A value of nullopt means that the group was not assigned a position and
  // will be assigned one when it is added into the SavedTabGroupModel.
  std::optional<size_t> position_;

  // A guid which refers to the device which created the tab group. If metadata
  // is not being tracked when the saved tab group is being created, this value
  // will be null. The value could also be null if the group was created before
  // M127. Used for metrics purposes only.
  std::optional<std::string> creator_cache_guid_;

  // The cache guid of the device that last modified this tab group. Can be null
  // if the group was just created. Used for metrics purposes only.
  std::optional<std::string> last_updater_cache_guid_;

  // Whether the tab group was created when sync was disabled.
  bool created_before_syncing_tab_groups_;

  // Timestamp for when the tab was created using windows epoch microseconds.
  base::Time creation_time_windows_epoch_micros_;

  // Timestamp for when the tab was last updated using windows epoch
  // microseconds.
  base::Time update_time_windows_epoch_micros_;

  // Timestamp of last explicit user interaction with the group, which currently
  // refers to tab addition, tab removal and tab navigation only. Only for
  // metrics.
  base::Time last_user_interaction_time_;

  // Collaboration ID in case if the group is shared.
  std::optional<std::string> collaboration_id_;

  // The saved guid of the group that this group was created from. Used for
  // shared tab groups only.
  std::optional<base::Uuid> originating_saved_tab_group_guid_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_H_
