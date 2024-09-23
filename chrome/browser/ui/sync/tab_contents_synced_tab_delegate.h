// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace content {
class WebContents;
}

namespace tasks {
class TaskTabHelper;
}

// Partial implementation of SyncedTabDelegate for the cases where the tab has
// (either initially or late) a WebContents.
class TabContentsSyncedTabDelegate : public sync_sessions::SyncedTabDelegate {
 public:
  TabContentsSyncedTabDelegate() = default;

  TabContentsSyncedTabDelegate(const TabContentsSyncedTabDelegate&) = delete;
  TabContentsSyncedTabDelegate& operator=(const TabContentsSyncedTabDelegate&) =
      delete;

  ~TabContentsSyncedTabDelegate() override = default;

  // Resets the cached last_active_time value, allowing the next call to
  // GetLastActiveTime() to return the actual value.
  void ResetCachedLastActiveTime();

  // SyncedTabDelegate:
  base::Time GetLastActiveTime() override;
  bool IsBeingDestroyed() const override;
  std::string GetExtensionAppId() const override;
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  bool ProfileHasChildAccount() const override;
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool ShouldSync(sync_sessions::SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;

 protected:
  const content::WebContents* web_contents() const;
  content::WebContents* web_contents();
  void SetWebContents(content::WebContents* web_contents);

 private:
  const tasks::TaskTabHelper* task_tab_helper() const;

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;

  // Cached value of last_active_time_ticks and last_active_time, sometimes
  // returned instead of the last_active_time from the WebContents.
  std::optional<std::pair<base::TimeTicks, base::Time>>
      cached_last_active_time_;
};

#endif  // CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
