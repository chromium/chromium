// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_

#include <string>
#include <vector>

#include "base/guid.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

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
      absl::optional<base::GUID> saved_guid = absl::nullopt,
      absl::optional<tab_groups::TabGroupId> tab_group_id = absl::nullopt,
      absl::optional<base::Time> creation_time_windows_epoch_micros =
          absl::nullopt,
      absl::optional<base::Time> update_time_windows_epoch_micros =
          absl::nullopt);
  SavedTabGroup(const SavedTabGroup& other);
  ~SavedTabGroup();

  // Metadata accessors.
  const base::GUID& saved_guid() const { return saved_guid_; }
  const absl::optional<tab_groups::TabGroupId>& tab_group_id() const {
    return tab_group_id_;
  }
  const base::Time& creation_time_windows_epoch_micros() const {
    return creation_time_windows_epoch_micros_;
  }
  const base::Time& update_time_windows_epoch_micros() const {
    return update_time_windows_epoch_micros_;
  }
  const std::u16string& title() const { return title_; }
  const tab_groups::TabGroupColorId& color() const { return color_; }
  const std::vector<SavedTabGroupTab>& saved_tabs() const {
    return saved_tabs_;
  }

  // Metadata mutators.
  SavedTabGroup& SetTitle(std::u16string title) {
    title_ = title;
    return *this;
  };
  SavedTabGroup& SetColor(tab_groups::TabGroupColorId color) {
    color_ = color;
    return *this;
  };
  SavedTabGroup& SetLocalGroupId(
      absl::optional<tab_groups::TabGroupId> tab_group_id) {
    tab_group_id_ = tab_group_id;
    return *this;
  }

  // Converts a `SavedTabGroupSpecifics` retrieved from sync into a
  // `SavedTabGroupTab`.
  static SavedTabGroup FromSpecifics(
      const sync_pb::SavedTabGroupSpecifics& specific);

  // Converts a `SavedTabGroupTab` into a `SavedTabGroupSpecifics` for sync.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> ToSpecifics();

  // Converts tab group color ids into the sync data type for saved tab group
  // colors.
  static ::sync_pb::SavedTabGroup_SavedTabGroupColor TabGroupColorToSyncColor(
      const tab_groups::TabGroupColorId color);

  // Converts sync group colors into tab group colors ids.
  static tab_groups::TabGroupColorId SyncColorToTabGroupColor(
      const sync_pb::SavedTabGroup::SavedTabGroupColor color);

 private:
  // The ID used to represent the group in sync.
  base::GUID saved_guid_;

  // The ID of the tab group in the tab strip which is associated with the saved
  // tab group object. This can be null if the saved tab group is not in any tab
  // strip.
  absl::optional<tab_groups::TabGroupId> tab_group_id_;

  // The title of the saved tab group.
  std::u16string title_;

  // The color of the saved tab group.
  tab_groups::TabGroupColorId color_;

  // The URLS and later webcontents (such as favicons) of the saved tab group.
  std::vector<SavedTabGroupTab> saved_tabs_;

  // Timestamp for when the tab was created using windows epoch microseconds.
  base::Time creation_time_windows_epoch_micros_;

  // Timestamp for when the tab was last updated using windows epoch
  // microseconds.
  base::Time update_time_windows_epoch_micros_;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
