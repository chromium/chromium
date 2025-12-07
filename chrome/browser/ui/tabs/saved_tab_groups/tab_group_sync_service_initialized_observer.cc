// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"

#include "base/run_loop.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {

TabGroupSyncServiceInitializedObserver::TabGroupSyncServiceInitializedObserver(
    TabGroupSyncService* service)
    : service_(service) {
  CHECK(service);
  observer_.Observe(service);
}
TabGroupSyncServiceInitializedObserver::
    ~TabGroupSyncServiceInitializedObserver() = default;

void TabGroupSyncServiceInitializedObserver::Wait() {
  if (loaded_) {
    return;
  }
  run_loop_.Run();
}

void TabGroupSyncServiceInitializedObserver::OnInitialized() {
  loaded_ = true;
  run_loop_.Quit();
}

}  // namespace tab_groups
