// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_TAB_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_TAB_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace tab_groups {

// A SavedTabGroupTab stores the url, title, and favicon of a tab.
class SavedTabGroupTab {
 public:
  SavedTabGroupTab(
      const GURL& url,
      const std::u16string& title,
      const base::Uuid& group_guid,
      std::optional<size_t> position,
      std::optional<base::Uuid> saved_tab_guid = std::nullopt,
      std::optional<LocalTabID> local_tab_id = std::nullopt,
      std::optional<std::string> creator_cache_guid = std::nullopt,
      std::optional<std::string> last_updater_cache_guid = std::nullopt,
      std::optional<base::Time> creation_time_windows_epoch_micros =
          std::nullopt,
      std::optional<base::Time> update_time_windows_epoch_micros = std::nullopt,
      std::optional<gfx::Image> favicon = std::nullopt);
  SavedTabGroupTab(const SavedTabGroupTab& other);
  SavedTabGroupTab& operator=(const SavedTabGroupTab& other);
  SavedTabGroupTab(SavedTabGroupTab&& other);
  SavedTabGroupTab& operator=(SavedTabGroupTab&& other);
  ~SavedTabGroupTab();

  // Accessors.
  const base::Uuid& saved_tab_guid() const { return saved_tab_guid_; }
  const base::Uuid& saved_group_guid() const { return saved_group_guid_; }
  const std::optional<LocalTabID> local_tab_id() const { return local_tab_id_; }
  std::optional<size_t> position() const { return position_; }
  const GURL& url() const { return url_; }
  const std::u16string& title() const { return title_; }
  const std::optional<gfx::Image>& favicon() const { return favicon_; }
  const base::Time& creation_time_windows_epoch_micros() const {
    return creation_time_windows_epoch_micros_;
  }
  const base::Time& update_time_windows_epoch_micros() const {
    return update_time_windows_epoch_micros_;
  }
  const std::optional<std::string>& creator_cache_guid() const {
    return creator_cache_guid_;
  }
  const std::optional<std::string>& last_updater_cache_guid() const {
    return last_updater_cache_guid_;
  }
  const std::vector<GURL>& redirect_url_chain() const {
    return redirect_url_chain_;
  }

  // Mutators.
  SavedTabGroupTab& SetURL(GURL url) {
    url_ = std::move(url);
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetTitle(std::u16string title) {
    title_ = std::move(title);
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetFavicon(std::optional<gfx::Image> favicon) {
    favicon_ = std::move(favicon);
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetLocalTabID(std::optional<LocalTabID> local_tab_id) {
    local_tab_id_ = std::move(local_tab_id);
    return *this;
  }
  SavedTabGroupTab& SetPosition(size_t position) {
    position_ = position;
    SetUpdateTimeWindowsEpochMicros(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetCreatorCacheGuid(
      std::optional<std::string> new_cache_guid) {
    creator_cache_guid_ = std::move(new_cache_guid);
    return *this;
  }
  SavedTabGroupTab& SetLastUpdaterCacheGuid(
      std::optional<std::string> cache_guid) {
    last_updater_cache_guid_ = std::move(cache_guid);
    return *this;
  }
  SavedTabGroupTab& SetUpdateTimeWindowsEpochMicros(
      base::Time update_time_windows_epoch_micros) {
    update_time_windows_epoch_micros_ = update_time_windows_epoch_micros;
    return *this;
  }
  SavedTabGroupTab& SetRedirectURLChain(
      const std::vector<GURL>& redirect_url_chain) {
    redirect_url_chain_ = redirect_url_chain;
    return *this;
  }

  // Merges this tabs data with a specific from sync and returns the newly
  // merged specific. Side effect: Updates the values in the tab.
  void MergeRemoteTab(const SavedTabGroupTab& remote_tab);

  // Returns whether the `remote_tab` should be merged into the current one.
  bool ShouldMergeTab(const SavedTabGroupTab& remote_tab) const;

  // Returns true iff syncable data fields in `this` and `other` are equivalent.
  bool IsSyncEquivalent(const SavedTabGroupTab& other) const;

  // Check whether a URL is in the `redirect_url_chain_`.
  bool IsURLInRedirectChain(const GURL& url) const;

 private:
  // The ID used to represent the tab in sync.
  base::Uuid saved_tab_guid_;

  // The ID used to represent the tab's group in sync. This must not be null.
  base::Uuid saved_group_guid_;

  // The ID used to represent the tab in reference to the web_contents locally.
  std::optional<LocalTabID> local_tab_id_;

  // The current position of the tab in relation to all other tabs in the group.
  // A value of nullopt means that the group was not assigned a position and
  // will be assigned one when it is added into its saved group.
  std::optional<size_t> position_;

  // The link to navigate with.
  GURL url_;

  // The title of the website this url is associated with.
  std::u16string title_;

  // A guid which refers to the device which created the tab group. If metadata
  // is not being tracked when the saved tab group is being created, this value
  // will be null. The value could also be null if the group was created before
  // M127. Used for metrics purposes only.
  std::optional<std::string> creator_cache_guid_;

  // The cache guid of the device that last modified this tab group. Can be null
  // if the group was just created. Used for metrics purposes only.
  std::optional<std::string> last_updater_cache_guid_;

  // Timestamp for when the tab was created using windows epoch microseconds.
  base::Time creation_time_windows_epoch_micros_;

  // Timestamp for when the tab was last updated using windows epoch
  // microseconds.
  base::Time update_time_windows_epoch_micros_;

  // The following fields aren't synced across devices.

  // The favicon of the website this SavedTabGroupTab represents.
  std::optional<gfx::Image> favicon_;

  // Holds the current redirect chain which is used for equality check for any
  // incoming URL update. If any of the URLs in the chain matches with the new
  // URL, we don't do a navigation.
  std::vector<GURL> redirect_url_chain_;
};

class SavedTabGroupTabBuilder {
 public:
  SavedTabGroupTabBuilder();
  ~SavedTabGroupTabBuilder();

  // Disallow copy/assign.
  SavedTabGroupTabBuilder(const SavedTabGroupTabBuilder&) = delete;
  SavedTabGroupTabBuilder& operator=(const SavedTabGroupTabBuilder&) = delete;

  SavedTabGroupTabBuilder& SetURL(const GURL& url);
  SavedTabGroupTabBuilder& SetTitle(const std::u16string& title);
  SavedTabGroupTabBuilder& SetPosition(size_t position);
  SavedTabGroupTabBuilder& SetRedirectURLChain(
      const std::vector<GURL>& redirect_url_chain);

  SavedTabGroupTab Build(const SavedTabGroupTab& tab) const;

  // Accessors for testing.
  GURL url() const { return url_; }
  std::u16string title() const { return title_; }
  size_t position() const { return position_; }

 private:
  GURL url_;
  std::u16string title_;
  size_t position_ = 0;

  // Flags to indicate which properties have been set.
  bool has_url_ = false;
  bool has_title_ = false;
  bool has_position_ = false;
  std::vector<GURL> redirect_url_chain_;
  bool has_redirect_url_chain_ = false;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_TAB_H_
