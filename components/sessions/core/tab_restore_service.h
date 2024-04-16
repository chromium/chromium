// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/token.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace sessions {

class LiveTab;
class TabRestoreServiceObserver;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs and windows. When a tab is closed TabRestoreService::CreateHistoricalTab
// is invoked and a Tab is created to represent the tab. Similarly, when a
// browser is closed, BrowserClosing is invoked and a Window is created to
// represent the window. Similarly, when a group is closed,
// CreateHistoricalGroup is invoked and a Group is created to represent the
// group.
//
// To restore a tab/window from the TabRestoreService invoke RestoreEntryById
// or RestoreMostRecentEntry.
//
// To listen for changes to the set of entries managed by the TabRestoreService
// add an observer.
class SESSIONS_EXPORT TabRestoreService : public KeyedService {
 public:
  typedef std::list<std::unique_ptr<tab_restore::Entry>> Entries;
  typedef base::RepeatingCallback<bool(const SerializedNavigationEntry& entry)>
      DeletionPredicate;

  ~TabRestoreService() override = default;

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  virtual void AddObserver(TabRestoreServiceObserver* observer) = 0;
  virtual void RemoveObserver(TabRestoreServiceObserver* observer) = 0;

  // Creates a Tab to represent |live_tab| and notifies observers the list of
  // entries has changed. If successful, returns the unique SessionID associated
  // with the Tab.
  virtual std::optional<SessionID> CreateHistoricalTab(LiveTab* live_tab,
                                                       int index) = 0;

  // Creates a Group to represent a tab group with ID |id|, containing group
  // metadata and all tabs within the group.
  virtual void CreateHistoricalGroup(LiveTabContext* context,
                                     const tab_groups::TabGroupId& id) = 0;

  // Invoked when the group is done closing.
  virtual void GroupClosed(const tab_groups::TabGroupId& group) = 0;

  // Invoked when the group is did not fully close, e.g. because one of the tabs
  // had a beforeunload handler.
  virtual void GroupCloseStopped(const tab_groups::TabGroupId& group) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when a browser is closing. If |context| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  virtual void BrowserClosing(LiveTabContext* context) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when the browser is done closing.
  virtual void BrowserClosed(LiveTabContext* context) = 0;

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  virtual void ClearEntries() = 0;

  // Removes all SerializedNavigationEntries matching |predicate| and notifies
  // observers the list of tabs has changed.
  virtual void DeleteNavigationEntries(const DeletionPredicate& predicate) = 0;

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  virtual const Entries& entries() const = 0;

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |context|. Returns the LiveTab instances of the restored tab(s).
  virtual std::vector<LiveTab*> RestoreMostRecentEntry(
      LiveTabContext* context) = 0;

  // Removes the Entry with id |id|. The entry could be a Tab, Group, or Window.
  virtual void RemoveEntryById(SessionID id) = 0;

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |context| is NULL, this creates a new window for the
  // entry. |disposition| is respected, but the attributes (tabstrip index,
  // browser window) of the tab when it was closed will be respected if
  // disposition is UNKNOWN. Returns the LiveTab instances of the restored
  // tab(s).
  virtual std::vector<LiveTab*> RestoreEntryById(
      LiveTabContext* context,
      SessionID id,
      WindowOpenDisposition disposition) = 0;

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  virtual void LoadTabsFromLastSession() = 0;

  // Returns true if the tab entries have been loaded.
  virtual bool IsLoaded() const = 0;

  // Deletes the last session.
  virtual void DeleteLastSession() = 0;

  // Returns true if we're in the process of restoring some entries.
  virtual bool IsRestoring() const = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
