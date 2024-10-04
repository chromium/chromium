// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SYNC_BRIDGE_TAB_GROUP_MODEL_WRAPPER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SYNC_BRIDGE_TAB_GROUP_MODEL_WRAPPER_H_

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/tab_groups/tab_group_color.h"

namespace tab_groups {

class SavedTabGroup;
class SavedTabGroupTab;
class SavedTabGroupModel;

// Wrapper of SavedTabGroupModel for sync bridges. Used to enforce the bridges
// working with the expected types of tab groups to mitigate possible unexpected
// changes between the data types.
class SyncBridgeTabGroupModelWrapper {
 public:
  using LoadCallback = base::OnceCallback<void(std::vector<SavedTabGroup>,
                                               std::vector<SavedTabGroupTab>)>;

  // `model` must not be null and must outlive the current object.
  SyncBridgeTabGroupModelWrapper(syncer::DataType sync_data_type,
                                 SavedTabGroupModel* model,
                                 LoadCallback on_load_callback);
  SyncBridgeTabGroupModelWrapper(const SyncBridgeTabGroupModelWrapper&) =
      delete;
  SyncBridgeTabGroupModelWrapper& operator=(
      const SyncBridgeTabGroupModelWrapper&) = delete;
  ~SyncBridgeTabGroupModelWrapper();

  // Returns true if the model is initialized.
  bool IsInitialized() const;

  // Returns tab groups corresponding to the data type.
  std::vector<const SavedTabGroup*> GetTabGroups() const;

  // Returns a group with the given `group_id`, null if the group does not exist
  // or does not correspond to the data type.
  const SavedTabGroup* GetGroup(const base::Uuid& group_id) const;

  // Returns the group which contains the tab with `tab_id` if exists and
  // corresponds to the data type, otherwise returns null.
  const SavedTabGroup* GetGroupContainingTab(const base::Uuid& tab_id) const;

  // Removes the tab from the `group_id`.
  void RemoveTabFromGroup(const base::Uuid& group_id, const base::Uuid& tab_id);

  // Removes the whole group and all its tabs.
  void RemoveGroup(const base::Uuid& group_id);

  // Updates group's metadata based on the remote update.
  const SavedTabGroup* MergeRemoteGroupMetadata(
      const base::Uuid& group_id,
      const std::u16string& title,
      TabGroupColorId color,
      std::optional<size_t> position,
      std::optional<std::string> creator_cache_guid,
      std::optional<std::string> last_updater_cache_guid,
      base::Time update_time);

  // Updates the tab with the given remote data.
  const SavedTabGroupTab* MergeRemoteTab(const SavedTabGroupTab& remote_tab);

  // Adds a group created remotely.
  void AddGroup(SavedTabGroup group);

  // Adds a new tab to the group created remotely.
  void AddTabToGroup(const base::Uuid& group_id, SavedTabGroupTab tab);

  // Updates the creator cache guid for all saved groups that have
  // `old_cache_guid`, to `new_cache_guid`. Returns a pair of group IDs and tab
  // IDs which were updated.
  std::pair<std::set<base::Uuid>, std::set<base::Uuid>> UpdateLocalCacheGuid(
      std::optional<std::string> old_cache_guid,
      std::optional<std::string> new_cache_guid);

  // Initializes with the data loaded from the disk.
  void Initialize(std::vector<SavedTabGroup> groups,
                  std::vector<SavedTabGroupTab> tabs);

 private:
  // Returns whether the current wrapper is used for the shared tab group data.
  bool IsSharedTabGroupData() const;

  const syncer::DataType sync_data_type_;
  const raw_ptr<SavedTabGroupModel> model_;

  LoadCallback on_load_callback_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SYNC_BRIDGE_TAB_GROUP_MODEL_WRAPPER_H_
