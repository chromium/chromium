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
#include "google_apis/gaia/gaia_id.h"
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
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Time> update_time = std::nullopt,
      std::optional<gfx::Image> favicon = std::nullopt,
      bool is_pending_ntp = false);
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
  const base::Time& creation_time() const { return creation_time_; }
  const base::Time& update_time() const { return update_time_; }
  const base::Time& navigation_time() const { return navigation_time_; }
  const std::optional<base::Time>& last_seen_time() const {
    return last_seen_time_;
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
  const SharedAttribution& shared_attribution() const {
    return shared_attribution_;
  }
  bool is_pending_ntp() const { return is_pending_ntp_; }

  // Mutators.
  SavedTabGroupTab& SetURL(GURL url) {
    url_ = std::move(url);
    SetUpdateTime(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetTitle(std::u16string title) {
    title_ = std::move(title);
    SetUpdateTime(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetFavicon(std::optional<gfx::Image> favicon) {
    favicon_ = std::move(favicon);
    SetUpdateTime(base::Time::Now());
    return *this;
  }
  SavedTabGroupTab& SetLocalTabID(std::optional<LocalTabID> local_tab_id) {
    local_tab_id_ = std::move(local_tab_id);
    return *this;
  }
  SavedTabGroupTab& SetPosition(size_t position) {
    position_ = position;
    SetUpdateTime(base::Time::Now());
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
  SavedTabGroupTab& SetUpdateTime(base::Time update_time) {
    update_time_ = update_time;
    return *this;
  }
  SavedTabGroupTab& SetNavigationTime(base::Time navigation_time) {
    navigation_time_ = navigation_time;
    return *this;
  }
  SavedTabGroupTab& SetLastSeenTime(base::Time last_seen_time) {
    last_seen_time_ = last_seen_time;
    return *this;
  }
  SavedTabGroupTab& SetRedirectURLChain(
      const std::vector<GURL>& redirect_url_chain) {
    redirect_url_chain_ = redirect_url_chain;
    return *this;
  }
  SavedTabGroupTab& SetIsPendingNtp(bool is_pending_ntp) {
    is_pending_ntp_ = is_pending_ntp;
    return *this;
  }

  // Sets the updater of the tab, and also the creator if it's the first update.
  // This method should be preferred over SetCreatedByAttribution() for local
  // changes.
  SavedTabGroupTab& SetUpdatedByAttribution(GaiaId updated_by);

  // Sets the creator of the tab. Must be called only when there is no creator
  // already set. Don't invoke this method, as it should only be invoked from
  // the sync bridge for incoming sync updates (use SetUpdatedByAttribution()).
  SavedTabGroupTab& SetCreatedByAttribution(GaiaId created_by);

  // Merges this tabs data with a specific from sync and returns the newly
  // merged specific. Side effect: Updates the values in the tab.
  void MergeRemoteTab(const SavedTabGroupTab& remote_tab);

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
  // M127. Used for metrics purposes only. Applicable for saved tab groups only.
  std::optional<std::string> creator_cache_guid_;

  // The cache guid of the device that last modified this tab group. Can be null
  // if the group was just created. Used for metrics purposes only. Applicable
  // for saved tab groups only.
  std::optional<std::string> last_updater_cache_guid_;

  // Atribution data for the shared tab. Applicable to shared tab groups only.
  SharedAttribution shared_attribution_;

  // Timestamp for when the tab was created.
  base::Time creation_time_;

  // Timestamp for when the tab was last updated.
  base::Time update_time_;

  // Timestamp for when the tab was last navigated. Currently set to the mtime
  // from the metadata from remote updates. TODO(crbug.com/420739337): After
  // per-field mtime support is added, it will be correctly set to the server
  // timestamp of when URL field is modified. Updated locally for local
  // navigations.
  base::Time navigation_time_;

  // Timestamp of the last time a user saw the contents of this tab.
  // This value may be null if the user has never focused a tab added
  // from collaboration. Windows-epoch based.
  //
  // This is used by SharedTabGroupAccountDataSyncBridge to sync "read"
  // status for shared tab updates. As such, it is not saved to disk
  // alongside saved/shared tab group data. The account data sync bridge
  // manages syncing and saving this to disk.
  std::optional<base::Time> last_seen_time_;

  // The following fields aren't synced across devices.

  // The favicon of the website this SavedTabGroupTab represents.
  std::optional<gfx::Image> favicon_;

  // Holds the current redirect chain which is used for equality check for any
  // incoming URL update. If any of the URLs in the chain matches with the new
  // URL, we don't do a navigation.
  std::vector<GURL> redirect_url_chain_;

  // Whether the current tab is a pending NTP. The pending NTPs are
  // real NTPs in the local tab model, but never synced to the server side.
  // Pending NTPs are converted to regular tabs and synced to the server
  // side when there is a navigation or tab addition either locally or from the
  // server side. A pending NTP always has the position zero and is the only
  // tab in the group.
  bool is_pending_ntp_ = false;
};

class SavedTabGroupTabBuilder {
 public:
  SavedTabGroupTabBuilder();
  ~SavedTabGroupTabBuilder();

  // Allow copy/assign.
  SavedTabGroupTabBuilder(const SavedTabGroupTabBuilder&);
  SavedTabGroupTabBuilder& operator=(const SavedTabGroupTabBuilder&);

  SavedTabGroupTabBuilder& SetPosition(size_t position);
  SavedTabGroupTabBuilder& SetRedirectURLChain(
      const std::vector<GURL>& redirect_url_chain);

  SavedTabGroupTab Build(const SavedTabGroupTab& tab) const;

  // Accessors for testing.
  size_t position() const { return position_; }

 private:
  size_t position_ = 0;
  std::vector<GURL> redirect_url_chain_;

  // Flags to indicate which properties have been set.
  bool has_position_ = false;
  bool has_redirect_url_chain_ = false;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SAVED_TAB_GROUP_TAB_H_
