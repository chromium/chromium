// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_TAB_FETCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_TAB_FETCHER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/session_sync_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}
class TabAndroid;

namespace segmentation_platform {

// Interface for fetching and iterating over tabs.
class TabFetcher {
 public:
  // ID that points to a local or foreign tab.
  struct TabEntry {
    // For tabs from sync session.
    TabEntry(SessionID tab_id, const std::string& session_tag);
    // For tabs from local tab model.
    TabEntry(SessionID tab_id,
             content::WebContents* webcontents,
             TabAndroid* tab_android);

    // Session tag that identifies the session across all devices. Only
    // available for synced tabs.
    std::string session_tag;

    // A tab ID for the tab within the session.
    SessionID tab_id;

    // An ID for local tab derived from web contents or tab pointer. Do not use
    // the webcontents or tab by casting this pointer. Use the FindTab to fetch
    // the pointer, since the tab or webcontents could have been destroyed.
    raw_ptr<void, DanglingUntriaged> web_contents_data;
    raw_ptr<void> tab_android_data;
  };

  // Represents a local or foreign tab.
  struct Tab {
    // Local tab's webcontents.
    raw_ptr<content::WebContents, DanglingUntriaged> webcontents = nullptr;
    // Local tab's pointer, only available on Android.
    raw_ptr<TabAndroid> tab_android = nullptr;
    // Foreign tab's session data.
    raw_ptr<const sessions::SessionTab> session_tab = nullptr;

    // URL for the tab.
    GURL tab_url;
    // Returns the time since last modification of the `tab`. Returns
    // TimeDelta::Max() on failure.
    base::TimeDelta time_since_modified;
  };

  explicit TabFetcher(sync_sessions::SessionSyncService* session_sync_service);
  virtual ~TabFetcher() = default;

  // Appends a list of all remote tabs to `tabs`.
  bool FillAllRemoteTabs(std::vector<TabEntry>& tabs);

  // Appends a list of all remote tabs to `tabs` loaded after the given
  // timestamp.
  bool FillAllRemoteTabsAfterTime(std::vector<TabEntry>& tabs,
                                  base::Time tabs_loaded_after_timestamp);

  // Appends a list of all local tabs to `tabs`.
  bool FillAllLocalTabs(std::vector<TabEntry>& tabs);

  // Finds the tab corresponding to the `entry`.
  Tab FindTab(const TabEntry& entry);

  // Returns the count of remote tabs loaded after the given timestamp till now.
  size_t GetRemoteTabsCountAfterTime(base::Time tabs_loaded_after_timestamp);

  // Returns the modified time for the latest remote sync session if sync is
  // enabled.
  std::optional<base::Time> GetLatestRemoteSessionModifiedTime();

 protected:
  // Fills all the local tabs from the tab models in `tabs`.
  virtual bool FillAllLocalTabsFromTabModel(std::vector<TabEntry>& tabs);

  // Fills all the local tabs from the sync sessions database.
  bool FillAllLocalTabsFromSyncSessions(std::vector<TabEntry>& tabs);

  // Returns the local tab corresponding to `entry`. If the tab was closed, then
  // returns an empty Tab.
  virtual Tab FindLocalTab(const TabEntry& entry);

 private:
  const raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_TAB_FETCHER_H_
