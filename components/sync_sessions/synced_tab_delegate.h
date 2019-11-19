// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_TAB_DELEGATE_H__
#define COMPONENTS_SYNC_SESSIONS_SYNCED_TAB_DELEGATE_H__

#include <memory>
#include <string>
#include <vector>

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace sync_sessions {
class SyncSessionsClient;
}

namespace sync_sessions {

// A SyncedTabDelegate is used to insulate the sync code from depending
// directly on WebContents, NavigationController, and the extensions TabHelper.
class SyncedTabDelegate {
 public:
  virtual ~SyncedTabDelegate();

  // Methods from TabContents.
  virtual SessionID GetWindowId() const = 0;
  // Tab identifier: two tabs with the same ID (even across browser restarts)
  // will be considered identical. Tab/session restore may or may not be able
  // to restore this value, which means the opposite is not true: having
  // distinct IDs does not imply they are distinct tabs.
  virtual SessionID GetSessionId() const = 0;
  virtual bool IsBeingDestroyed() const = 0;

  // Method derived from extensions TabHelper.
  virtual std::string GetExtensionAppId() const = 0;

  // Methods from NavigationController.
  virtual bool IsInitialBlankNavigation() const = 0;
  virtual int GetCurrentEntryIndex() const = 0;
  virtual int GetEntryCount() const = 0;
  virtual GURL GetVirtualURLAtIndex(int i) const = 0;
  virtual GURL GetFaviconURLAtIndex(int i) const = 0;
  virtual ui::PageTransition GetTransitionAtIndex(int i) const = 0;
  virtual std::string GetPageLanguageAtIndex(int i) const = 0;
  virtual void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const = 0;

  // Supervised user related methods.
  virtual bool ProfileIsSupervised() const = 0;
  virtual const std::vector<
      std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const = 0;

  // Session sync related methods.
  virtual bool ShouldSync(SyncSessionsClient* sessions_client) = 0;

  // Whether this tab is a placeholder tab. On some platforms, tabs can be
  // restored without bringing all their state into memory, and are just
  // restored as a placeholder. In that case, the previous synced data from that
  // tab should be preserved.
  virtual bool IsPlaceholderTab() const = 0;

  // Task IDs represent navigations and relationships between navigations. -1
  // indicates the Task ID is unknown. A Navigation ID is a Unique ID and
  // is stored on a NavigationEntry and SerialiedNavigationEntry.
  virtual int64_t GetTaskIdForNavigationId(int nav_id) const = 0;
  virtual int64_t GetParentTaskIdForNavigationId(int nav_id) const = 0;
  virtual int64_t GetRootTaskIdForNavigationId(int nav_id) const = 0;

 protected:
  SyncedTabDelegate();
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_TAB_DELEGATE_H__
