// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"

#include "base/check.h"
#include "base/test/run_until.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {

TabGroupSyncServiceInitializedObserver::TabGroupSyncServiceInitializedObserver(
    TabGroupSyncService* service) {
  CHECK(service);
  // `TabGroupSyncService::AddObserver()` synchronously invokes
  // `OnInitialized()` if the service is already initialized, setting `loaded_`
  // to true immediately.
  observer_.Observe(service);
}
TabGroupSyncServiceInitializedObserver::
    ~TabGroupSyncServiceInitializedObserver() = default;

bool TabGroupSyncServiceInitializedObserver::Wait() {
  return loaded_ || base::test::RunUntil([this]() { return loaded_; });
}

void TabGroupSyncServiceInitializedObserver::OnInitialized() {
  loaded_ = true;
}

}  // namespace tab_groups
