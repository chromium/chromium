// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_

#include "chromeos/components/phonehub/browser_tabs_metadata_fetcher.h"

namespace chromeos {
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

 private:
  base::OnceCallback<void(BrowserTabsMetadataResponse)> callback_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_METADATA_FETCHER_H_
