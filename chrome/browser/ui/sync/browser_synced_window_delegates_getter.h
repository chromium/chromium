// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

class Profile;

namespace browser_sync {

// This class defines how to access SyncedWindowDelegates on desktop.
class BrowserSyncedWindowDelegatesGetter
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  explicit BrowserSyncedWindowDelegatesGetter(Profile* profile);

  BrowserSyncedWindowDelegatesGetter(
      const BrowserSyncedWindowDelegatesGetter&) = delete;
  BrowserSyncedWindowDelegatesGetter& operator=(
      const BrowserSyncedWindowDelegatesGetter&) = delete;

  ~BrowserSyncedWindowDelegatesGetter() override;

  // SyncedWindowDelegatesGetter implementation
  SyncedWindowDelegateMap GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(SessionID id) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_
