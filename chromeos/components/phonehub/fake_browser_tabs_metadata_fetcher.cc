// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_browser_tabs_metadata_fetcher.h"

namespace chromeos {
namespace phonehub {

FakeBrowserTabsMetadataFetcher::FakeBrowserTabsMetadataFetcher() = default;

FakeBrowserTabsMetadataFetcher::~FakeBrowserTabsMetadataFetcher() = default;

void FakeBrowserTabsMetadataFetcher::Fetch(
    const sync_sessions::SyncedSession* session,
    base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) {
  callback_ = std::move(callback);
}

void FakeBrowserTabsMetadataFetcher::RespondToCurrentFetchAttempt(
    const BrowserTabsMetadataResponse& response) {
  std::move(callback_).Run(response);
}

}  // namespace phonehub
}  // namespace chromeos
