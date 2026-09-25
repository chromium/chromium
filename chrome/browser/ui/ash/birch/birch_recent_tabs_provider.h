// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

// Manages fetching foreign session tabs for the birch feature. Fetched tabs
// are sent to the `BirchModel` to be stored.
class ASH_EXPORT BirchRecentTabsProvider : public BirchDataProvider {
 public:
  // `sync_service` and `session_sync_service` belong to `profile`.
  // `sync_service` may be null -- SyncServiceFactory hands out null whenever
  // --disable-sync is passed, and in tests that install no testing factory for
  // it. `session_sync_service` must be non-null.
  BirchRecentTabsProvider(
      Profile* profile,
      syncer::SyncService* sync_service,
      sync_sessions::SessionSyncService* session_sync_service);
  BirchRecentTabsProvider(const BirchRecentTabsProvider&) = delete;
  BirchRecentTabsProvider& operator=(const BirchRecentTabsProvider&) = delete;
  ~BirchRecentTabsProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // Once subscribed, this is called when the session sync service notifies of
  // foreign sessions changed.
  void OnForeignSessionsChanged();

 private:
  raw_ptr<Profile> profile_ = nullptr;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ref<sync_sessions::SessionSyncService> session_sync_service_;

  base::CallbackListSubscription foreign_sessions_subscription_;

  base::WeakPtrFactory<BirchRecentTabsProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_
