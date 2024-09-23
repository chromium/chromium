// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/browser_tabs_metadata_fetcher.h"

namespace ash {

namespace phonehub {

class FakeBrowserTabsMetadataFetcher : public BrowserTabsMetadataFetcher {
 public:
  FakeBrowserTabsMetadataFetcher();
  ~FakeBrowserTabsMetadataFetcher() override;

  // BrowserTabsMetadataFetcher:
  void Fetch(
      const sync_sessions::SyncedSession* session,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) override;
  void RespondToCurrentFetchAttempt(
      const BrowserTabsMetadataResponse& response);

  bool DoesPendingCallbackExist();

  const sync_sessions::SyncedSession* GetSession() const;

 private:
  raw_ptr<const sync_sessions::SyncedSession, DanglingUntriaged> session_;
  base::OnceCallback<void(BrowserTabsMetadataResponse)> callback_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_
