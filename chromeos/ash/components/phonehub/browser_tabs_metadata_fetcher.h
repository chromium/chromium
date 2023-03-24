// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_H_

#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_sessions {
struct SyncedSession;
}  // namespace sync_sessions

class SyncedSessionClientAsh;

namespace ash {

struct ForeignSyncedSessionAsh;
class SyncedSessionClientAsh;

namespace phonehub {

// Used to collect the most recently visited tab metadata from a
// sync_sessions::SyncedSession, fetch their respective favicon images, and
// return a list of BrowserTabMetadata to its caller.
class BrowserTabsMetadataFetcher {
 public:
  virtual ~BrowserTabsMetadataFetcher() = default;

  BrowserTabsMetadataFetcher(const BrowserTabsMetadataFetcher&) = delete;
  BrowserTabsMetadataFetcher& operator=(const BrowserTabsMetadataFetcher&) =
      delete;

  using BrowserTabsMetadataResponse =
      absl::optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>;

  // Fetches the metadata of the most recently visited tabs. Only one fetch is
  // possible at a given time, and if a new fetch is started when another is
  // already in progress, the previous fetch will be passed a absl::nullopt.
  virtual void Fetch(
      const sync_sessions::SyncedSession* session,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) = 0;
  virtual void FetchForeignSyncedPhoneSessionMetadata(
      const ForeignSyncedSessionAsh& session,
      SyncedSessionClientAsh* synced_session_client_ash,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) = 0;

 protected:
  BrowserTabsMetadataFetcher() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_H_
