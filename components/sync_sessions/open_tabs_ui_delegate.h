// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_H_
#define COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {

class OpenTabsUIDelegate {
 public:
  // Controls which foreign sessions are returned by `GetAllForeignSessions()`.
  enum class ForeignSessionFilter {
    // Includes only visible foreign sessions that have at least one window
    // with a syncable tab URL (`SyncSessionsClient::ShouldSyncURL()`, which
    // excludes `chrome://` and New Tab Page URLs), and excludes sessions hidden
    // via `SessionSyncPrefs` when `kDeviceTabVisibilitySettings` is enabled.
    kPresentable,
    // Includes all tracked foreign sessions with a non-empty session name
    // (maps to `SyncedSessionTracker::NAMED`), including sessions with zero
    // open tabs, sessions with only non-syncable tabs (e.g., `chrome://` or
    // New Tab Page URLs), and sessions hidden via `SessionSyncPrefs`. Sessions
    // with an empty session name are excluded because they represent
    // incomplete/orphan entries that have only received tab specifics without
    // a `SessionHeader` entity.
    kAll,
  };

  // Builds a list of foreign sessions matching `filter`, ordered from most
  // recent to least recent. Caller does NOT own `SyncedSession` objects.
  // Returns true if foreign sessions were found, false otherwise.
  virtual bool GetAllForeignSessions(
      std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>>* sessions,
      ForeignSessionFilter filter) = 0;

  // Returns the timestamp of each foreign session, keyed by its session tag.
  virtual base::flat_map<std::string, base::Time>
  GetAllForeignSessionLastModifiedTimes() const = 0;

  // Looks up the foreign tab identified by |tab_id| and belonging to foreign
  // session |tag|. Caller does NOT own the SessionTab object.
  // Returns true if the foreign session and tab were found, false otherwise.
  virtual bool GetForeignTab(const std::string& tag,
                             SessionID tab_id,
                             const sessions::SessionTab** tab) = 0;

  // Delete a foreign session and all its sync data.
  virtual void DeleteForeignSession(const std::string& tag) = 0;

  // Returns the foreign session windows associated with session tag `tag`.
  // Returns an empty vector if no windows for `tag`. Caller does NOT own
  // SessionWindow objects.
  virtual std::vector<const sessions::SessionWindow*> GetForeignSession(
      const std::string& tag) = 0;

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
