// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TAB_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TAB_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// A SavedTabGroupTab stores the url, title, and favicon of a tab.
class SavedTabGroupTab {
 public:
  SavedTabGroupTab(const GURL& url,
                   const std::u16string& title,
                   const base::Uuid& group_guid,
                   absl::optional<size_t> position,
                   absl::optional<base::Uuid> saved_tab_guid = absl::nullopt,
                   absl::optional<base::Token> local_tab_id = absl::nullopt,
                   absl::optional<base::Time>
                       creation_time_windows_epoch_micros = absl::nullopt,
                   absl::optional<base::Time> update_time_windows_epoch_micros =
                       absl::nullopt,
                   absl::optional<gfx::Image> favicon = absl::nullopt);
  SavedTabGroupTab(const SavedTabGroupTab& other);
  ~SavedTabGroupTab();

  // Accessors.
  const base::Uuid& saved_tab_guid() const { return saved_tab_guid_; }
  const base::Uuid& saved_group_guid() const { return saved_group_guid_; }
  const absl::optional<base::Token> local_tab_id() const {
    return local_tab_id_;
  }
  absl::optional<size_t> position() const { return position_; }
  const GURL& url() const { return url_; }
  const std::u16string& title() const { return title_; }
  const absl::optional<gfx::Image>& favicon() const { return favicon_; }
  const base::Time& creation_time_windows_epoch_micros() const {
    return creation_time_windows_epoch_micros_;
  }
  const base::Time& update_time_windows_epoch_micros() const {
    return update_time_windows_epoch_micros_;
  }

  // Mutators.
  SavedTabGroupTab& SetURL(GURL url) {
    url_ = url;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetTitle(std::u16string title) {
    title_ = title;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetFavicon(absl::optional<gfx::Image> favicon) {
    favicon_ = favicon;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetLocalTabID(absl::optional<base::Token> local_tab_id) {
    local_tab_id_ = local_tab_id;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetPosition(size_t position) {
    position_ = position;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetUpdateTimeWindowsEpochMicros(
      base::Time update_time_windows_epoch_micros) {
    update_time_windows_epoch_micros_ = update_time_windows_epoch_micros;
    return *this;
  }

  // Merges this tabs data with a specific from sync and returns the newly
  // merged specific. Side effect: Updates the values in the tab.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeTab(
      const sync_pb::SavedTabGroupSpecifics& sync_specific);

  // We should merge a tab if one of the following is true:
  // 1. The data from `sync_specific` has the most recent (larger) update time.
  // 2. The `sync_specific` has the oldest (smallest) creation time.
  bool ShouldMergeTab(
      const sync_pb::SavedTabGroupSpecifics& sync_specific) const;

  // Converts a `SavedTabGroupSpecifics` retrieved from sync into a
  // `SavedTabGroupTab`.
  static SavedTabGroupTab FromSpecifics(
      const sync_pb::SavedTabGroupSpecifics& specific);

  // Converts this `SavedTabGroupTab` into a `SavedTabGroupSpecifics` for sync.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> ToSpecifics() const;

  // Returns true iff syncable data fields in `this` and `other` are equivalent.
  bool IsSyncEquivalent(const SavedTabGroupTab& other) const;

 private:
  // The ID used to represent the tab in sync.
  base::Uuid saved_tab_guid_;

  // The ID used to represent the tab's group in sync. This must not be null.
  base::Uuid saved_group_guid_;

  // The ID used to represent the tab in reference to the web_contents locally.
  absl::optional<base::Token> local_tab_id_;

  // The current position of the tab in relation to all other tabs in the group.
  // A value of nullopt means that the group was not assigned a position and
  // will be assigned one when it is added into its saved group.
  absl::optional<size_t> position_;

  // The link to navigate with.
  GURL url_;

  // The title of the website this url is associated with.
  std::u16string title_;

  // The favicon of the website this SavedTabGroupTab represents.
  absl::optional<gfx::Image> favicon_;

  // Timestamp for when the tab was created using windows epoch microseconds.
  base::Time creation_time_windows_epoch_micros_;

  // Timestamp for when the tab was last updated using windows epoch
  // microseconds.
  base::Time update_time_windows_epoch_micros_;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TAB_H_
