// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"

namespace ash {
namespace phonehub {

BrowserTabsModelProvider::BrowserTabsModelProvider() = default;

BrowserTabsModelProvider::~BrowserTabsModelProvider() = default;

void BrowserTabsModelProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowserTabsModelProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowserTabsModelProvider::NotifyBrowserTabsUpdated(
    bool is_sync_enabled,
    const std::vector<BrowserTabsModel::BrowserTabMetadata>
        browser_tabs_metadata) {
  for (auto& observer : observer_list_) {
    observer.OnBrowserTabsUpdated(is_sync_enabled, browser_tabs_metadata);
  }
}

}  // namespace phonehub
}  // namespace ash
