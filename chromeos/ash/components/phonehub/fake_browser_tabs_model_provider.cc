// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_browser_tabs_model_provider.h"

namespace ash {
namespace phonehub {

FakeBrowserTabsModelProvider::FakeBrowserTabsModelProvider() = default;

FakeBrowserTabsModelProvider::~FakeBrowserTabsModelProvider() {}

void FakeBrowserTabsModelProvider::NotifyBrowserTabsUpdated(
    bool is_sync_enabled,
    const std::vector<BrowserTabsModel::BrowserTabMetadata>
        browser_tabs_metadata) {
  BrowserTabsModelProvider::NotifyBrowserTabsUpdated(is_sync_enabled,
                                                     browser_tabs_metadata);
}

}  // namespace phonehub
}  // namespace ash
