// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_H_
#define COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "url/gurl.h"

namespace sync_sessions {

class OpenTabsUIDelegate {
 public:
  // Builds a list of all foreign sessions, ordered from most recent to least
  // recent. Caller does NOT own SyncedSession objects.
  // Returns true if foreign sessions were found, false otherwise.
  virtual bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) = 0;

  // Looks up the foreign tab identified by |tab_id| and belonging to foreign
  // session |tag|. Caller does NOT own the SessionTab object.
  // Returns true if the foreign session and tab were found, false otherwise.
  virtual bool GetForeignTab(const std::string& tag,
                             SessionID tab_id,
                             const sessions::SessionTab** tab) = 0;

  // Delete a foreign session and all its sync data.
  virtual void DeleteForeignSession(const std::string& tag) = 0;

  // Loads all windows for foreign session with session tag |tag|. Caller does
  // NOT own SessionWindow objects.
  // Returns true if the foreign session was found, false otherwise.
  virtual bool GetForeignSession(
      const std::string& tag,
      std::vector<const sessions::SessionWindow*>* windows) = 0;

  // Loads all tabs for a foreign session, ignoring window grouping, and
  // ordering by recency (most recent to least recent). Will automatically
  // prune those tabs that are not syncable or are the NTP. Caller does NOT own
  // SessionTab objects.
  // Returns true if the foreign session was found, false otherwise.
  virtual bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) = 0;

  // Sets |*local| to point to the sessions sync representation of the
  // local machine.
  virtual bool GetLocalSession(const SyncedSession** local) = 0;

 protected:
  virtual ~OpenTabsUIDelegate();
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_H_
