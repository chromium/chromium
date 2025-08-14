// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "components/sync/service/sync_service_observer.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

// Watches a profile for changes in the sign-in state - see
// HistoryUtil::GetSignInState().
class HistorySignInStateWatcher : public syncer::SyncServiceObserver {
 public:
  HistorySignInStateWatcher(Profile* profile, base::RepeatingClosure callback);

  HistorySignInStateWatcher(const HistorySignInStateWatcher&) = delete;
  HistorySignInStateWatcher& operator=(const HistorySignInStateWatcher&) =
      delete;

  ~HistorySignInStateWatcher() override;

  HistorySignInState GetSignInState() const;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  // Runs |callback_| when the sign-in state changes.
  void RunCallback();

  // Weak reference to the profile this class observes.
  const raw_ptr<Profile> profile_;

  // Called when the history sync state changes.
  base::RepeatingClosure callback_;

  HistorySignInState cached_signin_state_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_
